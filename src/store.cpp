#include "store.h"
#include "calls.h"
#include "tdclient.h"
#include "media.h"
#include <bb/platform/Notification>
#include <bb/system/Clipboard>
#include <bb/ApplicationInfo>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>
#include <bb/system/InvokeTargetReply>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QTimer>
#include <QDateTime>

typedef QVariantMap M;
static inline qint64 I(const QVariant &v) { return v.toLongLong(); }
static inline QString T(const M &m) { return m.value("@type").toString(); }
static inline M sub(const M &m, const char *k) { return m.value(k).toMap(); }
static QVariantList path1(int i) { QVariantList l; l << i; return l; }
static M req(const char *type) { M m; m["@type"] = QString::fromLatin1(type); return m; }

// ---------------------------------------------------------------- chat list model
ChatListModel::ChatListModel(Store *store, const QString &listType) : bb::cascades::DataModel(store), m_store(store), m_listType(listType) {}
int ChatListModel::childCount(const QVariantList &p) { return p.isEmpty() ? m_rows.size() : 0; }
bool ChatListModel::hasChildren(const QVariantList &p) { return p.isEmpty() && !m_rows.isEmpty(); }
QString ChatListModel::itemType(const QVariantList &) { return "chat"; }
QVariant ChatListModel::data(const QVariantList &p) {
    int i = p.isEmpty() ? -1 : p.at(0).toInt();
    if (i < 0 || i >= m_rows.size()) return QVariant();
    return m_store->chatRow(m_rows.at(i).id);
}
int ChatListModel::find(qint64 id) const {
    for (int i = 0; i < m_rows.size(); i++) if (m_rows.at(i).id == id) return i;
    return -1;
}
void ChatListModel::setPosition(qint64 id, qint64 order) {
    int old = find(id);
    if (old >= 0) m_rows.removeAt(old);
    int ins = -1;
    if (order > 0) {
        for (ins = 0; ins < m_rows.size(); ins++) {
            const Row &r = m_rows.at(ins);
            if (r.order < order || (r.order == order && r.id < id)) break;
        }
        Row r; r.order = order; r.id = id;
        m_rows.insert(ins, r);
    }
    if (old >= 0 && ins == old) { Q_EMIT itemUpdated(path1(old)); return; }
    if (old >= 0) Q_EMIT itemRemoved(path1(old));
    if (ins >= 0) Q_EMIT itemAdded(path1(ins));
}
void ChatListModel::changed(qint64 id) { int i = find(id); if (i >= 0) Q_EMIT itemUpdated(path1(i)); }
void ChatListModel::clear() { m_rows.clear(); Q_EMIT itemsChanged(bb::cascades::DataModelChangeType::Init); }

// ---------------------------------------------------------------- message model
MessageModel::MessageModel(Store *store) : bb::cascades::DataModel(store), m_store(store) {}
int MessageModel::childCount(const QVariantList &p) { return p.isEmpty() ? m_rows.size() : 0; }
bool MessageModel::hasChildren(const QVariantList &p) { return p.isEmpty() && !m_rows.isEmpty(); }
QString MessageModel::itemType(const QVariantList &p) {
    int i = p.isEmpty() ? -1 : p.at(0).toInt();
    if (i < 0 || i >= m_rows.size()) return "in";
    const M &m = m_rows.at(i);
    if (Store::kindOf(sub(m, "content")) == "service") return "service";
    return m.value("is_outgoing").toBool() ? "out" : "in";
}
QVariant MessageModel::data(const QVariantList &p) {
    int i = p.isEmpty() ? -1 : p.at(0).toInt();
    if (i < 0 || i >= m_rows.size()) return QVariant();
    return m_store->messageRow(m_rows.at(i), i + 1 < m_rows.size() ? &m_rows.at(i + 1) : 0);
}
void MessageModel::reset() { m_rows.clear(); Q_EMIT itemsChanged(bb::cascades::DataModelChangeType::Init); }
QVariantMap *MessageModel::find(qint64 id, int *index) {
    for (int i = 0; i < m_rows.size(); i++)
        if (I(m_rows.at(i).value("id")) == id) { if (index) *index = i; return &m_rows[i]; }
    return 0;
}
void MessageModel::addNew(const M &m) {
    qint64 id = I(m.value("id"));
    int i;
    if (find(id, &i)) { m_rows[i] = m; Q_EMIT itemUpdated(path1(i)); return; }
    for (i = 0; i < m_rows.size(); i++) if (I(m_rows.at(i).value("id")) < id) break;
    m_rows.insert(i, m);
    Q_EMIT itemAdded(path1(i));
    if (i > 0) Q_EMIT itemUpdated(path1(i - 1));      // its day line / sender name depend on the row below it
}
void MessageModel::addOlder(const QVariantList &l) { for (int i = 0; i < l.size(); i++) addNew(l.at(i).toMap()); }
void MessageModel::replace(qint64 oldId, const M &m) {
    int i;
    if (find(oldId, &i)) {
        qint64 id = I(m.value("id"));
        bool keeps = (i == 0 || I(m_rows.at(i - 1).value("id")) > id) && (i + 1 >= m_rows.size() || I(m_rows.at(i + 1).value("id")) < id);
        if (keeps) { m_rows[i] = m; Q_EMIT itemUpdated(path1(i)); return; }
        m_rows.removeAt(i);
        Q_EMIT itemRemoved(path1(i));
    }
    addNew(m);
}
void MessageModel::remove(const QVariantList &ids) {
    for (int k = 0; k < ids.size(); k++) {
        int i;
        if (!find(I(ids.at(k)), &i)) continue;
        m_rows.removeAt(i);
        Q_EMIT itemRemoved(path1(i));
        if (i > 0) Q_EMIT itemUpdated(path1(i - 1));
    }
}
void MessageModel::touch(qint64 id) { int i; if (find(id, &i)) Q_EMIT itemUpdated(path1(i)); }
void MessageModel::touchAll() { for (int i = 0; i < m_rows.size(); i++) Q_EMIT itemUpdated(path1(i)); }
qint64 MessageModel::oldestId() const { return m_rows.isEmpty() ? 0 : I(m_rows.last().value("id")); }
qint64 MessageModel::newestId() const { return m_rows.isEmpty() ? 0 : I(m_rows.first().value("id")); }
int MessageModel::indexOf(qint64 id) { int i = -1; return find(id, &i) ? i : -1; }
QVariantList MessageModel::ids() const {
    QVariantList l;
    for (int i = 0; i < m_rows.size() && i < 60; i++) l << I(m_rows.at(i).value("id"));
    return l;
}

// ---------------------------------------------------------------- store
static const char *privacyKeys[][2] = {
    { "phone", "userPrivacySettingShowPhoneNumber" }, { "lastSeen", "userPrivacySettingShowStatus" },
    { "photo", "userPrivacySettingShowProfilePhoto" }, { "forwards", "userPrivacySettingShowLinkInForwardedMessages" },
    { "calls", "userPrivacySettingAllowCalls" }, { "invites", "userPrivacySettingAllowChatInvites" }, { 0, 0 } };
static QString privacyKeyOf(const QString &type) {
    for (int i = 0; privacyKeys[i][0]; i++) if (type == privacyKeys[i][1]) return privacyKeys[i][0];
    return QString();
}
Store::Store(TdClient *td, const QString &dataDir, QObject *parent)
    : QObject(parent), m_td(td), m_dataDir(dataDir), m_authState("starting"), m_myId(0), m_openId(0), m_loadingHistory(false),
      m_historyEnd(false), m_unread(0), m_active(true), m_demo(false), m_histSerial(0), m_profileId(0), m_hasNewer(false), m_searchSerial(0),
      m_lastTyping(0), m_pendingOpen(0), m_unreadFrom(0), m_pendingJump(0), m_stickerSetId(0), m_storyChat(0), m_storyId(0), m_storyIndex(0), m_storyListCount(-1) {
    listFor("main"); listFor("archive");
    M all; all["key"] = "main"; all["title"] = "All chats";
    m_folders << all;
    m_messages = new MessageModel(this);
    m_typingTimer = new QTimer(this);
    m_typingTimer->setSingleShot(true);
    connect(m_typingTimer, SIGNAL(timeout()), this, SLOT(typingOver()));
    m_stickerTimer = new QTimer(this);
    m_stickerTimer->setSingleShot(true); m_stickerTimer->setInterval(400);
    connect(m_stickerTimer, SIGNAL(timeout()), this, SLOT(rebuildStickers()));
    m_coverTimer = new QTimer(this);
    m_coverTimer->setSingleShot(true); m_coverTimer->setInterval(1500);
    connect(m_coverTimer, SIGNAL(timeout()), this, SLOT(buildCover()));
    m_sweep = new QTimer(this);
    m_sweep->setInterval(2000);
    connect(m_sweep, SIGNAL(timeout()), this, SLOT(sweepTyping()));
    m_calls = new Calls(this, dataDir, this);
    connect(m_calls, SIGNAL(message(QString)), this, SIGNAL(toast(QString)));
    connect(td, SIGNAL(update(QVariantMap)), this, SLOT(onUpdate(QVariantMap)));
    connect(td, SIGNAL(answer(QString,QVariantMap)), this, SLOT(onAnswer(QString,QVariantMap)));
}

void Store::send(const M &request, const QString &tag) {
    if (!m_demo) { m_td->send(request, tag); return; }
    // demo mode (tests without an account): answer the few requests the screens depend on
    QString t = T(request);
    if (t == "getChatHistory") {
        M r = req("messages");
        QVariantList all = m_demoHistory.value(QString::number(I(request.value("chat_id")))).toList(), out;
        qint64 from = I(request.value("from_message_id"));
        for (int i = 0; i < all.size(); i++) if (!from || I(all.at(i).toMap().value("id")) < from) out << all.at(i);
        r["messages"] = out;
        onAnswer(tag, r);
    } else if (t == "getUserPrivacySettingRules") {
        M r = req("userPrivacySettingRules"); QVariantList l; l << QVariant(req("userPrivacySettingRuleAllowContacts")); r["rules"] = l;
        onAnswer(tag, r);
    } else if (t == "getInstalledStickerSets") {
        M r = req("stickerSets"); QVariantList sets;
        M a = req("stickerSetInfo"); a["id"] = 7; a["title"] = "Faces"; sets << a;
        r["sets"] = sets; onAnswer(tag, r);
    } else if (t == "getStickerSet" || t == "getRecentStickers") {
        M r = req("stickers"); QVariantList l;
        if (t == "getStickerSet") {
            const char *e[] = { "😀", "😎", "🥳", "🤔", "😴", "👍", "🙏" };
            for (int i = 0; i < 7; i++) {
                M st = req("sticker"), f = req("file"), loc = req("localFile");
                f["id"] = 900 + i; loc["can_be_downloaded"] = false; f["local"] = loc;
                st["emoji"] = QString::fromUtf8(e[i]); st["format"] = req("stickerFormatWebp"); st["sticker"] = f; st["width"] = 512; st["height"] = 512;
                l << st;
            }
        }
        r["stickers"] = l; onAnswer(tag, r);
    } else if (t == "sendMessage") {
        M m = req("message");
        m["id"] = (qint64)QDateTime::currentMSecsSinceEpoch();
        m["chat_id"] = request.value("chat_id"); m["is_outgoing"] = true; m["date"] = QDateTime::currentDateTime().toTime_t();
        M s = req("messageSenderUser"); s["user_id"] = m_myId; m["sender_id"] = s;
        M c = req("messageText"); c["text"] = sub(request, "input_message_content").value("text"); m["content"] = c;
        M u = req("updateNewMessage"); u["message"] = m;
        onUpdate(u);
    }
}

void Store::loadDemo(const QString &file) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray b = f.readAll();
    M d = TdClient::fromJson(b.constData()).toMap();
    m_demo = true;
    m_demoHistory = sub(d, "history");
    QVariantList l = d.value("updates").toList();
    for (int i = 0; i < l.size(); i++) onUpdate(l.at(i).toMap());
    setAuth("ready");
    qDebug() << "demo data:" << l.size() << "updates," << listFor("main")->size() << "chats";
}

void Store::setAuth(const QString &state, const M &info) {
    m_authState = state; m_authInfo = info;
    if (!m_authError.isEmpty()) { m_authError.clear(); Q_EMIT authErrorChanged(); }
    qDebug() << "auth:" << state;
    Q_EMIT authStateChanged();
}

void Store::sendParameters() {
    QFile f(m_dataDir + "/api.json");
    if (!f.open(QIODevice::ReadOnly)) { setAuth("credentials"); return; }
    if (m_td->remote()) { M c; c["@ui"] = "params"; m_td->control(c); return; }   // the service reads the file itself
    QByteArray b = f.readAll();
    M c = TdClient::fromJson(b.constData()).toMap();
    if (c.value("id").toInt() <= 0 || c.value("hash").toString().isEmpty()) { setAuth("credentials"); return; }
    M p = req("setTdlibParameters");
    p["database_directory"] = m_dataDir + "/td";
    p["files_directory"] = m_dataDir + "/files";
    p["use_file_database"] = true; p["use_chat_info_database"] = true; p["use_message_database"] = true; p["use_secret_chats"] = true;
    p["api_id"] = c.value("id").toInt(); p["api_hash"] = c.value("hash").toString();
    p["system_language_code"] = "en"; p["device_model"] = "BlackBerry Passport"; p["system_version"] = "BlackBerry 10.3";
    p["application_version"] = bb::ApplicationInfo().version();
    send(p, "params");
}

void Store::setCredentials(const QString &apiId, const QString &apiHash) {
    QString id = apiId.trimmed(), hash = apiHash.trimmed();
    if (id.toInt() <= 0 || hash.size() < 16) { m_authError = "Enter the numeric API ID and the API hash from my.telegram.org."; Q_EMIT authErrorChanged(); return; }
    QString path = m_dataDir + "/api.json";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { m_authError = "Cannot save the credentials."; Q_EMIT authErrorChanged(); return; }
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    M c; c["id"] = id; c["hash"] = hash;
    f.write(TdClient::toJson(c));
    f.close();
    sendParameters();
}
void Store::resetCredentials() {             // wrong API ID/hash: forget them and start the library again
    QFile::remove(m_dataDir + "/api.json");
    send(req("close"));
}
void Store::setPhone(const QString &phone) {
    M r = req("setAuthenticationPhoneNumber"); r["phone_number"] = phone.trimmed();
    send(r, "auth");
}
void Store::checkCode(const QString &code) { M r = req("checkAuthenticationCode"); r["code"] = code.trimmed(); send(r, "auth"); }
void Store::checkPassword(const QString &password) { M r = req("checkAuthenticationPassword"); r["password"] = password; send(r, "auth"); }
void Store::logOut() { send(req("logOut"), "auth"); }

