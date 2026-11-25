// gramd: Gram's headless part. BB10 starts it at boot and after install (invoke target org.bq.gram.service) and
// keeps it running without a window (_sys_headless_nostop). It owns the Telegram library, so messages arrive and
// Hub notifications are posted while the app itself is closed.
//
// The app's window connects to data/gramd.sock (one JSON object per line, both ways):
//   window -> service: TDLib requests (passed on unchanged), or {"@ui": "state", "active": bool, "chat": id}
//                      / {"@ui": "params"} (API credentials were just saved) / {"@ui": "lock"} (data/lock.json changed) / {"@ui": "heap"} (log heap use)
// App lock with "hide message text" on: Hub entries say only "Gram" and how many messages arrived.
// Calls: an incoming call starts the window if it is closed (it rings and shows the call screen); the latest
// updateCall of each call going on is sent after the window's getCurrentState answer.
//   service -> window: everything TDLib sends (the window asks for getCurrentState first).
// Notifications come from TDLib's notification groups (they follow mute settings and disappear when the chat is
// read anywhere); one Hub entry per chat, opening it invokes the window's target org.bq.gram.open with the chat id.
#include <bb/Application>
#include <bb/platform/Notification>
#include <bb/system/InvokeRequest>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeTargetReply>
#include <bb/data/JsonDataAccess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSet>
#include <QThread>
#include <QDateTime>
#include <QTimer>
#include <bb/ApplicationInfo>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <malloc.h>

extern "C" {
int tg_create_client_id(void);
void tg_send(int client_id, const char *request);
const char *tg_receive(double timeout);
const char *tg_execute(const char *request);
}

typedef QVariantMap M;
static inline qint64 I(const QVariant &v) { return v.toLongLong(); }
static inline QString T(const M &m) { return m.value("@type").toString(); }
static inline M sub(const M &m, const char *k) { return m.value(k).toMap(); }