static QString friendly(const M &error) {
    QString m = error.value("message").toString();
    if (m.contains("PHONE_NUMBER_INVALID")) return "That phone number is not valid. Use the international form, for example +491701234567.";
    if (m.contains("PHONE_CODE_INVALID")) return "That code is not right.";
    if (m.contains("PHONE_CODE_EXPIRED")) return "That code has expired. Go back and ask for a new one.";
    if (m.contains("PASSWORD_HASH_INVALID")) return "That password is not right.";
    if (m.contains("API_ID_INVALID") || m.contains("API_ID_PUBLISHED_FLOOD")) return "Telegram did not accept the API ID or hash.";
    if (m.contains("retry after")) return "Too many tries. " + m.mid(m.indexOf("retry after")).replace("retry after", "Try again in") + " seconds.";
    return m;
}

void Store::onAnswer(const QString &tag, const M &r) {
    bool error = T(r) == "error";
    if (tag.startsWith("call:")) { m_calls->onAnswer(tag, r); return; }
    if (tag.startsWith("adm:")) { onAdminAnswer(tag, r); return; }
    if (tag == "stories" || tag.startsWith("story")) { onStoryAnswer(tag, r); return; }
    if (tag == "params") {
        if (error) { setAuth("credentials"); m_authError = friendly(r); Q_EMIT authErrorChanged(); }
    } else if (tag == "auth") {
        if (error) { m_authError = friendly(r); Q_EMIT authErrorChanged(); }
    } else if (tag.startsWith("hist:")) {
        if (tag.mid(5).toInt() != m_histSerial) return;
        QVariantList l = r.value("messages").toList();
        m_messages->addOlder(l);
        if (error || l.isEmpty()) m_historyEnd = true;
        m_loadingHistory = false;
        Q_EMIT loadingHistoryChanged();
        if (!m_historyEnd && m_messages->size() < 30) loadOlder();
        viewOpenMessages();
    } else if (tag.startsWith("around:") || tag.startsWith("newer:")) {
        QStringList p = tag.split(':');
        if (p.value(1).toInt() != m_histSerial) return;
        QVariantList l = r.value("messages").toList();
        int before = m_messages->size();
        m_messages->addOlder(l);
        if (p.at(0) == "newer" && m_messages->size() == before) { m_hasNewer = false; Q_EMIT hasNewerChanged(); }
        if (p.at(0) == "around") {
            int i = m_messages->indexOf(p.value(2).toLongLong());
            if (i >= 0) Q_EMIT scrollTo(p.value(3) == "unread" && i > 0 ? i - 1 : i);
            qint64 last = I(sub(m_chats.value(m_openId), "last_message").value("id"));
            if (last && m_messages->newestId() >= last) { m_hasNewer = false; Q_EMIT hasNewerChanged(); }
        }
        m_loadingHistory = false;
        Q_EMIT loadingHistoryChanged();
        viewOpenMessages();
    } else if (tag.startsWith("pinned:")) {
        if (tag.mid(7).toInt() != m_histSerial) return;
        m_pinned = error ? M() : r;
        refreshOpenChat();
    } else if (tag == "me") {
        if (!error) { qint64 id = I(r.value("id")); m_users[id] = r; if (!m_myId) m_myId = id; Q_EMIT meChanged(); }
    } else if (tag.startsWith("s:")) {                     // search answers: s:<serial>:<kind>
        QStringList p = tag.split(':');
        if (p.value(1).toInt() != m_searchSerial || error) return;
        QString kind = p.value(2);
        QVariantList rows;
        if (kind == "public") {
            QVariantList ids = r.value("chat_ids").toList();
            for (int i = 0; i < ids.size(); i++) if (m_chats.contains(I(ids.at(i)))) rows << chatRow(I(ids.at(i)));
        } else {
            QVariantList l = r.value("messages").toList();
            for (int i = 0; i < l.size(); i++) rows << foundRow(l.at(i).toMap());
        }
        Q_EMIT searchDone(kind, rows);
    } else if (tag.startsWith("full:")) {
        if (error || I(tag.mid(5)) != m_profileId) return;
        if (T(r) == "userFullInfo") m_userFull[m_profileId] = r; else m_profileFull = r;
        buildProfile();
        if (T(r) == "supergroupFullInfo" && r.value("can_get_members").toBool()) {
            M q = req("getSupergroupMembers"); q["supergroup_id"] = I(sub(sub(m_chats.value(m_profileId), "type"), "supergroup_id"));
            q["filter"] = req("supergroupMembersFilterRecent"); q["offset"] = 0; q["limit"] = 100;
            send(q, "members:" + QString::number(m_profileId));
        }
    } else if (tag.startsWith("members:")) {
        if (error || I(tag.mid(8)) != m_profileId) return;
        m_profileFull["members"] = r.value("members");
        buildProfile();
    } else if (tag == "contacts") {
        if (error) return;
        QVariantList ids = r.value("user_ids").toList(), rows;
        QMap<QString, qint64> sorted;
        for (int i = 0; i < ids.size(); i++) sorted.insertMulti(userName(I(ids.at(i))).toLower(), I(ids.at(i)));
        for (QMap<QString, qint64>::const_iterator i = sorted.constBegin(); i != sorted.constEnd(); ++i) {
            M row = userRow(i.value(), QString()); row["title"] = userName(i.value()); rows << row;
        }
        Q_EMIT searchDone("contacts", rows);
    } else if (tag == "callback") {
        if (error) { Q_EMIT toast("The bot didn't answer"); return; }
        if (!r.value("url").toString().isEmpty()) openLink(r.value("url").toString());
        else if (!r.value("text").toString().isEmpty()) Q_EMIT toast(r.value("text").toString());
    } else if (tag.startsWith("link:")) {
        QString url = tag.mid(5), lt = T(r);
        if (!error && lt == "internalLinkTypePublicChat") {
            M q = req("searchPublicChat"); q["username"] = r.value("chat_username"); send(q, "openchat");
        } else if (!error && lt == "internalLinkTypeMessage") {
            M q = req("getMessageLinkInfo"); q["url"] = url; send(q, "msglink");
        } else {
            bb::system::InvokeRequest ir; ir.setTarget("sys.browser"); ir.setAction("bb.action.OPEN"); ir.setUri(QUrl(url));
            bb::system::InvokeManager im; im.invoke(ir);
        }
    } else if (tag == "msglink") {
        if (error) { Q_EMIT toast(friendly(r)); return; }
        qint64 cid = I(r.value("chat_id")), mid = I(sub(r, "message").value("id"));
        if (!cid) { Q_EMIT toast("Message not found"); return; }
        if (cid == m_openId && mid) jumpTo(mid); else { m_pendingJump = mid; Q_EMIT openChatRequested(cid); }
    } else if (tag == "stsets") {
        if (error) return;
        QVariantList l = r.value("sets").toList();
        m_stickerSets.clear();
        M rec; rec["id"] = 0; rec["title"] = "Recent"; m_stickerSets << rec;
        for (int i = 0; i < l.size(); i++) { M x = l.at(i).toMap(), row; row["id"] = I(x.value("id")); row["title"] = x.value("title"); m_stickerSets << row; }
        Q_EMIT stickersChanged();
    } else if (tag == "gifs") {
        if (error) return;
        m_gifRaw = r.value("animations").toList();
        rebuildStickers();
    } else if (tag.startsWith("stset:")) {
        if (error || I(tag.mid(6)) != m_stickerSetId) return;
        m_stickerRaw = r.value("stickers").toList();
        if (!m_stickerSetId && m_stickerRaw.isEmpty() && m_stickerSets.size() > 1) { showStickerSet(m_stickerSets.at(1).toMap().value("id")); return; }
        rebuildStickers();
    } else if (tag.startsWith("kbd:")) {
        m_keyboard.clear(); m_keyboardRaw.clear();
        if (!error && I(tag.mid(4)) == m_openId) {
            M rm = sub(r, "reply_markup");
            if (T(rm) == "replyMarkupShowKeyboard") {
                m_keyboardRaw = rm.value("rows").toList();
                for (int i = 0; i < m_keyboardRaw.size(); i++) {
                    QVariantList row = m_keyboardRaw.at(i).toList(), out;
                    for (int k = 0; k < row.size(); k++) out << row.at(k).toMap().value("text");
                    m_keyboard << QVariant(out);
                }
            }
        }
        Q_EMIT keyboardChanged();
    } else if (tag.startsWith("privacy:")) {
        if (error) return;
        QString key = tag.mid(8);
        QVariantList rules = r.value("rules").toList();
        QString base = "Nobody"; int exceptions = 0;
        for (int i = 0; i < rules.size(); i++) {
            QString t = T(rules.at(i).toMap());
            if (t == "userPrivacySettingRuleAllowAll") { base = "Everybody"; break; }
            if (t == "userPrivacySettingRuleAllowContacts") { base = "My contacts"; break; }
            if (t == "userPrivacySettingRuleRestrictAll") break;
        }
        for (int i = 0; i < rules.size(); i++) {
            QString t = T(rules.at(i).toMap());
            if (t.endsWith("Users")) exceptions += rules.at(i).toMap().value("user_ids").toList().size();
            if (t.endsWith("ChatMembers")) exceptions += rules.at(i).toMap().value("chat_ids").toList().size();
        }
        m_privacyRules[key] = rules;
        m_privacy[key] = exceptions ? base + " (+" + QString::number(exceptions) + (exceptions == 1 ? " exception)" : " exceptions)") : base;
        Q_EMIT privacyChanged();
    } else if (tag == "created") {
        if (error) { Q_EMIT toast(friendly(r)); return; }
        qint64 cid = T(r) == "createdBasicGroupChat" ? I(r.value("chat_id")) : I(r.value("id"));
        if (cid) Q_EMIT openChatRequested(cid);
    } else if (tag == "mefull") {
        if (!error) { m_userFull[m_myId] = r; Q_EMIT meChanged(); }
    } else if (tag == "saved" || tag == "savedbio") {
        if (error) Q_EMIT toast(friendly(r)); else { Q_EMIT toast("Saved"); if (tag == "savedbio") loadMyInfo(); }
    } else if (tag == "removed") {
        if (error) { Q_EMIT toast(friendly(r)); return; }
        Q_EMIT toast("Removed from the group");
        if (m_profileId) openProfile(m_profileId);
    } else if (tag == "added") {
        if (error) { Q_EMIT toast(friendly(r)); return; }
        int failed = r.value("failed_to_add_members").toList().size();
        Q_EMIT toast(failed ? QString::number(failed) + " could not be added (their privacy settings)" : QString("Added"));
        if (m_profileId) openProfile(m_profileId);
    } else if (tag == "invite") {
        if (error) { Q_EMIT toast(friendly(r)); return; }
        m_profile["inviteLink"] = r.value("invite_link"); Q_EMIT profileChanged();
        copyTextRaw(r.value("invite_link").toString());
    } else if (tag == "resend") {
        if (error) Q_EMIT toast(friendly(r));
    } else if (tag == "join") {
        if (!error) Q_EMIT toast("Joined");
    } else if (tag == "openchat") {
        if (!error) Q_EMIT openChatRequested(I(r.value("id")));
    } else if (tag == "sessions") {
        if (error) return;
        QVariantList l = r.value("sessions").toList();
        m_sessions.clear();
        for (int i = 0; i < l.size(); i++) {
            M x = l.at(i).toMap(), row;
            row["id"] = x.value("id").toString();
            row["current"] = x.value("is_current").toBool();
            row["title"] = (x.value("application_name").toString() + " " + x.value("application_version").toString()).trimmed();
            row["device"] = (x.value("device_model").toString() + ", " + x.value("platform").toString() + " " + x.value("system_version").toString()).trimmed();
            row["place"] = x.value("location").toString() + (x.value("is_current").toBool() ? QString(" · this phone") : " · " + listDate(x.value("last_active_date").toUInt()));
            m_sessions << row;
        }
        Q_EMIT sessionsChanged();
    } else if (tag.startsWith("chats:")) {
        // error 404 = the whole list is loaded
    } else if (error && !tag.startsWith("q")) {
        Q_EMIT toast(friendly(r));
    }
}