static QVariant parse(const QByteArray &b) {
    bb::data::JsonDataAccess jda;
    QVariant v = jda.loadFromBuffer(b);
    return jda.hasError() ? QVariant() : v;
}
static QByteArray jstr(const QString &s) {                // JSON string literal
    QByteArray o = "\"";
    QByteArray u = s.toUtf8();
    for (int i = 0; i < u.size(); i++) {
        char c = u.at(i);
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if ((unsigned char)c < 0x20) { char b[8]; qsnprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += c;
    }
    return o + "\"";
}

static void logHeap(const char *when) {         // heap in use; the launcher allows 30 MB for this process
    struct mallinfo mi = mallinfo();
    fprintf(stderr, "gramd: heap %s: %d KB in use, %d KB arena\n", when, mi.uordblks / 1024, mi.arena / 1024);
}

class Receiver : public QThread {
    Q_OBJECT
public:
    volatile bool stop;
    Receiver() : stop(false) {}
Q_SIGNALS:
    void got(const QByteArray &line);
protected:
    void run() { while (!stop) { const char *r = tg_receive(1.0); if (r) Q_EMIT got(QByteArray(r)); } }
};

class Service : public QObject {
    Q_OBJECT
public:
    Service(const QString &dataDir) : m_data(dataDir), m_id(0), m_ui(0), m_uiActive(false), m_uiChat(0), m_paramsSent(false), m_waitingParams(false), m_myId(0), m_hide(false), m_hold(true) {
        tg_execute("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":1}");
        m_rx = new Receiver;
        connect(m_rx, SIGNAL(got(QByteArray)), this, SLOT(onTd(QByteArray)), Qt::QueuedConnection);
        newClient();
        m_rx->start();
        QString path = m_data + "/gramd.sock";
        QLocalServer::removeServer(path);
        m_server = new QLocalServer(this);
        if (!m_server->listen(path)) fprintf(stderr, "gramd: cannot listen on %s\n", qPrintable(path));
        else QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::WriteGroup);
        connect(m_server, SIGNAL(newConnection()), this, SLOT(onConnection()));
        QTimer *t = new QTimer(this);              // credentials may be saved later (from the sign-in screen)
        connect(t, SIGNAL(timeout()), this, SLOT(retryParams()));
        t->start(3000);
        QTimer *h = new QTimer(this);              // heap use in the log every 10 minutes
        connect(h, SIGNAL(timeout()), this, SLOT(heapTick()));
        h->start(600000);
    }

private Q_SLOTS:
    void retryParams() { if (m_waitingParams) sendParams(); }
    void heapTick() { logHeap("now"); }
    // TDLib aborts (AttachMenuManager CHECK is_inited_) if getCurrentState arrives after a saved login has loaded but
    // before every manager has started, which is exactly when a window connects right after the service starts.
    // So window requests wait until TDLib has reported its first authorization state (+1.5 s once it is Ready).
    void releaseUi() {
        if (!m_hold) return;
        m_hold = false;
        fprintf(stderr, "gramd: passing %d held window requests\n", m_held.size());
        Q_FOREACH (const QByteArray &l, m_held) tg_send(m_id, l.constData());
        m_held.clear();
    }
    void onConnection() {
        QLocalSocket *s = m_server->nextPendingConnection();
        if (!s) return;
        if (m_ui) { dropUi(); }
        m_ui = s;
        m_buf.clear();
        connect(s, SIGNAL(readyRead()), this, SLOT(onUiData()));
        connect(s, SIGNAL(disconnected()), this, SLOT(onUiGone()));
        fprintf(stderr, "gramd: window connected\n");
    }
    void onUiGone() {
        if (sender() != m_ui) return;
        fprintf(stderr, "gramd: window gone\n");
        dropUi();
    }
    void onUiData() {
        if (!m_ui) return;
        m_buf += m_ui->readAll();
        int nl;
        while ((nl = m_buf.indexOf('\n')) >= 0) {
            QByteArray line = m_buf.left(nl);
            m_buf.remove(0, nl + 1);
            if (line.isEmpty()) continue;
            if (line.startsWith("{\"@ui\"")) { control(parse(line).toMap()); continue; }
            // chats the window opens: closed again if the window goes away without closing them
            if (line.contains("\"@type\":\"openChat\"") || line.contains("\"@type\":\"closeChat\"")) {
                M r = parse(line).toMap();
                if (T(r) == "openChat") m_uiOpen.insert(I(r.value("chat_id"))); else m_uiOpen.remove(I(r.value("chat_id")));
            }
            if (m_hold) m_held.append(line); else tg_send(m_id, line.constData());
        }
    }
    void onTd(const QByteArray &line) {
        if (line.contains("\"@extra\":\"params\"") && line.startsWith("{\"@type\":\"error\"")) {
            fprintf(stderr, "gramd: Telegram refused the API ID/hash\n");
            m_badParams = QFileInfo(m_data + "/api.json").lastModified();
            m_paramsSent = false;
        }
        if (m_ui && !line.contains("\"@extra\":\"svc")) {
            m_ui->write(line); m_ui->write("\n");
            if (line.startsWith("{\"@type\":\"updates\"")) {        // the window's getCurrentState: add the calls going on
                Q_FOREACH (const QByteArray &c, m_callLines) { m_ui->write(c); m_ui->write("\n"); }
            }
        }
        // only what the service itself needs is parsed
        if (!line.startsWith("{\"@type\":\"update")) return;
        static const char *wanted[] = { "updateAuthorizationState", "updateNotificationGroup", "updateActiveNotifications", "updateNewChat\"",
                                        "updateChatTitle", "updateUser\"", "updateOption\"", "updateCall\"", 0 };
        bool want = false;
        for (int i = 0; wanted[i] && !want; i++) want = line.startsWith(QByteArray("{\"@type\":\"") + wanted[i]);
        if (!want) return;
        M u = parse(line).toMap();
        QString t = T(u);
        if (t == "updateAuthorizationState") {
            QString st = T(sub(u, "authorization_state"));
            m_waitingParams = st == "authorizationStateWaitTdlibParameters";
            if (m_hold) {
                if (st == "authorizationStateReady") QTimer::singleShot(1500, this, SLOT(releaseUi()));
                else if (st != "authorizationStateClosing" && st != "authorizationStateClosed") releaseUi();
            }
            if (m_waitingParams) { m_paramsSent = false; sendParams(); }
            else if (st == "authorizationStateReady") {
                logHeap("at ready");
                tg_send(m_id, "{\"@type\":\"setOption\",\"name\":\"notification_group_count_max\",\"value\":{\"@type\":\"optionValueInteger\",\"value\":25},\"@extra\":\"svc\"}");
                tg_send(m_id, "{\"@type\":\"setOption\",\"name\":\"notification_group_size_max\",\"value\":{\"@type\":\"optionValueInteger\",\"value\":10},\"@extra\":\"svc\"}");
            } else if (st == "authorizationStateClosed") {
                bb::platform::Notification::deleteAllFromInbox();
                m_titles.clear(); m_users.clear(); m_groups.clear();
                newClient();
            }
        } else if (t == "updateOption") {
            if (u.value("name").toString() == "my_id") m_myId = I(sub(u, "value").value("value"));
        } else if (t == "updateNewChat") {
            M c = sub(u, "chat"); m_titles[I(c.value("id"))] = c.value("title").toString();
        } else if (t == "updateChatTitle") {
            m_titles[I(u.value("chat_id"))] = u.value("title").toString();
        } else if (t == "updateUser") {
            M x = sub(u, "user"); m_users[I(x.value("id"))] = (x.value("first_name").toString() + " " + x.value("last_name").toString()).trimmed();
        } else if (t == "updateCall") {
            M call = sub(u, "call"); qint64 id = I(call.value("id")); QString st = T(sub(call, "state"));
            if (st == "callStateDiscarded" || st == "callStateError") {
                m_callLines.remove(id); m_callsSeen.remove(id);
                bb::platform::Notification::deleteFromInbox("call" + QString::number(id));
                return;
            }
            m_callLines[id] = line;
            if (!call.value("is_outgoing").toBool() && st == "callStatePending" && !m_callsSeen.contains(id)) {
                m_callsSeen.insert(id);
                qint64 uid = I(call.value("user_id"));
                QString who = m_users.value(uid);
                fprintf(stderr, "gramd: incoming call, window %s\n", m_ui ? "connected" : "closed");
                bb::platform::Notification n("call" + QString::number(id));    // Hub entry while it rings; the window rings itself
                n.setTitle(hidden() ? QString("Gram") : who.isEmpty() ? QString("Incoming call") : who);
                n.setBody("Incoming call");
                bb::system::InvokeRequest req;
                req.setTarget("org.bq.gram.open"); req.setAction("bb.action.OPEN");
                req.setMimeType("application/vnd.bq.gram.chat"); req.setData("call");
                n.setInvokeRequest(req);
                n.clearEffects();
                n.notify();
                if (!m_ui) {                                                     // app closed: start it, it rings and shows the call
                    bb::system::InvokeManager im;
                    bb::system::InvokeTargetReply *r = im.invoke(req);
                    if (r) r->deleteLater();
                }
            }
        } else if (t == "updateActiveNotifications") {
            QVariantList gs = u.value("groups").toList();
            for (int i = 0; i < gs.size(); i++) {
                M g = gs.at(i).toMap();
                Group &grp = m_groups[g.value("id").toInt()];
                grp.chatId = I(g.value("chat_id"));
                QVariantList ns = g.value("notifications").toList();
                for (int k = 0; k < ns.size(); k++) grp.items[ns.at(k).toMap().value("id").toInt()] = ns.at(k).toMap();
                show(g.value("id").toInt(), false);
            }
        } else if (t == "updateNotificationGroup") {
            int gid = u.value("notification_group_id").toInt();
            Group &grp = m_groups[gid];
            grp.chatId = I(u.value("chat_id"));
            QVariantList rm = u.value("removed_notification_ids").toList(), add = u.value("added_notifications").toList();
            for (int i = 0; i < rm.size(); i++) grp.items.remove(rm.at(i).toInt());
            bool fresh = false;
            for (int i = 0; i < add.size(); i++) {
                M n = add.at(i).toMap();
                grp.items[n.value("id").toInt()] = n;
                if (!n.value("is_silent").toBool()) fresh = true;
            }
            show(gid, fresh);
        }
    }

private:
    struct Group { qint64 chatId; QMap<int, M> items; Group() : chatId(0) {} };

    void newClient() { m_hold = true; m_id = tg_create_client_id(); tg_send(m_id, "{\"@type\":\"getOption\",\"name\":\"version\",\"@extra\":\"svc\"}"); }
    void dropUi() {
        QLocalSocket *s = m_ui;
        m_ui = 0;
        Q_FOREACH (qint64 c, m_uiOpen) tg_send(m_id, QByteArray("{\"@type\":\"closeChat\",\"chat_id\":" + QByteArray::number(c) + ",\"@extra\":\"svc\"}").constData());
        m_uiOpen.clear();
        m_held.clear();
        m_uiActive = false; m_uiChat = 0;
        if (s) { s->disconnect(this); s->abort(); s->deleteLater(); }
    }
    void control(const M &c) {
        QString what = c.value("@ui").toString();
        if (what == "state") {
            m_uiActive = c.value("active").toBool(); m_uiChat = I(c.value("chat"));
            if (m_uiActive) {                       // looking at the app: the Hub entries of open chats go away
                QHash<int, Group>::iterator i;
                for (i = m_groups.begin(); i != m_groups.end(); ++i) if (i.value().chatId == m_uiChat) bb::platform::Notification::deleteFromInbox(key(m_uiChat));
            }
        } else if (what == "params") { m_paramsSent = false; sendParams(); }
        else if (what == "lock") { m_lockStamp = QDateTime(); hidden(); }
        else if (what == "heap") logHeap(c.value("when").toString().toLatin1().constData());   // test aid (debug heapTest)
    }
    void sendParams() {
        if (m_paramsSent) return;
        QFile f(m_data + "/api.json");
        if (m_badParams.isValid() && QFileInfo(f).lastModified() == m_badParams) return;   // same file that failed
        if (!f.open(QIODevice::ReadOnly)) return;
        M c = parse(f.readAll()).toMap();
        if (c.value("id").toInt() <= 0 || c.value("hash").toString().isEmpty()) return;
        QByteArray p = "{\"@type\":\"setTdlibParameters\",\"database_directory\":" + jstr(m_data + "/td") + ",\"files_directory\":" + jstr(m_data + "/files")
            + ",\"use_file_database\":true,\"use_chat_info_database\":true,\"use_message_database\":true,\"use_secret_chats\":true,\"api_id\":"
            + QByteArray::number(c.value("id").toInt()) + ",\"api_hash\":" + jstr(c.value("hash").toString())
            + ",\"system_language_code\":\"en\",\"device_model\":\"BlackBerry Passport\",\"system_version\":\"BlackBerry 10.3\",\"application_version\":\"" + bb::ApplicationInfo().version().toUtf8() + "\",\"@extra\":\"params\"}";
        tg_send(m_id, p.constData());
        m_paramsSent = true;
    }
    // app lock: notifications without names or text? (data/lock.json, re-read when it changes)
    bool hidden() {
        QFileInfo fi(m_data + "/lock.json");
        QDateTime stamp = fi.exists() ? fi.lastModified() : QDateTime::fromTime_t(1);
        if (stamp != m_lockStamp) {
            m_lockStamp = stamp;
            QFile f(fi.filePath());
            M l = f.open(QIODevice::ReadOnly) ? parse(f.readAll()).toMap() : M();
            m_hide = l.value("on").toBool() && (!l.contains("hideText") || l.value("hideText").toBool());
            fprintf(stderr, "gramd: notification text %s\n", m_hide ? "hidden (app lock)" : "shown");
        }
        return m_hide;
    }
    static QString key(qint64 chatId) { return "chat" + QString::number(chatId); }
    QString summary(const M &c) const {
        QString t = T(c).mid(7);
        QString cap = sub(c, "caption").value("text").toString();
        if (t == "Text") return sub(c, "text").value("text").toString();
        if (t == "Photo") return cap.isEmpty() ? QString("Photo") : "Photo, " + cap;
        if (t == "Video") return cap.isEmpty() ? QString("Video") : "Video, " + cap;
        if (t == "Animation") return "GIF";
        if (t == "Sticker") return sub(c, "sticker").value("emoji").toString() + " Sticker";
        if (t == "VoiceNote") return "Voice message";
        if (t == "VideoNote") return "Video message";
        if (t == "Audio") return "Audio";
        if (t == "Document") return sub(c, "document").value("file_name").toString();
        if (t == "Poll") return "Poll";
        if (t == "Location" || t == "Venue") return "Location";
        if (t == "Contact") return "Contact";
        return "New message";
    }
    void show(int gid, bool alert) {
        Group grp = m_groups.value(gid);
        if (!grp.chatId) return;
        QString k = key(grp.chatId);
        if (grp.items.isEmpty()) { bb::platform::Notification::deleteFromInbox(k); m_groups.remove(gid); return; }
        if (m_uiActive && grp.chatId == m_uiChat) return;          // the person is reading this chat
        M last = grp.items.value(grp.items.keys().last()), type = sub(last, "type");
        QString body;
        if (T(type) == "notificationTypeNewMessage") {
            M msg = sub(type, "message"), s = sub(msg, "sender_id");
            QString text = type.value("show_preview").toBool() ? summary(sub(msg, "content")) : QString("New message");
            qint64 uid = I(s.value("user_id"));
            bool group = uid && uid != grp.chatId;
            body = group && m_users.contains(uid) ? m_users.value(uid).section(' ', 0, 0) + ": " + text : text;
        } else if (T(type) == "notificationTypeNewPushMessage") {
            body = type.value("sender_name").toString() + ": New message";
        } else body = "New message";
        if (grp.items.size() > 1) body = QString("(%1) ").arg(grp.items.size()) + body;
        QString title = m_titles.value(grp.chatId);
        if (grp.chatId == m_myId) title = "Saved Messages";
        if (hidden()) { title = "Gram"; body = grp.items.size() > 1 ? QString("%1 new messages").arg(grp.items.size()) : QString("New message"); }
        bb::platform::Notification n(k);
        n.setTitle(title.isEmpty() ? QString("Gram") : title);
        n.setBody(body.left(300));
        n.setTimestamp(QDateTime::fromTime_t(last.value("date").toUInt()));
        bb::system::InvokeRequest req;
        req.setTarget("org.bq.gram.open");
        req.setAction("bb.action.OPEN");
        req.setMimeType("application/vnd.bq.gram.chat");
        req.setData(QByteArray::number(grp.chatId));
        n.setInvokeRequest(req);
        if (!alert) n.clearEffects();
        n.notify();
    }

    QString m_data;
    int m_id;
    Receiver *m_rx;
    QLocalServer *m_server;
    QLocalSocket *m_ui;
    QByteArray m_buf;
    bool m_uiActive;
    qint64 m_uiChat;
    QSet<qint64> m_uiOpen;
    bool m_paramsSent;
    bool m_waitingParams;
    QDateTime m_badParams;
    QMap<qint64, QByteArray> m_callLines;          // latest updateCall of each call going on (for a window that connects)
    QSet<qint64> m_callsSeen;
    qint64 m_myId;
    QHash<qint64, QString> m_titles, m_users;
    QHash<int, Group> m_groups;
    QDateTime m_lockStamp;
    bool m_hide;
    bool m_hold;                                    // window requests wait until TDLib has settled (see releaseUi)
    QList<QByteArray> m_held;
};

int main(int argc, char **argv) {
    QString root = QDir::currentPath();
    QByteArray log = (root + "/data/gramd.log").toUtf8();
    if (freopen(log.constData(), "w", stderr)) setvbuf(stderr, NULL, _IOLBF, 0);
    bb::Application app(argc, argv);
    fprintf(stderr, "gramd: start pid %d\n", (int)getpid());
    struct rlimit rl;
    if (getrlimit(RLIMIT_DATA, &rl) == 0) {
        fprintf(stderr, "gramd: data limit %lld / %lld\n", (long long)rl.rlim_cur, (long long)rl.rlim_max);
        if (rl.rlim_cur != rl.rlim_max) { rl.rlim_cur = rl.rlim_max; if (setrlimit(RLIMIT_DATA, &rl) == 0) fprintf(stderr, "gramd: raised to %lld\n", (long long)rl.rlim_cur); }
    }
    Service s(root + "/data");
    return bb::Application::exec();
}

#include "gramd.moc"