void Store::onUpdate(const M &u) {
    const QString t = T(u);
    if (t == "updateCall") { m_calls->onUpdateCall(sub(u, "call")); return; }
    if ((t.startsWith("updateStory") || t == "updateChatActiveStories") && onStoryUpdate(t, u)) return;
    if (!m_coverTimer->isActive() && (t == "updateNewMessage" || t == "updateChatReadInbox" || t == "updateUnreadMessageCount" || t == "updateChatLastMessage"
        || t == "updateChatPosition" || t == "updateNewChat" || t == "updateChatTitle")) m_coverTimer->start();
    if (t == "updateAuthorizationState") {
        M s = sub(u, "authorization_state");
        QString st = T(s);
        if (st == "authorizationStateWaitTdlibParameters") sendParameters();
        else if (st == "authorizationStateWaitPhoneNumber") setAuth("phone");
        else if (st == "authorizationStateWaitCode") {
            M info, ci = sub(s, "code_info");
            info["phone"] = ci.value("phone_number");
            QString ct = T(sub(ci, "type"));
            info["canResend"] = !sub(ci, "next_type").isEmpty();
            info["timeout"] = ci.value("timeout").toInt();
            QString nt = T(sub(ci, "next_type"));
            info["resendHow"] = nt == "authenticationCodeTypeSms" ? "Send the code by SMS" : nt == "authenticationCodeTypeCall" ? "Call me with the code" : "Send the code again";
            info["where"] = ct == "authenticationCodeTypeTelegramMessage" ? "Telegram sent the code to your other devices signed in to this account."
                          : ct == "authenticationCodeTypeSms" ? "Telegram sent the code by SMS."
                          : ct == "authenticationCodeTypeCall" ? "Telegram is calling you with the code." : "Telegram sent you a code.";
            setAuth("code", info);
        } else if (st == "authorizationStateWaitPassword") { M info; info["hint"] = s.value("password_hint"); setAuth("password", info); }
        else if (st == "authorizationStateWaitRegistration") { M info; info["text"] = "This number has no Telegram account yet. Create it in an official app first."; setAuth("other", info); }
        else if (st == "authorizationStateWaitOtherDeviceConfirmation" || st == "authorizationStateWaitEmailAddress" || st == "authorizationStateWaitEmailCode"
                 || st == "authorizationStateWaitPremiumPurchase") { M info; info["text"] = "Telegram asked for a sign-in step this app does not support yet (" + st.mid(22) + ")."; setAuth("other", info); }
        else if (st == "authorizationStateReady") {
            setAuth("ready");
            if (m_pendingOpen) { Q_EMIT openChatRequested(m_pendingOpen); m_pendingOpen = 0; }
            loadMoreChats("main");
            send(req("getMe"), "me");
            loadStories();
        } else if (st == "authorizationStateClosed") {
            m_users.clear(); m_chats.clear(); m_basicGroups.clear(); m_supergroups.clear(); m_files.clear(); m_fileChats.clear(); m_fileMsgs.clear(); m_asked.clear();
            Q_FOREACH (ChatListModel *l, m_lists) l->clear();
            m_activeStories.clear(); Q_EMIT storiesChanged();
            m_messages->reset(); m_openId = 0; m_unread = 0; m_profileId = 0; m_userFull.clear();
            Q_EMIT unreadChanged();
            setAuth("starting");
            m_td->restart();
        } else { M info; info["text"] = "Please wait…"; setAuth("other", info); }
    } else if (t == "updateOption") {
        if (u.value("name").toString() == "my_id") { m_myId = I(sub(u, "value").value("value")); Q_EMIT meChanged(); }
    } else if (t == "updateUser") {
        M usr = sub(u, "user"); qint64 id = I(usr.value("id"));
        m_users[id] = usr;
        if (id == m_openId) refreshOpenChat();
        if (id == m_myId) Q_EMIT meChanged();
        if (id == m_profileId) buildProfile();
    } else if (t == "updateUserStatus") {
        qint64 id = I(u.value("user_id"));
        if (m_users.contains(id)) m_users[id]["status"] = u.value("status");
        if (id == m_openId) refreshOpenChat();
    } else if (t == "updateBasicGroup") { M g = sub(u, "basic_group"); m_basicGroups[I(g.value("id"))] = g; }
    else if (t == "updateSupergroup") { M g = sub(u, "supergroup"); m_supergroups[I(g.value("id"))] = g; }
    else if (t == "updateNewChat") {
        M c = sub(u, "chat"); qint64 id = I(c.value("id"));
        m_chats[id] = c;
        setPositions(id, c.value("positions").toList());
    } else if (t == "updateChatPosition") {
        applyPosition(I(u.value("chat_id")), sub(u, "position"));
    } else if (t == "updateChatLastMessage" || t == "updateChatDraftMessage") {
        qint64 id = I(u.value("chat_id"));
        if (!m_chats.contains(id)) return;
        if (t == "updateChatLastMessage") m_chats[id]["last_message"] = u.value("last_message");
        else m_chats[id]["draft_message"] = u.value("draft_message");
        setPositions(id, u.value("positions").toList());
        chatTouched(id);
    } else if (t == "updateChatTitle") setChatField(I(u.value("chat_id")), "title", u.value("title"));
    else if (t == "updateChatPhoto") setChatField(I(u.value("chat_id")), "photo", u.value("photo"));
    else if (t == "updateChatPermissions") setChatField(I(u.value("chat_id")), "permissions", u.value("permissions"));
    else if (t == "updateChatNotificationSettings") setChatField(I(u.value("chat_id")), "notification_settings", u.value("notification_settings"));
    else if (t == "updateChatUnreadMentionCount") setChatField(I(u.value("chat_id")), "unread_mention_count", u.value("unread_mention_count"));
    else if (t == "updateChatIsMarkedAsUnread") setChatField(I(u.value("chat_id")), "is_marked_as_unread", u.value("is_marked_as_unread"));
    else if (t == "updateChatReadInbox") {
        qint64 id = I(u.value("chat_id"));
        if (!m_chats.contains(id)) return;
        m_chats[id]["unread_count"] = u.value("unread_count");
        m_chats[id]["last_read_inbox_message_id"] = u.value("last_read_inbox_message_id");
        chatTouched(id);
    } else if (t == "updateChatReadOutbox") {
        qint64 id = I(u.value("chat_id"));
        if (!m_chats.contains(id)) return;
        m_chats[id]["last_read_outbox_message_id"] = u.value("last_read_outbox_message_id");
        chatTouched(id);
        if (id == m_openId) m_messages->touchAll();
    } else if (t == "updateChatAction") {
        qint64 cid = I(u.value("chat_id"));
        M sender = sub(u, "sender_id");
        if (I(sender.value("user_id")) == m_myId) return;
        QString a = T(sub(u, "action"));
        if (a == "chatActionCancel") m_listTyping.remove(cid);
        else {
            QString w = a.startsWith("chatActionRecording") ? "recording…" : a.startsWith("chatActionUploading") ? "sending a file…" : a == "chatActionChoosingSticker" ? "choosing a sticker…" : "typing…";
            M c = m_chats.value(cid);
            m_listTyping[cid] = qMakePair(isGroup(c) ? userName(I(sender.value("user_id"))).section(' ', 0, 0) + " is " + w : w, QDateTime::currentMSecsSinceEpoch() + 6000);
            if (!m_sweep->isActive()) m_sweep->start();
        }
        Q_FOREACH (ChatListModel *l, m_lists) l->changed(cid);
        if (cid != m_openId) return;
        if (a == "chatActionCancel") { typingOver(); return; }
        QString what = a == "chatActionTyping" ? "typing…" : a.startsWith("chatActionRecording") ? "recording…" : a.startsWith("chatActionUploading") ? "sending a file…" : "typing…";
        m_typing = isGroup(m_chats.value(m_openId)) ? senderName(sender) + " is " + what : what;
        m_typingTimer->start(6000);
        refreshOpenChat();
    } else if (t == "updateNewMessage") {
        M m = sub(u, "message");
        if (!m.value("is_outgoing").toBool() && m_listTyping.remove(I(m.value("chat_id")))) {}
        if (I(m.value("chat_id")) == m_openId && !m_hasNewer) {
            m_messages->addNew(m);
            if (!m.value("is_outgoing").toBool()) { typingOver(); viewOpenMessages(); }
            Q_EMIT newestAdded(m.value("is_outgoing").toBool());
        }
        notify(m);
    } else if (t == "updateMessageSendSucceeded" || t == "updateMessageSendFailed") {
        M m = sub(u, "message");
        if (I(m.value("chat_id")) == m_openId) m_messages->replace(I(u.value("old_message_id")), m);
        if (t == "updateMessageSendFailed") Q_EMIT toast("Not sent: " + friendly(sub(u, "error")));
    } else if (t == "updateMessageContent") {
        if (I(u.value("chat_id")) != m_openId) return;
        qint64 id = I(u.value("message_id"));
        M *m = m_messages->find(id);
        if (m) { (*m)["content"] = u.value("new_content"); m_messages->touch(id); }
    } else if (t == "updateMessageEdited") {
        if (I(u.value("chat_id")) != m_openId) return;
        qint64 id = I(u.value("message_id"));
        M *m = m_messages->find(id);
        if (m) { (*m)["edit_date"] = u.value("edit_date"); m_messages->touch(id); }
    } else if (t == "updateDeleteMessages") {
        if (I(u.value("chat_id")) == m_openId && u.value("is_permanent").toBool()) m_messages->remove(u.value("message_ids").toList());
    } else if (t == "updateFile") {
        M f = sub(u, "file"); int id = f.value("id").toInt();
        m_files[id] = f;
        if (sub(f, "local").value("is_downloading_completed").toBool() && m_fileChats.contains(id))
            Q_FOREACH (qint64 c, m_fileChats.value(id)) chatTouched(c);
        if (m_fileMsgs.contains(id)) Q_FOREACH (qint64 mid, m_fileMsgs.value(id)) m_messages->touch(mid);
        if (m_storyFiles.contains(id) && sub(f, "local").value("is_downloading_completed").toBool()) buildStory();
        if (m_stickerFiles.contains(id) && sub(f, "local").value("is_downloading_completed").toBool() && !m_stickerTimer->isActive()) m_stickerTimer->start();
    } else if (t == "updateUserPrivacySettingRules") {
        QString key = privacyKeyOf(T(sub(u, "setting")));
        if (key.isEmpty()) return;
        M ans = sub(u, "rules"); ans["@type"] = "userPrivacySettingRules";
        onAnswer("privacy:" + key, ans);
    } else if (t == "updateSecretChat") {
        M sc = sub(u, "secret_chat"); int sid = sc.value("id").toInt();
        m_secretChats[sid] = sc;
        Q_FOREACH (qint64 cid, m_chats.keys()) {
            M ty = sub(m_chats.value(cid), "type");
            if (T(ty) != "chatTypeSecret" || ty.value("secret_chat_id").toInt() != sid) continue;
            chatTouched(cid);
            if (cid == m_openId) refreshOpenChat();
        }
    } else if (t == "updateChatReplyMarkup") {
        if (I(u.value("chat_id")) == m_openId) { m_chats[m_openId]["reply_markup_message_id"] = u.value("reply_markup_message_id"); loadKeyboard(); }
    } else if (t == "updateConnectionState") {
        QString s = T(sub(u, "state"));
        m_connection = s == "connectionStateReady" ? "" : s == "connectionStateUpdating" ? "Updating…" : s == "connectionStateWaitingForNetwork" ? "Waiting for network…" : "Connecting…";
        Q_EMIT connectionChanged();
    } else if (t == "updateUnreadMessageCount") {
        if (T(sub(u, "chat_list")) == "chatListMain") { m_unread = u.value("unread_count").toInt(); Q_EMIT unreadChanged(); }
    } else if (t == "updateScopeNotificationSettings") {
        m_scopeMute[T(sub(u, "scope"))] = sub(u, "notification_settings").value("mute_for").toInt();
        m_scopeSettings[T(sub(u, "scope"))] = sub(u, "notification_settings");
        Q_EMIT notifyScopesChanged();
    } else if (t == "updateChatFolders") {
        QVariantList l = u.value("chat_folders").toList();
        int mainAt = u.value("main_chat_list_position").toInt();
        M all; all["key"] = "main"; all["title"] = "All chats";
        m_folders.clear();
        for (int i = 0; i < l.size(); i++) {
            M f = l.at(i).toMap(), r;
            r["key"] = "folder:" + QString::number(f.value("id").toInt());
            r["title"] = sub(sub(f, "name"), "text").value("text").toString();
            m_folders << r;
        }
        m_folders.insert(qBound(0, mainAt, m_folders.size()), all);
        Q_EMIT foldersChanged();
    } else if (t == "updateMessageIsPinned") {
        if (I(u.value("chat_id")) != m_openId) return;
        M *m = m_messages->find(I(u.value("message_id")));
        if (m) (*m)["is_pinned"] = u.value("is_pinned");
        M r = req("getChatPinnedMessage"); r["chat_id"] = m_openId; send(r, "pinned:" + QString::number(m_histSerial));
    } else if (t == "updateMessageInteractionInfo") {
        if (I(u.value("chat_id")) != m_openId) return;
        qint64 id = I(u.value("message_id"));
        M *m = m_messages->find(id);
        if (m) { (*m)["interaction_info"] = u.value("interaction_info"); m_messages->touch(id); }
    } else if (t == "updateUserFullInfo") {
        m_userFull[I(u.value("user_id"))] = sub(u, "user_full_info");
        if (m_profileId && m_profileId == I(u.value("user_id"))) buildProfile();
    } else if (t == "updateBasicGroupFullInfo" || t == "updateSupergroupFullInfo") {
        if (!m_profileId) return;
        M type = sub(m_chats.value(m_profileId), "type");
        qint64 gid = I(t == "updateBasicGroupFullInfo" ? u.value("basic_group_id") : u.value("supergroup_id"));
        if (gid == I(type.value(t == "updateBasicGroupFullInfo" ? "basic_group_id" : "supergroup_id"))) {
            m_profileFull = sub(u, t == "updateBasicGroupFullInfo" ? "basic_group_full_info" : "supergroup_full_info");
            buildProfile();
        }
    }
}

void Store::setChatField(qint64 id, const char *key, const QVariant &value) {
    if (!m_chats.contains(id)) return;
    m_chats[id][key] = value;
    chatTouched(id);
}
static QString listKey(const M &l) {
    QString t = T(l);
    if (t == "chatListMain") return "main";
    if (t == "chatListArchive") return "archive";
    return "folder:" + QString::number(l.value("chat_folder_id").toInt());
}
static M listOf(const QString &key) {
    if (key == "archive") return req("chatListArchive");
    if (key.startsWith("folder:")) { M l = req("chatListFolder"); l["chat_folder_id"] = key.mid(7).toInt(); return l; }
    return req("chatListMain");
}
ChatListModel *Store::listFor(const QString &key) {
    ChatListModel *l = m_lists.value(key);
    if (l) return l;
    l = new ChatListModel(this, key);
    m_lists[key] = l;
    if (key.startsWith("folder:")) {                  // fill it from what we know, then ask for the rest
        for (QHash<qint64, M>::const_iterator i = m_chats.constBegin(); i != m_chats.constEnd(); ++i) {
            QVariantList pos = i.value().value("positions").toList();
            for (int k = 0; k < pos.size(); k++)
                if (listKey(sub(pos.at(k).toMap(), "list")) == key) l->setPosition(i.key(), I(pos.at(k).toMap().value("order")));
        }
        if (m_authState == "ready") loadMoreChats(key);
    }
    return l;
}
void Store::applyPosition(qint64 id, const M &pos) {
    if (!m_chats.contains(id)) return;
    QString key = listKey(sub(pos, "list"));
    QVariantList l = m_chats[id].value("positions").toList(), out;
    for (int i = 0; i < l.size(); i++) if (listKey(sub(l.at(i).toMap(), "list")) != key) out << l.at(i);
    qint64 order = I(pos.value("order"));
    if (order) out << pos;
    m_chats[id]["positions"] = out;
    if (m_lists.contains(key)) m_lists.value(key)->setPosition(id, order);
}
void Store::setPositions(qint64 id, const QVariantList &positions) {
    if (!m_chats.contains(id)) return;
    QHash<QString, qint64> orders;
    for (int i = 0; i < positions.size(); i++) {
        M p = positions.at(i).toMap();
        orders[listKey(sub(p, "list"))] = I(p.value("order"));
    }
    m_chats[id]["positions"] = positions;
    for (QHash<QString, ChatListModel *>::const_iterator i = m_lists.constBegin(); i != m_lists.constEnd(); ++i)
        i.value()->setPosition(id, orders.value(i.key()));
}
void Store::chatTouched(qint64 id) {
    Q_FOREACH (ChatListModel *l, m_lists) l->changed(id);
    if (id == m_openId) refreshOpenChat();
    if (id == m_profileId) buildProfile();
}
void Store::typingOver() { if (m_typing.isEmpty()) return; m_typing.clear(); m_typingTimer->stop(); refreshOpenChat(); }

// ---------------------------------------------------------------- names, times, text
bool Store::isChannel(const M &c) const { M t = sub(c, "type"); return T(t) == "chatTypeSupergroup" && t.value("is_channel").toBool(); }
bool Store::isGroup(const M &c) const { QString t = T(sub(c, "type")); return t == "chatTypeBasicGroup" || (t == "chatTypeSupergroup" && !isChannel(c)); }
bool Store::isMuted(const M &c) const {
    M n = sub(c, "notification_settings");
    if (!n.value("use_default_mute_for").toBool()) return n.value("mute_for").toInt() > 0;
    QString t = T(sub(c, "type"));
    QString scope = isChannel(c) ? "notificationSettingsScopeChannelChats" : (t == "chatTypePrivate" || t == "chatTypeSecret") ? "notificationSettingsScopePrivateChats" : "notificationSettingsScopeGroupChats";
    return m_scopeMute.value(scope) > 0;
}
QString Store::initials(const QString &title) const {
    QStringList w = title.split(' ', QString::SkipEmptyParts);
    QString r;
    for (int i = 0; i < w.size() && r.size() < 2; i++) { QChar c = w.at(i).at(0); if (c.isLetterOrNumber()) r += c.toUpper(); }
    return r.isEmpty() ? QString("#") : r;
}
static const char *palette[] = { "#e17076", "#eda86c", "#a695e7", "#7bc862", "#6ec9cb", "#65aadd", "#ee7aae" };
QVariantMap Store::userCard(qint64 uid) {
    M u = m_users.value(uid), r;
    QString name = userName(uid), photo;
    filePath(sub(sub(u, "profile_photo"), "big"), &photo, true);
    r["name"] = name; r["initials"] = initials(name); r["photo"] = photo; r["color"] = QString(palette[uid % 7]);
    return r;
}
QString Store::userName(qint64 id) const {
    M u = m_users.value(id);
    QString n = (u.value("first_name").toString() + " " + u.value("last_name").toString()).trimmed();
    return n.isEmpty() ? QString("Deleted account") : n;
}
QString Store::senderName(const M &s) const {
    if (T(s) == "messageSenderUser") return userName(I(s.value("user_id")));
    return m_chats.value(I(s.value("chat_id"))).value("title").toString();
}
QString Store::clock(uint t) { return QDateTime::fromTime_t(t).toString("hh:mm"); }
QString Store::listDate(uint t) {
    QDateTime d = QDateTime::fromTime_t(t); QDate today = QDate::currentDate();
    int days = d.date().daysTo(today);
    if (days <= 0) return d.toString("hh:mm");
    if (days < 7) return d.toString("ddd");
    return d.toString(d.date().year() == today.year() ? "d MMM" : "d MMM yyyy");
}
QString Store::dayName(uint t) {
    QDate d = QDateTime::fromTime_t(t).date(), today = QDate::currentDate();
    int days = d.daysTo(today);
    if (days == 0) return "Today";
    if (days == 1) return "Yesterday";
    return d.toString(d.year() == today.year() ? "d MMMM" : "d MMMM yyyy");
}
QString Store::sizeText(qint64 b) {
    if (b < 1024) return QString::number(b) + " B";
    if (b < 1024 * 1024) return QString::number(b / 1024.0, 'f', 0) + " KB";
    if (b < 1024LL * 1024 * 1024) return QString::number(b / 1048576.0, 'f', 1) + " MB";
    return QString::number(b / 1073741824.0, 'f', 2) + " GB";
}
static QString durationText(int s) { return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')); }
QString Store::statusText(const M &u) const {
    if (I(u.value("id")) == 777000) return "service notifications";
    if (T(sub(u, "type")) == "userTypeBot") return "bot";
    M s = sub(u, "status"); QString t = T(s);
    if (t == "userStatusOnline") return "online";
    if (t == "userStatusOffline") {
        uint w = s.value("was_online").toUInt();
        QDate d = QDateTime::fromTime_t(w).date();
        return "last seen " + (d == QDate::currentDate() ? "at " + clock(w) : listDate(w));
    }
    if (t == "userStatusRecently") return "last seen recently";
    if (t == "userStatusLastWeek") return "last seen within a week";
    if (t == "userStatusLastMonth") return "last seen within a month";
    return "last seen a long time ago";
}

static QString esc(const QString &s) {
    QString o;
    for (int i = 0; i < s.size(); i++) {
        ushort c = s.at(i).unicode();
        if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;";
        else if (c == '\'') o += "&#39;"; else if (c == '\n') o += "<br/>"; else if (c < 0x20 && c != '\t') continue; else o += s.at(i);
    }
    return o;
}
// formattedText -> the small HTML subset Cascades labels draw (bold, italic, links...)
QString Store::html(const M &ft) {
    QString text = ft.value("text").toString();
    QVariantList ents = ft.value("entities").toList();
    QMap<int, QStringList> opens, closes;
    for (int i = 0; i < ents.size(); i++) {
        M e = ents.at(i).toMap(), ty = sub(e, "type"); QString t = T(ty).mid(14);       // after "textEntityType"
        int off = e.value("offset").toInt(), len = e.value("length").toInt();
        if (off < 0 || len <= 0 || off + len > text.size()) continue;
        QString part = text.mid(off, len), o, c;
        if (t == "Bold") { o = "<b>"; c = "</b>"; }
        else if (t == "Italic" || t == "BlockQuote" || t == "ExpandableBlockQuote") { o = "<i>"; c = "</i>"; }
        else if (t == "Underline") { o = "<span style='text-decoration:underline'>"; c = "</span>"; }
        else if (t == "Strikethrough") { o = "<span style='text-decoration:line-through'>"; c = "</span>"; }
        else if (t == "Code" || t == "Pre" || t == "PreCode") { o = "<span style='font-family:monospace'>"; c = "</span>"; }
        else {
            QString href;
            if (t == "Url") href = part.contains("://") ? part : "http://" + part;
            else if (t == "TextUrl") href = ty.value("url").toString();
            else if (t == "Mention") href = "https://t.me/" + part.mid(1);
            else if (t == "EmailAddress") href = "mailto:" + part;
            else if (t == "PhoneNumber") href = "tel:" + part;
            if (href.isEmpty()) continue;
            o = "<a href=\"" + esc(href) + "\">"; c = "</a>";
        }
        opens[off].append(o); closes[off + len].prepend(c);
    }
    QString out = "<html>";
    for (int i = 0; i <= text.size(); i++) {
        if (closes.contains(i)) out += closes.value(i).join("");
        if (opens.contains(i)) out += opens.value(i).join("");
        if (i < text.size()) out += esc(text.mid(i, 1));
    }
    return out + "</html>";
}

QString Store::kindOf(const M &content) {
    QString t = T(content).mid(7);                       // after "message"
    if (t == "Text") return "text";
    if (t == "Photo") return "photo";
    if (t == "Video") return "video";
    if (t == "Animation") return "animation";
    if (t == "Sticker") return "sticker";
    if (t == "VoiceNote") return "voice";
    if (t == "VideoNote") return "videonote";
    if (t == "Audio") return "audio";
    if (t == "Document") return "document";
    if (t == "Call") return "call";
    if (t.startsWith("Chat") || t == "PinMessage" || t == "ContactRegistered" || t.endsWith("ChatCreate") || t == "ScreenshotTaken" || t.startsWith("VideoChat")
        || t.startsWith("ForumTopic") || t == "CustomServiceAction" || t == "GameScore" || t == "ProximityAlertTriggered") return "service";
    return "other";
}

static int callSeconds(const M &c) { return c.value("duration").toInt(); }
static bool callBad(const M &c) {                  // missed / declined / not answered: shown in red
    QString r = T(sub(c, "discard_reason"));
    return r == "callDiscardReasonMissed" || r == "callDiscardReasonDeclined";
}
static QString callText(const M &c, bool out) {
    QString r = T(sub(c, "discard_reason")); bool video = c.value("is_video").toBool();
    QString what = video ? "video call" : "call";
    if (r == "callDiscardReasonMissed") return out ? "Cancelled " + what : "Missed " + what;
    if (r == "callDiscardReasonDeclined") return out ? (video ? "Video call declined" : "Call declined") : "Declined " + what;
    return (out ? "Outgoing " : "Incoming ") + what;
}
static QString clockLen(int s) {
    return s >= 3600 ? QString("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'))
                     : QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}
QString Store::contentSummary(const M &c) const {
    QString t = T(c).mid(7);
    QString cap = sub(c, "caption").value("text").toString().replace('\n', ' ');
    if (t == "Text") return sub(c, "text").value("text").toString().replace('\n', ' ');
    if (t == "Photo") return cap.isEmpty() ? QString("Photo") : "Photo, " + cap;
    if (t == "Video") return cap.isEmpty() ? QString("Video") : "Video, " + cap;
    if (t == "Animation") return cap.isEmpty() ? QString("GIF") : "GIF, " + cap;
    if (t == "Sticker") return sub(c, "sticker").value("emoji").toString() + " Sticker";
    if (t == "VoiceNote") return "Voice message";
    if (t == "VideoNote") return "Video message";
    if (t == "Audio") { QString n = sub(c, "audio").value("title").toString(); return n.isEmpty() ? QString("Audio") : n; }
    if (t == "Document") { QString n = sub(c, "document").value("file_name").toString(); return n.isEmpty() ? QString("File") : n; }
    if (t == "Location" || t == "Venue") return "Location";
    if (t == "Contact") return "Contact";
    if (t == "Poll") return "Poll: " + sub(sub(c, "poll"), "question").value("text").toString();
    if (t == "Call") return c.value("is_video").toBool() ? "Video call" : "Call";
    if (t == "ChatAddMembers") {
        QVariantList ids = c.value("member_user_ids").toList(); QStringList names;
        for (int i = 0; i < ids.size() && i < 5; i++) names << userName(I(ids.at(i)));
        return "added " + names.join(", ");
    }
    if (t == "ChatJoinByLink" || t == "ChatJoinByRequest") return "joined the group";
    if (t == "ChatDeleteMember") return "left the group";
    if (t == "ChatChangeTitle") return "changed the name to " + c.value("title").toString();
    if (t == "ChatChangePhoto") return "changed the photo";
    if (t == "ChatDeletePhoto") return "removed the photo";
    if (t == "PinMessage") return "pinned a message";
    if (t == "ContactRegistered") return "joined Telegram";
    if (t == "BasicGroupChatCreate" || t == "SupergroupChatCreate") return "created \"" + c.value("title").toString() + "\"";
    if (t == "ScreenshotTaken") return "took a screenshot";
    if (t == "CustomServiceAction") return c.value("text").toString();
    if (t == "Unsupported") return "Message not supported by this app yet";
    QString words;                                        // "PaidMedia" -> "Paid media"
    for (int i = 0; i < t.size(); i++) { if (i && t.at(i).isUpper()) words += ' '; words += i ? t.at(i).toLower() : t.at(i); }
    return words;
}

QString Store::preview(const M &m, bool withSender) {
    if (m.isEmpty()) return "";
    M c = sub(m, "content");
    QString s = contentSummary(c);
    if (kindOf(c) == "call") {
        bool out = m.value("is_outgoing").toBool(); int d = callSeconds(c);
        return callText(c, out) + (d > 0 ? " (" + clockLen(d) + ")" : QString());
    }
    if (kindOf(c) == "service") return senderName(sub(m, "sender_id")) + " " + s;
    if (!withSender) return s;
    if (m.value("is_outgoing").toBool()) return "You: " + s;
    M sender = sub(m, "sender_id");
    if (T(sender) == "messageSenderUser") return m_users.value(I(sender.value("user_id"))).value("first_name").toString() + ": " + s;
    return s;
}

// ---------------------------------------------------------------- files
bool Store::filePath(const M &file, QString *path, bool wanted, qint64 forChat, qint64 forMsg) {
    int id = file.value("id").toInt();
    if (!id) return false;
    M f = m_files.contains(id) ? m_files.value(id) : file, local = sub(f, "local");
    if (local.value("is_downloading_completed").toBool()) { *path = "file://" + local.value("path").toString(); return true; }
    if (forChat) m_fileChats[id].insert(forChat);
    if (forMsg) m_fileMsgs[id].insert(forMsg);
    if (wanted && !m_asked.contains(id) && local.value("can_be_downloaded").toBool()) downloadFile(id);
    return false;
}
void Store::downloadFile(int id) {
    if (m_asked.contains(id)) return;
    m_asked.insert(id);
    M r = req("downloadFile"); r["file_id"] = id; r["priority"] = 8; r["offset"] = 0; r["limit"] = 0; r["synchronous"] = false;
    send(r, "q");
}

// ---------------------------------------------------------------- rows for the screens

QVariantMap Store::chatRow(qint64 id) {
    M c = m_chats.value(id), r, last = sub(c, "last_message");
    QString t = T(sub(c, "type"));
    bool saved = id == m_myId && t == "chatTypePrivate";
    QString title = saved ? QString("Saved Messages") : c.value("title").toString();
    if (title.isEmpty()) title = "Deleted account";
    r["id"] = id; r["title"] = title; r["initials"] = saved ? QString("★") : initials(title);
    qint64 a = id < 0 ? -id : id;
    r["color"] = QString(palette[a % 7]);
    QString photo;
    if (!saved) filePath(sub(sub(c, "photo"), "small"), &photo, true, id);
    r["photo"] = photo;
    r["kind"] = saved ? "saved" : isChannel(c) ? "channel" : isGroup(c) ? "group" : T(sub(m_users.value(id), "type")) == "userTypeBot" ? "bot" : "user";
    bool secret = T(sub(c, "type")) == "chatTypeSecret";
    r["secret"] = secret;
    if (secret) {
        QString st = T(sub(m_secretChats.value(sub(c, "type").value("secret_chat_id").toInt()), "state"));
        r["secretState"] = st == "secretChatStateReady" ? "ready" : st == "secretChatStateClosed" ? "closed" : "pending";
    }
    M draft = sub(c, "draft_message");
    QString dtext = sub(sub(draft, "content"), "text").value("text").toString();
    r["draft"] = !dtext.isEmpty();
    r["typing"] = m_listTyping.contains(id) ? m_listTyping.value(id).first : QString();
    r["preview"] = !dtext.isEmpty() ? dtext.replace('\n', ' ') : preview(last, isGroup(c));
    r["date"] = last.isEmpty() ? QString() : listDate(last.value("date").toUInt());
    r["unread"] = c.value("unread_count").toInt();
    r["marked"] = c.value("is_marked_as_unread").toBool();
    r["mentions"] = c.value("unread_mention_count").toInt() > 0;
    r["muted"] = isMuted(c);
    bool pinned = false, archived = false;
    QVariantList pos = c.value("positions").toList();
    for (int i = 0; i < pos.size(); i++) {
        M p = pos.at(i).toMap();
        if (p.value("is_pinned").toBool()) pinned = true;
        if (T(sub(p, "list")) == "chatListArchive") archived = true;
    }
    r["pinned"] = pinned; r["archived"] = archived;
    QString state;
    if (last.value("is_outgoing").toBool() && !saved) {
        QString ss = T(sub(last, "sending_state"));
        state = ss == "messageSendingStatePending" ? "sending" : ss == "messageSendingStateFailed" ? "failed"
              : I(last.value("id")) <= I(c.value("last_read_outbox_message_id")) ? "read" : "sent";
    }
    r["state"] = state;
    return r;
}
QVariantMap Store::chatInfo(const QVariant &chatId) { return chatRow(I(chatId)); }

QVariantList Store::findChats(const QString &text) {
    QString q = text.trimmed();
    QVariantList out;
    QList<qint64> ids;
    for (QHash<qint64, M>::const_iterator i = m_chats.constBegin(); i != m_chats.constEnd(); ++i) {
        if (i.value().value("positions").toList().isEmpty()) continue;
        QString title = i.key() == m_myId ? QString("Saved Messages") : i.value().value("title").toString();
        if (q.isEmpty() || title.contains(q, Qt::CaseInsensitive)) ids << i.key();
    }
    for (int i = 0; i < ids.size() && out.size() < 60; i++) out << chatRow(ids.at(i));
    return out;
}

QVariantMap Store::messageRow(const M &m, const M *older) {
    M r, c = sub(m, "content"), chat = m_chats.value(I(m.value("chat_id")));
    qint64 id = I(m.value("id"));
    bool out = m.value("is_outgoing").toBool();
    QString kind = kindOf(c);
    uint date = m.value("date").toUInt();
    r["id"] = id; r["out"] = out; r["kind"] = kind; r["time"] = clock(date);
    r["edited"] = m.value("edit_date").toInt() > 0;
    r["unreadLine"] = !out && I(m.value("chat_id")) == m_openId && m_unreadFrom && id > m_unreadFrom && older && I(older->value("id")) <= m_unreadFrom;
    bool newDay = !older || QDateTime::fromTime_t(older->value("date").toUInt()).date() != QDateTime::fromTime_t(date).date();
    r["day"] = newDay ? dayName(date) : QString();
    QString state;
    if (out) {
        QString ss = T(sub(m, "sending_state"));
        state = ss == "messageSendingStatePending" ? "sending" : ss == "messageSendingStateFailed" ? "failed"
              : id <= I(chat.value("last_read_outbox_message_id")) ? "read" : "sent";
    }
    r["state"] = state;
    M sender = sub(m, "sender_id");
    qint64 sid = T(sender) == "messageSenderUser" ? I(sender.value("user_id")) : I(sender.value("chat_id"));
    bool showSender = false;
    if (isGroup(chat) && !out && kind != "service") {
        M os = older ? sub(*older, "sender_id") : M();
        qint64 oid = T(os) == "messageSenderUser" ? I(os.value("user_id")) : I(os.value("chat_id"));
        showSender = newDay || oid != sid || kindOf(sub(*older, "content")) == "service";
    }
    r["showSender"] = showSender;
    r["sender"] = senderName(sender);
    r["senderColor"] = QString(palette[(sid < 0 ? -sid : sid) % 7]);

    // forwarded / reply lines
    M fwd = sub(m, "forward_info");
    QString from;
    if (!fwd.isEmpty()) {
        M o = sub(fwd, "origin"); QString ot = T(o);
        from = ot == "messageOriginUser" ? userName(I(o.value("sender_user_id"))) : ot == "messageOriginHiddenUser" ? o.value("sender_name").toString()
             : m_chats.value(I(ot == "messageOriginChat" ? o.value("sender_chat_id") : o.value("chat_id"))).value("title").toString();
        from = "Forwarded from " + (from.isEmpty() ? QString("a channel") : from);
    }
    r["forward"] = from;
    M rt = sub(m, "reply_to");
    QString reply;
    if (T(rt) == "messageReplyToMessage") {
        M *orig = I(rt.value("chat_id")) == m_openId ? m_messages->find(I(rt.value("message_id"))) : 0;
        if (orig) reply = senderName(sub(*orig, "sender_id")) + ": " + contentSummary(sub(*orig, "content"));
        else if (!sub(rt, "content").isEmpty()) reply = contentSummary(sub(rt, "content"));
        else reply = "Reply to an earlier message";
    }
    r["reply"] = reply;

    // text
    M ft = kind == "text" ? sub(c, "text") : sub(c, "caption");
    QString plain = ft.value("text").toString();
    if (kind == "service") plain = (out ? QString("You") : senderName(sender)) + " " + contentSummary(c);
    else if (kind == "other") { plain = contentSummary(c); ft = M(); }
    else if (kind == "sticker") { plain = sub(c, "sticker").value("emoji").toString(); ft = M(); }
    r["hasText"] = !plain.isEmpty();
    r["text"] = ft.isEmpty() ? "<html>" + esc(plain) + "</html>" : html(ft);
    if (kind == "call") {
        int d = callSeconds(c);
        r["hasText"] = false;
        r["callTitle"] = callText(c, out);
        r["callSub"] = d > 0 ? clockLen(d) : QString();
        r["callBad"] = callBad(c);
    }

    // media
    QString image, anim; int w = 0, h = 0; M file; QString fileName; int duration = -1;
    if (kind == "photo") {
        QVariantList sizes = sub(c, "photo").value("sizes").toList();
        M best;
        for (int i = 0; i < sizes.size(); i++) {             // the largest one up to 1280 px
            M s = sizes.at(i).toMap();
            if (best.isEmpty() || s.value("width").toInt() <= 1280) best = s;
        }
        w = best.value("width").toInt(); h = best.value("height").toInt();
        filePath(sub(best, "photo"), &image, true, 0, id);
    } else if (kind == "video" || kind == "animation" || kind == "videonote") {
        M v = sub(c, kind == "video" ? "video" : kind == "animation" ? "animation" : "video_note");
        w = v.value(kind == "videonote" ? "length" : "width").toInt(); h = v.value(kind == "videonote" ? "length" : "height").toInt();
        duration = v.value("duration").toInt();
        M th = sub(v, "thumbnail"); QString fmt = T(sub(th, "format"));
        image = shownImage(sub(th, "file"), fmt == "thumbnailFormatWebp" ? "webp" : (fmt == "thumbnailFormatJpeg" || fmt == "thumbnailFormatPng") ? "jpeg" : "", id);
        file = sub(v, kind == "animation" ? "animation" : "video");
        fileName = v.value("file_name").toString();
    } else if (kind == "sticker") {
        M st = sub(c, "sticker"); QString fmt = T(sub(st, "format"));
        if (fmt == "stickerFormatWebp") image = shownImage(sub(st, "sticker"), "webp", id);
        else {                                                   // animated: the still thumbnail until the file is here
            M th = sub(st, "thumbnail"); QString tf = T(sub(th, "format"));
            image = shownImage(sub(th, "file"), tf == "thumbnailFormatWebp" ? "webp" : tf == "thumbnailFormatJpeg" ? "jpeg" : "", id);
            QString ap;
            if (filePath(sub(st, "sticker"), &ap, true, 0, id)) anim = ap.mid(7);
        }
        w = h = image.isEmpty() && anim.isEmpty() ? 0 : 340;
    } else if (kind == "voice") { M v = sub(c, "voice_note"); duration = v.value("duration").toInt(); file = sub(v, "voice"); fileName = "Voice message"; }
    else if (kind == "audio") {
        M v = sub(c, "audio"); duration = v.value("duration").toInt(); file = sub(v, "audio");
        fileName = v.value("title").toString().isEmpty() ? v.value("file_name").toString() : v.value("performer").toString() + " – " + v.value("title").toString();
    } else if (kind == "document") { M v = sub(c, "document"); file = sub(v, "document"); fileName = v.value("file_name").toString(); }
    if (w > 0 && h > 0 && kind != "sticker") {                // size on screen
        double k = qMin(860.0 / w, 860.0 / h);
        if (k > 3) k = 3;
        w = qMax(160, (int)(w * k)); h = qMax(120, (int)(h * k));
    }
    M ii = sub(m, "interaction_info");
    QVariantList reacts = sub(ii, "reactions").value("reactions").toList();
    QStringList rs;
    for (int i = 0; i < reacts.size() && i < 8; i++) {
        M x = reacts.at(i).toMap(), rt = sub(x, "type");
        QString e = T(rt) == "reactionTypeEmoji" ? rt.value("emoji").toString() : QString("★");
        rs << (x.value("is_chosen").toBool() ? "[" + e + " " + QString::number(x.value("total_count").toInt()) + "]" : e + " " + QString::number(x.value("total_count").toInt()));
    }
    r["reactions"] = rs.join("   ");
    int views = ii.value("view_count").toInt();
    r["views"] = views <= 0 ? QString() : views < 1000 ? QString::number(views) : views < 1000000 ? QString::number(views / 1000.0, 'f', 1) + "K" : QString::number(views / 1000000.0, 'f', 1) + "M";
    r["pinned"] = m.value("is_pinned").toBool();
    r["anim"] = anim;
    if (kind == "sticker" && (!image.isEmpty() || !anim.isEmpty())) r["hasText"] = false;
    M lp = sub(c, "link_preview");
    r["linkSite"] = lp.value("site_name").toString();
    r["linkTitle"] = lp.value("title").toString();
    r["linkText"] = sub(lp, "description").value("text").toString().left(300);
    r["hasLink"] = !lp.isEmpty() && !(lp.value("title").toString().isEmpty() && lp.value("site_name").toString().isEmpty());
    // poll
    QVariantList opts;
    if (T(c) == "messagePoll") {
        M poll = sub(c, "poll");
        bool quiz = T(sub(poll, "type")) == "pollTypeQuiz";
        QVariantList po = poll.value("options").toList();
        bool voted = false, closed = poll.value("is_closed").toBool();
        for (int i = 0; i < po.size(); i++) if (po.at(i).toMap().value("is_chosen").toBool()) voted = true;
        for (int i = 0; i < po.size(); i++) {
            M o = po.at(i).toMap(), x;
            x["text"] = sub(o, "text").value("text").toString();
            x["pct"] = o.value("vote_percentage").toInt();
            x["chosen"] = o.value("is_chosen").toBool();
            x["picked"] = (bool)m_pollPick.value(id).contains(i);
            opts << x;
        }
        int total = poll.value("total_voter_count").toInt();
        r["pollQuestion"] = sub(poll, "question").value("text").toString();
        r["pollShowResults"] = voted || closed;
        r["pollMulti"] = poll.value("allows_multiple_answers").toBool() && !quiz;
        r["pollPicked"] = m_pollPick.value(id).size();
        r["pollCanRetract"] = voted && !closed && !quiz;
        r["pollInfo"] = QString(closed ? "Final results · " : "") + (quiz ? "Quiz · " : poll.value("is_anonymous").toBool() ? "Anonymous poll · " : "Poll · ")
                        + (total == 1 ? QString("1 vote") : QString::number(total) + " votes");
        r["hasText"] = false;
    }
    r["pollOpts"] = opts;
    // bot buttons under the message
    QVariantList btns;
    M rm = sub(m, "reply_markup");
    if (T(rm) == "replyMarkupInlineKeyboard") {
        QVariantList rows = rm.value("rows").toList();
        for (int i = 0; i < rows.size(); i++) {
            QVariantList row = rows.at(i).toList();
            for (int k = 0; k < row.size(); k++) { M b = row.at(k).toMap(), x; x["text"] = b.value("text"); x["row"] = i; btns << x; }
        }
    }
    r["buttons"] = btns;
    r["image"] = image; r["imgW"] = w; r["imgH"] = h;
    r["duration"] = duration >= 0 ? durationText(duration) : QString();
    int fid = file.value("id").toInt();
    r["fileId"] = fid; r["fileName"] = fileName;
    QString fstate, fpath, fsize;
    if (fid) {
        M f = m_files.contains(fid) ? m_files.value(fid) : file, local = sub(f, "local");
        qint64 total = I(f.value("size")) ? I(f.value("size")) : I(f.value("expected_size"));
        fsize = sizeText(total);
        if (filePath(file, &fpath, false, 0, id)) fstate = "done";
        else if (local.value("is_downloading_active").toBool()) {
            fstate = "loading";
            if (total > 0) fsize = QString::number(100 * I(local.value("downloaded_size")) / total) + "% of " + fsize;
        } else fstate = "none";
    }
    r["fileState"] = fstate; r["filePath"] = fpath; r["fileSize"] = fsize;
    return r;
}

// ---------------------------------------------------------------- chat lists
void Store::loadMoreChats(const QString &key) {
    M r = req("loadChats"); r["chat_list"] = listOf(key); r["limit"] = 40;
    send(r, "chats:" + key);
}

// ---------------------------------------------------------------- the open chat
void Store::refreshOpenChat() {
    if (!m_openId) { m_chat = M(); Q_EMIT chatChanged(); return; }
    M c = m_chats.value(m_openId), r = chatRow(m_openId), type = sub(c, "type");
    QString t = T(type), subtitle; bool canSend = true;
    if (t == "chatTypePrivate" || t == "chatTypeSecret") subtitle = m_openId == m_myId ? QString() : statusText(m_users.value(I(type.value("user_id"))));
    if (t == "chatTypeSecret") {
        QString st = r.value("secretState").toString();
        if (st == "pending") { subtitle = "Waiting for " + userName(I(type.value("user_id"))).section(' ', 0, 0) + " to come online…"; canSend = false; }
        else if (st == "closed") { subtitle = "Secret chat ended"; canSend = false; }
        else subtitle = "Secret chat · " + subtitle;
    }
    else if (t == "chatTypeBasicGroup") {
        M g = m_basicGroups.value(I(type.value("basic_group_id")));
        subtitle = QString::number(g.value("member_count").toInt()) + " members";
        canSend = sub(c, "permissions").value("can_send_basic_messages").toBool() || T(sub(g, "status")) == "chatMemberStatusCreator" || T(sub(g, "status")) == "chatMemberStatusAdministrator";
    } else {
        M g = m_supergroups.value(I(type.value("supergroup_id")));
        int n = g.value("member_count").toInt(); QString st = T(sub(g, "status"));
        bool admin = st == "chatMemberStatusCreator" || st == "chatMemberStatusAdministrator";
        if (isChannel(c)) { subtitle = n ? QString::number(n) + " subscribers" : QString("channel"); canSend = admin; }
        else { subtitle = n ? QString::number(n) + " members" : QString("group"); canSend = admin || sub(c, "permissions").value("can_send_basic_messages").toBool(); }
        if (st == "chatMemberStatusLeft" || st == "chatMemberStatusBanned") canSend = false;
        r["canJoin"] = st == "chatMemberStatusLeft";
    }
    r["subtitle"] = m_typing.isEmpty() ? subtitle : m_typing;
    r["typing"] = !m_typing.isEmpty();
    r["canSend"] = canSend;
    if (t == "chatTypePrivate" || t == "chatTypeSecret") {
        qint64 uid = I(type.value("user_id")); M full = m_userFull.value(uid);
        r["callUser"] = uid;
        r["canCall"] = uid != m_myId && T(sub(m_users.value(uid), "type")) == "userTypeRegular" && (full.isEmpty() || full.value("can_be_called").toBool());
    } else r["canCall"] = false;
    r["pinnedId"] = I(m_pinned.value("id"));
    r["pinnedText"] = m_pinned.isEmpty() ? QString() : contentSummary(sub(m_pinned, "content"));
    r["draftText"] = sub(sub(sub(c, "draft_message"), "content"), "text").value("text").toString();
    r["muted"] = isMuted(c);
    m_chat = r;
    Q_EMIT chatChanged();
}

void Store::openChat(const QVariant &chatId) {
    qint64 id = I(chatId);
    if (m_openId == id) return;
    if (m_openId) closeChat();
    m_openId = id; m_histSerial++; m_historyEnd = false; m_loadingHistory = false; m_typing.clear(); m_pinned = M();
    if (m_hasNewer) { m_hasNewer = false; Q_EMIT hasNewerChanged(); }
    m_fileMsgs.clear();
    m_messages->reset();
    refreshOpenChat();
    tellService();
    if (m_td->remote()) bb::platform::Notification::deleteFromInbox("chat" + QString::number(id));
    M r = req("openChat"); r["chat_id"] = id; send(r, "q");
    M pr = req("getChatPinnedMessage"); pr["chat_id"] = id; send(pr, "pinned:" + QString::number(m_histSerial));
    m_keyboard.clear(); m_keyboardRaw.clear(); Q_EMIT keyboardChanged();
    loadKeyboard();
    M c = m_chats.value(id), last = sub(c, "last_message");
    if (m_pendingJump) { m_unreadFrom = 0; qint64 j = m_pendingJump; m_pendingJump = 0; jumpTo(j); return; }
    m_unreadFrom = c.value("unread_count").toInt() > 0 ? I(c.value("last_read_inbox_message_id")) : 0;
    if (m_unreadFrom && c.value("unread_count").toInt() > 3) {      // start at the first unread message
        m_hasNewer = true; Q_EMIT hasNewerChanged();
        m_loadingHistory = true; Q_EMIT loadingHistoryChanged();
        M r2 = req("getChatHistory"); r2["chat_id"] = id; r2["from_message_id"] = m_unreadFrom; r2["offset"] = -25; r2["limit"] = 50; r2["only_local"] = false;
        send(r2, "around:" + QString::number(m_histSerial) + ":" + QString::number(m_unreadFrom) + ":unread");
        return;
    }
    if (!last.isEmpty()) m_messages->addNew(last);
    loadOlder();
}
void Store::closeChat() {
    if (!m_openId) return;
    M r = req("closeChat"); r["chat_id"] = m_openId; send(r, "q");
    m_openId = 0; m_histSerial++; m_fileMsgs.clear();
    tellService();
    m_messages->reset();
    refreshOpenChat();
}
void Store::loadOlder() {
    if (!m_openId || m_loadingHistory || m_historyEnd) return;
    m_loadingHistory = true;
    Q_EMIT loadingHistoryChanged();
    M r = req("getChatHistory");
    r["chat_id"] = m_openId; r["from_message_id"] = m_messages->oldestId(); r["offset"] = 0; r["limit"] = 50; r["only_local"] = false;
    send(r, "hist:" + QString::number(m_histSerial));
}
void Store::viewOpenMessages() {
    if (!m_openId || !m_active || m_messages->size() == 0) return;
    M r = req("viewMessages"); r["chat_id"] = m_openId; r["message_ids"] = m_messages->ids(); r["force_read"] = false;
    send(r, "q");
}
void Store::buildCover() {
    // unread chats first (newest first), then the other latest chats; muted unread chats count as "other"
    ChatListModel *l = listFor("main");
    QMap<qint64, M> unread, other;                     // key: -date, so the newest comes first
    int unreadChats = 0, n = l->size();
    for (int i = 0; i < n; i++) {
        M c = l->data(QVariantList() << i).toMap();
        qint64 date = sub(m_chats.value(I(c.value("id"))), "last_message").value("date").toLongLong();
        M r; r["title"] = c.value("title"); r["text"] = c.value("preview"); r["unread"] = c.value("unread"); r["muted"] = c.value("muted");
        bool un = c.value("unread").toInt() > 0 && !c.value("muted").toBool();
        if (un) unreadChats++;
        (un ? unread : other).insertMulti(-date, r);
    }
    QVariantList rows;
    for (QMap<qint64, M>::const_iterator i = unread.constBegin(); i != unread.constEnd() && rows.size() < 3; ++i) rows << i.value();
    for (QMap<qint64, M>::const_iterator i = other.constBegin(); i != other.constEnd() && rows.size() < 3; ++i) rows << i.value();
    M cv; cv["rows"] = rows; cv["unread"] = m_unread;
    cv["line"] = m_authState != "ready" ? QString("Not signed in") : m_unread == 0 ? QString("No unread messages")
               : QString::number(m_unread) + (m_unread == 1 ? " unread message" : " unread messages") + (unreadChats > 1 ? " in " + QString::number(unreadChats) + " chats" : QString());
    m_cover = cv;
    Q_EMIT coverChanged();
}
void Store::setActive(bool on) {
    if (m_active == on) return;
    m_active = on;
    if (!on) buildCover();
    if (on) { bb::platform::Notification::clearEffectsForAll(); viewOpenMessages(); }
    tellService();
    Q_EMIT activeChanged();
}

static M textContent(const QString &text) {
    M ft = req("formattedText"); ft["text"] = text;
    M c = req("inputMessageText"); c["text"] = ft; c["clear_draft"] = true;
    return c;
}
void Store::sendText(const QString &text, const QVariant &replyToId) {
    QString s = text.trimmed();
    if (!m_openId || s.isEmpty()) return;
    M r = req("sendMessage"); r["chat_id"] = m_openId;
    if (I(replyToId)) { M rt = req("inputMessageReplyToMessage"); rt["message_id"] = I(replyToId); r["reply_to"] = rt; }
    r["input_message_content"] = textContent(s);
    send(r, "send");
}
void Store::editText(const QVariant &messageId, const QString &text) {
    if (!m_openId || text.trimmed().isEmpty()) return;
    M r = req("editMessageText"); r["chat_id"] = m_openId; r["message_id"] = I(messageId); r["input_message_content"] = textContent(text.trimmed());
    send(r, "edit");
}
void Store::deleteMessage(const QVariant &messageId, bool forEveryone) {
    if (!m_openId) return;
    M r = req("deleteMessages"); r["chat_id"] = m_openId; r["message_ids"] = QVariantList() << I(messageId); r["revoke"] = forEveryone;
    send(r, "delete");
}
void Store::forwardMessage(const QVariant &messageId, const QVariant &toChatId) {
    if (!m_openId) return;
    M r = req("forwardMessages"); r["chat_id"] = I(toChatId); r["from_chat_id"] = m_openId; r["message_ids"] = QVariantList() << I(messageId);
    r["send_copy"] = false; r["remove_caption"] = false;
    send(r, "forward");
}
void Store::sendFile(const QString &path, const QString &caption, bool asPhoto) {
    if (!m_openId) return;
    QString p = path; if (p.startsWith("file://")) p = p.mid(7);
    QString ext = p.section('.', -1).toLower();
    M f = req("inputFileLocal"); f["path"] = p;
    M cap = req("formattedText"); cap["text"] = caption;
    M c;
    if (asPhoto && (ext == "jpg" || ext == "jpeg" || ext == "png")) {
        M ph = req("inputPhoto"); ph["photo"] = f;
        c = req("inputMessagePhoto"); c["photo"] = ph;
    } else if (asPhoto && (ext == "mp4" || ext == "mov" || ext == "3gp" || ext == "m4v")) {
        M v = req("inputVideo"); v["video"] = f; v["supports_streaming"] = true;
        c = req("inputMessageVideo"); c["video"] = v;
    } else {
        M d = req("inputDocument"); d["document"] = f;
        c = req("inputMessageDocument"); c["document"] = d;
    }
    c["caption"] = cap;
    M r = req("sendMessage"); r["chat_id"] = m_openId; r["input_message_content"] = c;
    send(r, "send");
}
QVariantMap Store::message(const QVariant &messageId) {
    int i; M *m = m_messages->find(I(messageId), &i);
    if (!m) return M();
    M r = messageRow(*m, 0), c = sub(*m, "content");
    r["plain"] = (kindOf(c) == "text" ? sub(c, "text") : sub(c, "caption")).value("text").toString();
    r["canEdit"] = m->value("is_outgoing").toBool() && kindOf(c) == "text";
    return r;
}
QString Store::plainText(const QVariant &messageId) { return message(messageId).value("plain").toString(); }
void Store::copyText(const QVariant &messageId) {
    QString s = plainText(messageId);
    if (s.isEmpty()) return;
    bb::system::Clipboard clip;
    clip.clear();
    clip.insert("text/plain", s.toUtf8());
    Q_EMIT toast("Copied");
}

// ---------------------------------------------------------------- chat actions
void Store::setMuted(const QVariant &chatId, bool muted) {
    qint64 id = I(chatId);
    M n = sub(m_chats.value(id), "notification_settings");
    if (n.isEmpty()) return;
    n["use_default_mute_for"] = false; n["mute_for"] = muted ? 400 * 86400 : 0;
    M r = req("setChatNotificationSettings"); r["chat_id"] = id; r["notification_settings"] = n;
    send(r, "mute");
}
void Store::setPinned(const QVariant &chatId, bool pinned) {
    qint64 id = I(chatId);
    bool archived = chatRow(id).value("archived").toBool();
    M r = req("toggleChatIsPinned"); r["chat_list"] = req(archived ? "chatListArchive" : "chatListMain"); r["chat_id"] = id; r["is_pinned"] = pinned;
    send(r, "pin");
}
void Store::setArchived(const QVariant &chatId, bool archived) {
    M r = req("addChatToList"); r["chat_id"] = I(chatId); r["chat_list"] = req(archived ? "chatListArchive" : "chatListMain");
    send(r, "archive");
}
void Store::markRead(const QVariant &chatId) {
    qint64 id = I(chatId);
    M last = sub(m_chats.value(id), "last_message");
    if (!last.isEmpty()) {
        M r = req("viewMessages"); r["chat_id"] = id; r["message_ids"] = QVariantList() << I(last.value("id")); r["force_read"] = true;
        send(r, "q");
    }
    M r = req("toggleChatIsMarkedAsUnread"); r["chat_id"] = id; r["is_marked_as_unread"] = false;
    send(r, "q");
}
void Store::deleteChat(const QVariant &chatId) {
    qint64 id = I(chatId);
    QString t = T(sub(m_chats.value(id), "type"));
    if (t == "chatTypePrivate" || t == "chatTypeSecret") {
        M r = req("deleteChatHistory"); r["chat_id"] = id; r["remove_from_chat_list"] = true; r["revoke"] = false;
        send(r, "deletechat");
    } else { M r = req("leaveChat"); r["chat_id"] = id; send(r, "deletechat"); }
}

// ---------------------------------------------------------------- notifications
void Store::notify(const M &m) {
    if (m.value("is_outgoing").toBool() || m_demo || m_td->remote()) return;
    qint64 chatId = I(m.value("chat_id"));
    if (m_active && chatId == m_openId) return;
    uint now = QDateTime::currentDateTime().toTime_t();
    if (m.value("date").toUInt() + 120 < now) return;          // old messages arriving while catching up
    M c = m_chats.value(chatId);
    if (c.isEmpty() || isMuted(c) || m_active) return;        // in front: the chat list itself shows it
    bb::platform::Notification n;
    QFile lf(m_dataDir + "/lock.json");                       // app lock with hidden text: no names, no text
    M lock = lf.open(QIODevice::ReadOnly) ? TdClient::fromJson(lf.readAll().constData()).toMap() : M();
    if (lock.value("on").toBool() && (!lock.contains("hideText") || lock.value("hideText").toBool())) { n.setTitle("Gram"); n.setBody("New message"); }
    else { n.setTitle(c.value("title").toString()); n.setBody(preview(m, isGroup(c))); }
    n.notify();
}

// ---------------------------------------------------------------- stage 2: search, jumps, drafts, reactions
QVariantMap Store::foundRow(const M &m) {
    M r; qint64 chatId = I(m.value("chat_id"));
    M c = m_chats.value(chatId);
    r["type"] = "message"; r["chatId"] = chatId; r["msgId"] = I(m.value("id"));
    r["title"] = chatId == m_myId ? QString("Saved Messages") : c.value("title").toString();
    r["preview"] = preview(m, true);
    r["date"] = listDate(m.value("date").toUInt());
    r["sender"] = senderName(sub(m, "sender_id"));
    return r;
}
void Store::searchAll(const QString &text) {
    m_searchSerial++;
    QString q = text.trimmed();
    Q_EMIT searchDone("chats", findChats(q));
    if (q.size() < 2 || m_demo) { Q_EMIT searchDone("public", QVariantList()); Q_EMIT searchDone("messages", QVariantList()); return; }
    QString tag = "s:" + QString::number(m_searchSerial) + ":";
    M p = req("searchPublicChats"); p["query"] = q; send(p, tag + "public");
    M m = req("searchMessages"); m["chat_list"] = req("chatListMain"); m["query"] = q; m["offset"] = ""; m["limit"] = 40; m["min_date"] = 0; m["max_date"] = 0;
    send(m, tag + "messages");
}
void Store::searchInChat(const QString &text) {
    m_searchSerial++;
    if (!m_openId || text.trimmed().isEmpty()) { Q_EMIT searchDone("inchat", QVariantList()); return; }
    M r = req("searchChatMessages"); r["chat_id"] = m_openId; r["query"] = text.trimmed(); r["from_message_id"] = 0; r["offset"] = 0; r["limit"] = 60;
    send(r, "s:" + QString::number(m_searchSerial) + ":inchat");
}
void Store::reloadLatest() {
    m_histSerial++; m_historyEnd = false; m_loadingHistory = false;
    if (m_hasNewer) { m_hasNewer = false; Q_EMIT hasNewerChanged(); }
    m_messages->reset();
    M last = sub(m_chats.value(m_openId), "last_message");
    if (!last.isEmpty()) m_messages->addNew(last);
    loadOlder();
}
void Store::jumpTo(const QVariant &messageId) {
    qint64 id = I(messageId);
    if (!m_openId || !id) return;
    int i = m_messages->indexOf(id);
    if (i >= 0) { Q_EMIT scrollTo(i); return; }
    m_histSerial++; m_historyEnd = false;
    m_messages->reset();
    m_hasNewer = true; Q_EMIT hasNewerChanged();
    m_loadingHistory = true; Q_EMIT loadingHistoryChanged();
    M r = req("getChatHistory"); r["chat_id"] = m_openId; r["from_message_id"] = id; r["offset"] = -20; r["limit"] = 45; r["only_local"] = false;
    send(r, "around:" + QString::number(m_histSerial) + ":" + QString::number(id));
}
void Store::loadNewer() {
    if (!m_openId || !m_hasNewer || m_loadingHistory) return;
    m_loadingHistory = true; Q_EMIT loadingHistoryChanged();
    M r = req("getChatHistory"); r["chat_id"] = m_openId; r["from_message_id"] = m_messages->newestId(); r["offset"] = -49; r["limit"] = 50; r["only_local"] = false;
    send(r, "newer:" + QString::number(m_histSerial));
}
void Store::toLatest() { if (m_hasNewer) reloadLatest(); Q_EMIT scrollTo(0); }
void Store::typing() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_openId || now - m_lastTyping < 5000) return;
    m_lastTyping = now;
    M r = req("sendChatAction"); r["chat_id"] = m_openId; r["action"] = req("chatActionTyping");
    send(r, "q");
}
void Store::saveDraft(const QString &text) {
    if (!m_openId) return;
    QString old = sub(sub(sub(m_chats.value(m_openId), "draft_message"), "content"), "text").value("text").toString();
    if (old == text) return;
    M r = req("setChatDraftMessage"); r["chat_id"] = m_openId;
    if (!text.trimmed().isEmpty()) {
        M ft = req("formattedText"); ft["text"] = text;
        M c = req("draftMessageContentText"); c["text"] = ft;
        M d = req("draftMessage"); d["content"] = c;
        r["draft_message"] = d;
    }
    m_lastTyping = 0;
    send(r, "q");
}
void Store::react(const QVariant &messageId, const QString &emoji) {
    M *m = m_openId ? m_messages->find(I(messageId)) : 0;
    if (!m) return;
    bool chosen = false;
    QVariantList l = sub(sub(*m, "interaction_info"), "reactions").value("reactions").toList();
    for (int i = 0; i < l.size(); i++) {
        M x = l.at(i).toMap();
        if (x.value("is_chosen").toBool() && sub(x, "type").value("emoji").toString() == emoji) chosen = true;
    }
    M rt = req("reactionTypeEmoji"); rt["emoji"] = emoji;
    M r = req(chosen ? "removeMessageReaction" : "addMessageReaction");
    r["chat_id"] = m_openId; r["message_id"] = I(messageId); r["reaction_type"] = rt;
    if (!chosen) { r["is_big"] = false; r["update_recent_reactions"] = true; }
    send(r, "react");
}
void Store::loadStickers() {
    M r = req("getInstalledStickerSets"); r["sticker_type"] = req("stickerTypeRegular");
    send(r, "stsets");
    showStickerSet(m_stickerSetId);
}
void Store::showStickerSet(const QVariant &setId) {
    m_stickerSetId = I(setId);
    m_stickerRows.clear(); m_stickerRaw.clear(); m_stickerFiles.clear();
    Q_EMIT stickersChanged();
    if (m_stickerSetId) { M r = req("getStickerSet"); r["set_id"] = m_stickerSetId; send(r, "stset:" + QString::number(m_stickerSetId)); }
    else { M r = req("getRecentStickers"); r["is_attached"] = false; send(r, "stset:0"); }
}
void Store::loadGifs() { send(req("getSavedAnimations"), "gifs"); }
void Store::sendGif(int index) {
    if (!m_openId || index < 0 || index >= m_gifRaw.size()) return;
    M a = m_gifRaw.at(index).toMap(), f = req("inputFileId"), ia = req("inputAnimation"), c = req("inputMessageAnimation");
    f["id"] = sub(a, "animation").value("id"); ia["animation"] = f;
    ia["duration"] = a.value("duration"); ia["width"] = a.value("width"); ia["height"] = a.value("height");
    c["animation"] = ia;
    M r = req("sendMessage"); r["chat_id"] = m_openId; r["input_message_content"] = c;
    send(r, "send");
}
void Store::rebuildStickers() {
    m_gifRows.clear();
    for (int i = 0; i < m_gifRaw.size(); i++) {
        M a = m_gifRaw.at(i).toMap(), th = sub(a, "thumbnail"), row;
        QString tf = T(sub(th, "format"));
        QString img = shownImage(sub(th, "file"), tf == "thumbnailFormatJpeg" || tf == "thumbnailFormatPng" ? "jpeg" : tf == "thumbnailFormatWebp" ? "webp" : "", 0);
        if (img.isEmpty()) m_stickerFiles.insert(sub(th, "file").value("id").toInt());
        row["image"] = img; m_gifRows << row;
    }
    m_stickerRows.clear();
    for (int i = 0; i < m_stickerRaw.size(); i++) {
        M st = m_stickerRaw.at(i).toMap(), row, file;
        QString format;
        if (T(sub(st, "format")) == "stickerFormatWebp") { file = sub(st, "sticker"); format = "webp"; }
        else { M th = sub(st, "thumbnail"); QString tf = T(sub(th, "format")); file = sub(th, "file");
               format = tf == "thumbnailFormatWebp" ? "webp" : tf == "thumbnailFormatJpeg" || tf == "thumbnailFormatPng" ? "jpeg" : ""; }
        QString img = shownImage(file, format, 0);
        if (img.isEmpty()) m_stickerFiles.insert(file.value("id").toInt());
        row["image"] = img; row["emoji"] = st.value("emoji");
        m_stickerRows << row;
    }
    Q_EMIT stickersChanged();
}
void Store::sendSticker(int index) {
    if (!m_openId || index < 0 || index >= m_stickerRaw.size()) return;
    M st = m_stickerRaw.at(index).toMap(), f = req("inputFileId"), is = req("inputSticker"), c = req("inputMessageSticker");
    f["id"] = sub(st, "sticker").value("id"); is["sticker"] = f; is["width"] = st.value("width"); is["height"] = st.value("height");
    c["sticker"] = is; c["emoji"] = st.value("emoji");
    M r = req("sendMessage"); r["chat_id"] = m_openId; r["input_message_content"] = c;
    send(r, "send");
}
void Store::loadKeyboard() {
    qint64 mid = I(m_chats.value(m_openId).value("reply_markup_message_id"));
    if (!mid) { if (!m_keyboard.isEmpty()) { m_keyboard.clear(); m_keyboardRaw.clear(); Q_EMIT keyboardChanged(); } return; }
    M r = req("getMessage"); r["chat_id"] = m_openId; r["message_id"] = mid;
    send(r, "kbd:" + QString::number(m_openId));
}
void Store::pressKey(int row, int col) {
    M b = m_keyboardRaw.value(row).toList().value(col).toMap();
    QString t = T(sub(b, "type"));
    if (b.isEmpty()) return;
    if (t == "keyboardButtonTypeText") sendText(b.value("text").toString(), 0);
    else Q_EMIT toast("Gram can't do this button yet");
}
QString Store::recordPath() {
    QDir().mkpath(m_dataDir + "/cache");
    return "file://" + m_dataDir + "/cache/rec-" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".m4a";
}
void Store::discardFile(const QString &url) { QFile::remove(url.startsWith("file://") ? url.mid(7) : url); }
void Store::sendVoice(const QString &url, int seconds) {
    if (!m_openId) return;
    M f = req("inputFileLocal"); f["path"] = url.startsWith("file://") ? url.mid(7) : url;
    M v = req("inputVoiceNote"); v["voice_note"] = f; v["duration"] = seconds; v["waveform"] = QString();
    M c = req("inputMessageVoiceNote"); c["voice_note"] = v;
    M r = req("sendMessage"); r["chat_id"] = m_openId; r["input_message_content"] = c;
    send(r, "send");
}
void Store::createChat(const QString &title, const QVariantList &userIds, bool channel) {
    if (!channel && !userIds.isEmpty()) {
        QVariantList ids; for (int i = 0; i < userIds.size(); i++) ids << I(userIds.at(i));
        M r = req("createNewBasicGroupChat"); r["user_ids"] = ids; r["title"] = title; r["message_auto_delete_time"] = 0;
        send(r, "created");
    } else {
        M r = req("createNewSupergroupChat"); r["title"] = title; r["is_forum"] = false; r["is_channel"] = channel;
        r["description"] = QString(); r["message_auto_delete_time"] = 0; r["for_import"] = false;
        send(r, "created");
    }
}
void Store::loadPrivacy() {
    for (int i = 0; privacyKeys[i][0]; i++) {
        M r = req("getUserPrivacySettingRules"); r["setting"] = req(privacyKeys[i][1]);
        send(r, QString("privacy:") + privacyKeys[i][0]);
    }
}
void Store::setPrivacy(const QString &key, int level) {
    const char *type = 0;
    for (int i = 0; privacyKeys[i][0]; i++) if (key == privacyKeys[i][0]) type = privacyKeys[i][1];
    if (!type) return;
    QVariantList old = m_privacyRules.value(key).toList(), rules;
    for (int i = 0; i < old.size(); i++) {                   // keep the exceptions (people and groups)
        QString t = T(old.at(i).toMap());
        if (t.endsWith("Users") || t.endsWith("ChatMembers")) rules << old.at(i);
    }
    rules << QVariant(req(level == 0 ? "userPrivacySettingRuleAllowAll" : level == 1 ? "userPrivacySettingRuleAllowContacts" : "userPrivacySettingRuleRestrictAll"));
    M rs = req("userPrivacySettingRules"); rs["rules"] = rules;
    M r = req("setUserPrivacySettingRules"); r["setting"] = req(type); r["rules"] = rs;
    send(r, "q");
    M ru = req("updateUserPrivacySettingRules"); ru["setting"] = req(type); ru["rules"] = rs;   // show it at once
    onUpdate(ru);
}
void Store::resendCode() {
    M rr = req("resendCodeReasonUserRequest");
    M r = req("resendAuthenticationCode"); r["reason"] = rr;
    send(r, "resend");
}
void Store::votePoll(const QVariant &messageId, int option) {
    qint64 id = I(messageId);
    M *m = m_openId ? m_messages->find(id) : 0;
    if (!m) return;
    M poll = sub(sub(*m, "content"), "poll");
    if (poll.value("allows_multiple_answers").toBool() && T(sub(poll, "type")) != "pollTypeQuiz") {
        QList<int> &l = m_pollPick[id];
        if (l.contains(option)) l.removeAll(option); else l << option;
        m_messages->touch(id);
        return;
    }
    M r = req("setPollAnswer"); r["chat_id"] = m_openId; r["message_id"] = id; r["option_ids"] = QVariantList() << option;
    send(r, "vote");
}
void Store::sendVote(const QVariant &messageId) {
    qint64 id = I(messageId);
    QVariantList o; QList<int> l = m_pollPick.take(id);
    qSort(l);
    for (int i = 0; i < l.size(); i++) o << l.at(i);
    if (o.isEmpty() || !m_openId) return;
    M r = req("setPollAnswer"); r["chat_id"] = m_openId; r["message_id"] = id; r["option_ids"] = o;
    send(r, "vote");
}
void Store::retractVote(const QVariant &messageId) {
    if (!m_openId) return;
    M r = req("setPollAnswer"); r["chat_id"] = m_openId; r["message_id"] = I(messageId); r["option_ids"] = QVariantList();
    send(r, "vote");
}
void Store::pressButton(const QVariant &messageId, int index) {
    M *m = m_openId ? m_messages->find(I(messageId)) : 0;
    if (!m) return;
    QVariantList rows = sub(sub(*m, "reply_markup"), "rows").value("rows").toList();
    if (rows.isEmpty()) rows = sub(*m, "reply_markup").value("rows").toList();
    int n = 0;
    for (int i = 0; i < rows.size(); i++) {
        QVariantList row = rows.at(i).toList();
        for (int k = 0; k < row.size(); k++, n++) {
            if (n != index) continue;
            M b = row.at(k).toMap(), bt = sub(b, "type");
            QString t = T(bt);
            if (t == "inlineKeyboardButtonTypeUrl" || t == "inlineKeyboardButtonTypeLoginUrl" || t == "inlineKeyboardButtonTypeWebApp") openLink(bt.value("url").toString());
            else if (t == "inlineKeyboardButtonTypeCallback") {
                M p = req("callbackQueryPayloadData"); p["data"] = bt.value("data");
                M r = req("getCallbackQueryAnswer"); r["chat_id"] = m_openId; r["message_id"] = I(messageId); r["payload"] = p;
                send(r, "callback");
            } else if (t == "inlineKeyboardButtonTypeCopyText") copyTextRaw(bt.value("text").toString());
            else if (t == "inlineKeyboardButtonTypeUser") openUserChat(bt.value("user_id"));
            else Q_EMIT toast("Gram can't do this button yet");
            return;
        }
    }
}
void Store::openLink(const QString &url) {
    if (url.isEmpty()) return;
    M r = req("getInternalLinkType"); r["link"] = url;
    send(r, "link:" + url);
}
void Store::sweepTyping() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<qint64> gone;
    for (QHash<qint64, QPair<QString, qint64> >::const_iterator i = m_listTyping.constBegin(); i != m_listTyping.constEnd(); ++i)
        if (i.value().second < now) gone << i.key();
    for (int i = 0; i < gone.size(); i++) { m_listTyping.remove(gone.at(i)); Q_FOREACH (ChatListModel *l, m_lists) l->changed(gone.at(i)); }
    if (m_listTyping.isEmpty()) m_sweep->stop();
}
void Store::pinMessage(const QVariant &messageId, bool pin) {
    if (!m_openId) return;
    M r = req(pin ? "pinChatMessage" : "unpinChatMessage"); r["chat_id"] = m_openId; r["message_id"] = I(messageId);
    if (pin) { r["disable_notification"] = true; r["only_for_self"] = false; }
    send(r, "pin");
}

// ---------------------------------------------------------------- stage 2: account, info pages
QVariantMap Store::me() {
    M u = m_users.value(m_myId), r;
    if (u.isEmpty()) return r;
    QString name = userName(m_myId), photo;
    filePath(sub(sub(u, "profile_photo"), "small"), &photo, true, m_myId);
    r["id"] = m_myId; r["name"] = name; r["initials"] = initials(name); r["photo"] = photo;
    r["color"] = QString(palette[m_myId % 7]);
    r["phone"] = u.value("phone_number").toString().isEmpty() ? QString() : "+" + u.value("phone_number").toString();
    QStringList un = sub(u, "usernames").value("active_usernames").toStringList();
    r["username"] = un.isEmpty() ? QString() : "@" + un.first();
    r["first"] = u.value("first_name"); r["last"] = u.value("last_name");
    r["bio"] = sub(m_userFull.value(m_myId), "bio").value("text").toString();
    return r;
}
void Store::startSecretChat(const QVariant &userId) {
    M r = req("createNewSecretChat"); r["user_id"] = I(userId);
    send(r, "openchat");
}
void Store::setMuteFor(const QVariant &chatId, int seconds) {
    qint64 id = I(chatId);
    M n = sub(m_chats.value(id), "notification_settings");
    if (n.isEmpty()) return;
    n["use_default_mute_for"] = false; n["mute_for"] = seconds;
    M r = req("setChatNotificationSettings"); r["chat_id"] = id; r["notification_settings"] = n;
    send(r, "mute");
}
void Store::removeMember(const QVariant &chatId, const QVariant &userId) {
    M who = req("messageSenderUser"); who["user_id"] = I(userId);
    M r = req("setChatMemberStatus"); r["chat_id"] = I(chatId); r["member_id"] = who; r["status"] = req("chatMemberStatusLeft");
    send(r, "removed");
}
void Store::loadMyInfo() { M r = req("getUserFullInfo"); r["user_id"] = m_myId; send(r, "mefull"); }
void Store::saveProfile(const QString &first, const QString &last, const QString &bio, const QString &username) {
    M me = this->me();
    if (first.trimmed().isEmpty()) { Q_EMIT toast("The first name can't be empty"); return; }
    if (first != me.value("first").toString() || last != me.value("last").toString()) { M r = req("setName"); r["first_name"] = first.trimmed(); r["last_name"] = last.trimmed(); send(r, "saved"); }
    if (bio != me.value("bio").toString()) { M r = req("setBio"); r["bio"] = bio; send(r, "savedbio"); }
    QString un = username.trimmed(); if (un.startsWith('@')) un = un.mid(1);
    QString old = me.value("username").toString().mid(1);
    if (un != old) { M r = req("setUsername"); r["username"] = un; send(r, "saved"); }
}
void Store::addMembers(const QVariant &chatId, const QVariantList &userIds) {
    QVariantList ids; for (int i = 0; i < userIds.size(); i++) ids << I(userIds.at(i));
    if (ids.isEmpty()) return;
    M r = req("addChatMembers"); r["chat_id"] = I(chatId); r["user_ids"] = ids;
    send(r, "added");
}
void Store::copyInviteLink(const QVariant &chatId) {
    QString link = m_profile.value("inviteLink").toString();
    if (!link.isEmpty()) { copyTextRaw(link); return; }
    M r = req("createChatInviteLink"); r["chat_id"] = I(chatId); r["name"] = QString(); r["expiration_date"] = 0; r["member_limit"] = 0; r["creates_join_request"] = false;
    send(r, "invite");
}
QVariantMap Store::notifyScopes() const {
    M r;
    r["private"] = m_scopeMute.value("notificationSettingsScopePrivateChats") == 0;
    r["groups"] = m_scopeMute.value("notificationSettingsScopeGroupChats") == 0;
    r["channels"] = m_scopeMute.value("notificationSettingsScopeChannelChats") == 0;
    return r;
}
void Store::setScopeOn(const QString &scope, bool on) {
    QString t = scope == "private" ? "notificationSettingsScopePrivateChats" : scope == "groups" ? "notificationSettingsScopeGroupChats" : "notificationSettingsScopeChannelChats";
    M n = m_scopeSettings.value(t);
    if (n.isEmpty()) n = req("scopeNotificationSettings");
    n["@type"] = "scopeNotificationSettings";
    n["mute_for"] = on ? 0 : 400 * 86400;
    M r = req("setScopeNotificationSettings"); r["scope"] = req(t.toLatin1().constData()); r["notification_settings"] = n;
    send(r, "scope");
}
void Store::loadSessions() { send(req("getActiveSessions"), "sessions"); }
void Store::endSession(const QVariant &sessionId) {
    M r = req("terminateSession"); r["session_id"] = sessionId.toString().toLongLong();
    send(r, "endsession");
    QTimer::singleShot(1500, this, SLOT(loadSessions()));
}
void Store::openUserChat(const QVariant &userId) {
    if (m_chats.contains(I(userId))) { Q_EMIT openChatRequested(I(userId)); return; }
    M r = req("createPrivateChat"); r["user_id"] = I(userId); r["force"] = false;
    send(r, "openchat");
}
QVariantMap Store::userRow(qint64 uid, const QString &extra) {
    M u = m_users.value(uid), r;
    QString name = userName(uid), photo;
    filePath(sub(sub(u, "profile_photo"), "small"), &photo, true, m_profileId);
    r["id"] = uid; r["title"] = uid == m_myId ? QString("You") : name; r["initials"] = initials(name); r["photo"] = photo;
    r["color"] = QString(palette[uid % 7]);
    r["preview"] = statusText(u); r["date"] = extra;
    r["kind"] = "user"; r["unread"] = 0; r["marked"] = false; r["mentions"] = false; r["muted"] = false; r["pinned"] = false;
    r["archived"] = false; r["state"] = ""; r["draft"] = false;
    return r;
}
void Store::openProfile(const QVariant &chatId) {
    m_profileId = I(chatId);
    m_profileFull = M(); m_profileMembers.clear();
    buildProfile();
    M type = sub(m_chats.value(m_profileId), "type"); QString t = T(type);
    QString tag = "full:" + QString::number(m_profileId);
    if (t == "chatTypePrivate" || t == "chatTypeSecret") { M r = req("getUserFullInfo"); r["user_id"] = I(type.value("user_id")); send(r, tag); }
    else if (t == "chatTypeBasicGroup") { M r = req("getBasicGroupFullInfo"); r["basic_group_id"] = I(type.value("basic_group_id")); send(r, tag); }
    else if (t == "chatTypeSupergroup") { M r = req("getSupergroupFullInfo"); r["supergroup_id"] = I(type.value("supergroup_id")); send(r, tag); }
}
void Store::buildProfile() {
    if (!m_profileId) { m_profile = M(); Q_EMIT profileChanged(); return; }
    M c = m_chats.value(m_profileId), type = sub(c, "type"), r = chatRow(m_profileId);
    QString t = T(type);
    bool isUser = t == "chatTypePrivate" || t == "chatTypeSecret";
    QString big;
    if (r.value("kind").toString() != "saved" && filePath(sub(sub(c, "photo"), "big"), &big, true, m_profileId)) r["photo"] = big;
    r["isUser"] = isUser; r["muted"] = isMuted(c);
    QVariantList rows; QString about, username, phone, status;
    if (isUser) {
        qint64 uid = I(type.value("user_id"));
        M u = m_users.value(uid), full = m_userFull.value(uid);
        r["userId"] = uid;
        r["canSecret"] = t == "chatTypePrivate" && uid != m_myId && T(sub(u, "type")) == "userTypeRegular";
        r["canCall"] = uid != m_myId && T(sub(u, "type")) == "userTypeRegular" && (full.isEmpty() || full.value("can_be_called").toBool());
        status = r.value("kind").toString() == "saved" ? QString() : statusText(u);
        about = sub(full, "bio").value("text").toString();
        if (about.isEmpty()) about = sub(full, "bot_info").value("description").toString();
        phone = u.value("phone_number").toString().isEmpty() ? QString() : "+" + u.value("phone_number").toString();
        QStringList un = sub(u, "usernames").value("active_usernames").toStringList();
        if (!un.isEmpty()) username = "@" + un.first();
    } else {
        about = m_profileFull.value("description").toString();
        int n = 0;
        if (t == "chatTypeBasicGroup") n = m_basicGroups.value(I(type.value("basic_group_id"))).value("member_count").toInt();
        else {
            M g = m_supergroups.value(I(type.value("supergroup_id")));
            n = g.value("member_count").toInt();
            QStringList un = sub(g, "usernames").value("active_usernames").toStringList();
            if (!un.isEmpty()) username = "@" + un.first();
        }
        status = QString::number(n) + (isChannel(c) ? " subscribers" : " members");
        QVariantList members = m_profileFull.value("members").toList();
        for (int i = 0; i < members.size() && i < 200; i++) {
            M mm = members.at(i).toMap(), mid = sub(mm, "member_id"); QString st = T(sub(mm, "status"));
            if (T(mid) != "messageSenderUser") continue;
            rows << userRow(I(mid.value("user_id")), st == "chatMemberStatusCreator" ? QString("owner") : st == "chatMemberStatusAdministrator" ? QString("admin") : QString());
        }
    }
    r["status"] = status; r["about"] = about; r["username"] = username; r["phone"] = phone; r["members"] = rows;
    if (!isUser) {
        QString st = t == "chatTypeBasicGroup" ? T(sub(m_basicGroups.value(I(type.value("basic_group_id"))), "status"))
                                               : T(sub(m_supergroups.value(I(type.value("supergroup_id"))), "status"));
        bool admin = st == "chatMemberStatusCreator" || st == "chatMemberStatusAdministrator";
        bool member = admin || st == "chatMemberStatusMember" || st == "chatMemberStatusRestricted";
        r["canAdd"] = member && (admin || sub(c, "permissions").value("can_invite_users").toBool()) && !isChannel(c);
        r["inviteLink"] = sub(m_profileFull, "invite_link").value("invite_link").toString();
        if (r.value("inviteLink").toString().isEmpty() && !username.isEmpty()) r["inviteLink"] = "https://t.me/" + username.mid(1);
        r["canLink"] = admin || !r.value("inviteLink").toString().isEmpty();
        r["canRemove"] = admin && !isChannel(c);
        // admin tools: what this account may do here
        M my = t == "chatTypeBasicGroup" ? sub(m_basicGroups.value(I(type.value("basic_group_id"))), "status")
                                         : sub(m_supergroups.value(I(type.value("supergroup_id"))), "status");
        bool owner = st == "chatMemberStatusCreator";
        M rights = sub(my, "rights");
        r["isOwner"] = owner; r["isAdmin"] = admin; r["isSuper"] = t == "chatTypeSupergroup"; r["isChannel"] = isChannel(c);
        r["canEditInfo"] = owner || rights.value("can_change_info").toBool() || (t == "chatTypeBasicGroup" && member && sub(c, "permissions").value("can_change_info").toBool());
        r["canRestrict"] = owner || rights.value("can_restrict_members").toBool();
        r["canPromote"] = owner || rights.value("can_promote_members").toBool();
        r["myRights"] = rights;
        r["permissions"] = sub(c, "permissions");
        r["slowMode"] = m_profileFull.value("slow_mode_delay").toInt();
        r["canLog"] = admin && t == "chatTypeSupergroup";
        r["description"] = m_profileFull.value("description").toString();
    }
    m_profile = r;
    Q_EMIT profileChanged();
}

void Store::copyTextRaw(const QString &text) {
    bb::system::Clipboard clip;
    clip.clear();
    clip.insert("text/plain", text.toUtf8());
    Q_EMIT toast("Copied " + text);
}
void Store::playAudio(const QVariant &messageId) {
    QVariantMap r = message(messageId);
    if (r.value("fileState").toString() != "done") return;
    QString path = r.value("filePath").toString().mid(7);
    QString ext = path.section('.', -1).toLower();
    if (r.value("kind").toString() == "voice" || ext == "ogg" || ext == "oga" || ext == "opus") {
        QString wav = m_dataDir + "/cache/" + QString::number(r.value("fileId").toInt()) + ".wav";
        if (!QFile::exists(wav)) {
            QDir().mkpath(m_dataDir + "/cache");
            if (opus_to_wav(QFile::encodeName(path).constData(), QFile::encodeName(wav).constData())) { Q_EMIT toast("This recording could not be read."); return; }
        }
        path = wav;
    }
    Q_EMIT playFile("file://" + path);
}
// A picture the screens can show: jpeg/png as they are, webp converted to png once (data/cache/<file id>.png).
QString Store::shownImage(const M &file, const QString &format, qint64 msgId) {
    QString path;
    if (format.isEmpty() || !filePath(file, &path, true, 0, msgId)) return QString();
    if (format != "webp") return path;
    QString png = m_dataDir + "/cache/" + QString::number(file.value("id").toInt()) + ".png";
    if (!QFile::exists(png)) {
        QDir().mkpath(m_dataDir + "/cache");
        if (webp_to_png(QFile::encodeName(path.mid(7)).constData(), QFile::encodeName(png).constData(), 512)) return QString();
    }
    return "file://" + png;
}

void Store::loadContacts() { send(req("getContacts"), "contacts"); }
void Store::joinChat(const QVariant &chatId) { M r = req("joinChat"); r["chat_id"] = I(chatId); send(r, "join"); }

void Store::tellService() {
    M c; c["@ui"] = "state"; c["active"] = m_active; c["chat"] = m_openId;
    m_td->control(c);
}
void Store::openFromNotification(qint64 chatId) {
    if (m_authState == "ready") Q_EMIT openChatRequested(chatId); else m_pendingOpen = chatId;
}
