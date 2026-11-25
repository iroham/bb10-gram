#include "calls.h"
#include "store.h"
#include "../port/voip.h"
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <QUrl>
#include <bb/multimedia/MediaPlayer>
#include <bb/multimedia/RepeatMode>
#include <bb/device/VibrationController>
#include <bb/platform/NotificationGlobalSettings>
#include <bb/platform/NotificationMode>
#include <QtSensors/QProximitySensor>

typedef QVariantMap M;
static inline qint64 I(const QVariant &v) { return v.toLongLong(); }
static inline QString T(const M &m) { return m.value("@type").toString(); }
static inline M sub(const M &m, const char *k) { return m.value(k).toMap(); }
static inline M req(const char *type) { M m; m["@type"] = QString(type); return m; }

static const char *RINGTONE = "/usr/share/sounds/notification-tones/phone/bbpro1.m4a";

Calls::Calls(Store *store, const QString &dataDir, QObject *parent)
    : QObject(parent), m_store(store), m_dataDir(dataDir), m_id(0), m_user(0), m_outgoing(false), m_muted(false), m_speaker(false),
      m_demo(false), m_bars(0), m_lastState(0), m_talked(0), m_media(0), m_ringer(0), m_vibra(0), m_prox(0), m_near(false) {
    m_poll = new QTimer(this); m_poll->setInterval(250);
    connect(m_poll, SIGNAL(timeout()), this, SLOT(poll()));
    m_tick = new QTimer(this); m_tick->setInterval(1000);
    connect(m_tick, SIGNAL(timeout()), this, SIGNAL(tick()));
    m_close = new QTimer(this); m_close->setSingleShot(true); m_close->setInterval(1800);
    connect(m_close, SIGNAL(timeout()), this, SLOT(finish()));
    m_vibraTimer = new QTimer(this); m_vibraTimer->setInterval(2500);
    connect(m_vibraTimer, SIGNAL(timeout()), this, SLOT(buzz()));
}
Calls::~Calls() { stopMedia(); ring(false); }

int Calls::seconds() const {
    if (m_phase == "ended") return m_talked;
    return m_talkStart.isValid() ? (int)(m_talkStart.secsTo(QDateTime::currentDateTime())) : 0;
}
QString Calls::durationText() const {
    int s = seconds();
    return s >= 3600 ? QString("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'))
                     : QString("%1:%2").arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
}

QVariantMap Calls::protocol() const {
    M p = req("callProtocol");
    p["udp_p2p"] = true; p["udp_reflector"] = true;
    p["min_layer"] = 65; p["max_layer"] = gv_max_layer();
    QVariantList v; v << QString("2.4.4");          // libtgvoip; the other side picks it from its own list
    p["library_versions"] = v;
    return p;
}

void Calls::setPhase(const QString &phase, const QString &status) {
    if (phase == m_phase && status == m_status) return;
    QString was = m_phase;
    m_phase = phase; m_status = status;
    if (phase == "talking" && was != "talking" && !m_talkStart.isValid()) { m_talkStart = QDateTime::currentDateTime(); m_tick->start(); }
    qDebug() << "call:" << phase << status;
    updateSensor();
    Q_EMIT changed();
}

// ---------------------------------------------------------------- user actions
void Calls::start(const QVariant &userId) {
    if (m_id) { Q_EMIT message("You are already in a call"); return; }
    qint64 uid = I(userId);
    if (!uid) return;
    reset();
    M card = m_store->userCard(uid);
    m_user = uid; m_outgoing = true; m_id = -1;     // -1 until TDLib gives the call id
    m_name = card.value("name").toString(); m_initials = card.value("initials").toString();
    m_color = card.value("color").toString(); m_photo = card.value("photo").toString();
    setPhase("requesting", "Requesting…");
    M r = req("createCall"); r["user_id"] = uid; r["protocol"] = protocol(); r["is_video"] = false;
    m_store->send(r, "call:create");
}
void Calls::accept() {
    if (m_id <= 0 || m_outgoing || m_phase != "ringing") return;
    ring(false);
    setPhase("exchanging", "Connecting…");
    M r = req("acceptCall"); r["call_id"] = m_id; r["protocol"] = protocol();
    m_store->send(r, "call:accept");
}
void Calls::hangUp() {
    if (!m_id) return;
    if (m_phase == "ended") { finish(); return; }
    ring(false);
    bool wasTalking = m_phase == "talking" || m_phase == "reconnecting";
    m_talked = seconds();
    if (m_id > 0) {
        M r = req("discardCall"); r["call_id"] = m_id; r["is_disconnected"] = false; r["duration"] = m_talked;
        r["is_video"] = false; r["connection_id"] = QString("0");
        m_store->send(r, "call:discard");
    }
    stopMedia();
    if (wasTalking) tone("end"); else tone("");
    setPhase("ended", m_outgoing && !wasTalking ? "Cancelled" : !m_outgoing && m_phase == "ringing" ? "Declined" : "Call ended");
    m_close->start();
    if (m_id < 0) m_id = -2;                         // createCall still on its way: discard when it answers
}
void Calls::setMuted(bool on) {
    m_muted = on;
    if (m_media) gv_mute(m_media, on);
    Q_EMIT changed();
}
void Calls::setSpeaker(bool on) {
    m_speaker = on;
    if (m_media) gv_speaker(m_media, on);
    updateSensor();
    Q_EMIT changed();
}
void Calls::dismiss() { finish(); }

// ---------------------------------------------------------------- TDLib
void Calls::onAnswer(const QString &tag, const QVariantMap &r) {
    bool error = T(r) == "error";
    if (tag == "call:create") {
        if (error) {
            QString msg = r.value("message").toString();
            QString text = msg.contains("PRIVACY") ? m_name + " doesn't accept calls" : msg.contains("PEER_FLOOD") || msg.contains("FLOOD")
                ? QString("Too many calls, try again later") : QString("Couldn't start the call");
            qDebug() << "call: create failed" << msg;
            Q_EMIT message(text);
            setPhase("ended", "Couldn't call");
            m_close->start();
            return;
        }
        qint64 id = I(r.value("id"));
        if (m_id == -2) {                            // hung up before the id arrived
            M d = req("discardCall"); d["call_id"] = id; d["is_disconnected"] = false; d["duration"] = 0; d["is_video"] = false; d["connection_id"] = QString("0");
            m_store->send(d, "call:discard");
            return;
        }
        if (m_id == -1) m_id = id;
    } else if (error && (tag == "call:accept")) {
        qDebug() << "call: accept failed" << r.value("message");
        Q_EMIT message("Couldn't answer the call");
        setPhase("ended", "Call failed"); m_close->start();
    }
}

void Calls::onUpdateCall(const QVariantMap &call) {
    qint64 id = I(call.value("id"));
    M st = sub(call, "state"); QString s = T(st);
    bool outgoing = call.value("is_outgoing").toBool();
    // another call while we are busy: decline it (one call at a time)
    if (m_id && id != m_id && !(m_id == -1 && outgoing)) {
        if (!outgoing && s == "callStatePending") {
            M d = req("discardCall"); d["call_id"] = id; d["is_disconnected"] = false; d["duration"] = 0; d["is_video"] = false; d["connection_id"] = QString("0");
            m_store->send(d, "call:discard");
        }
        return;
    }
    if (!m_id) {
        if (outgoing || s != "callStatePending") return;
        reset();
        m_id = id; m_user = I(call.value("user_id")); m_outgoing = false;
        M card = m_store->userCard(m_user);
        m_name = card.value("name").toString(); m_initials = card.value("initials").toString();
        m_color = card.value("color").toString(); m_photo = card.value("photo").toString();
        setPhase("ringing", call.value("is_video").toBool() ? "Video call (voice only on this phone)" : "Incoming call");
        ring(true);
        Q_EMIT ringing();
        return;
    }
    if (m_id == -1) m_id = id;
    if (m_phase == "ended" && s != "callStateDiscarded") return;
    if (s == "callStatePending") {
        if (outgoing) {
            if (st.value("is_received").toBool()) { if (m_phase != "ringing") { setPhase("ringing", "Ringing…"); tone("ringback"); } }
            else setPhase("requesting", "Requesting…");
        }
    } else if (s == "callStateExchangingKeys") {
        ring(false); tone("");
        setPhase("exchanging", "Exchanging encryption keys…");
    } else if (s == "callStateReady") {
        ring(false);
        if (!m_media) startMedia(st);
    } else if (s == "callStateHangingUp") {
        stopMedia();
        if (m_phase != "ended") setPhase("ended", "Ending call…");
    } else if (s == "callStateDiscarded") {
        bool talked = m_talkStart.isValid();
        if (m_phase != "ended") m_talked = seconds();
        stopMedia(); ring(false);
        QString reason = T(sub(st, "reason"));
        QString text = "Call ended"; QString t = talked ? "end" : "";
        if (reason == "callDiscardReasonDeclined") { text = outgoing ? "Line busy" : "Declined"; if (outgoing) t = "busy"; }
        else if (reason == "callDiscardReasonMissed") text = outgoing ? "No answer" : "Missed call";
        else if (reason == "callDiscardReasonDisconnected") { text = "Connection lost"; t = "end"; }
        if (m_phase != "ended") { tone(t); setPhase("ended", text); m_close->start(); }
        else if (m_status == "Ending call…") { setPhase("ended", text); m_close->start(); }
    } else if (s == "callStateError") {
        stopMedia(); ring(false); tone("");
        QString msg = sub(st, "error").value("message").toString();
        qDebug() << "call: error" << msg;
        setPhase("ended", msg.contains("PRIVACY") ? m_name + " doesn't accept calls" : QString("Call failed"));
        m_close->start();
    }
}

// ---------------------------------------------------------------- media
void Calls::startMedia(const QVariantMap &ready) {
    if (m_demo) { setPhase("talking", ""); return; }
    QVariantList servers = ready.value("servers").toList();
    QList<QByteArray> keep;                          // keeps the address strings alive for gv_start
    QVector<gv_endpoint> eps;
    for (int i = 0; i < servers.size(); i++) {
        M sv = servers.at(i).toMap(), type = sub(sv, "type");
        if (T(type) != "callServerTypeTelegramReflector") continue;
        gv_endpoint e; memset(&e, 0, sizeof e);
        e.id = I(sv.value("id"));
        keep << sv.value("ip_address").toString().toUtf8() << sv.value("ipv6_address").toString().toUtf8();
        e.ipv4 = keep.at(keep.size() - 2).constData(); e.ipv6 = keep.last().constData();
        e.port = sv.value("port").toInt();
        QByteArray tag = QByteArray::fromBase64(type.value("peer_tag").toString().toLatin1());
        memcpy(e.peer_tag, tag.constData(), qMin(16, tag.size()));
        e.tcp = type.value("is_tcp").toBool();
        eps << e;
    }
    QByteArray key = QByteArray::fromBase64(ready.value("encryption_key").toString().toLatin1());
    if (key.size() != 256 || eps.isEmpty()) {
        qDebug() << "call: bad ready state, key" << key.size() << "servers" << eps.size();
        hangUp(); setPhase("ended", "Call failed");
        return;
    }
    QStringList em; QVariantList el = ready.value("emojis").toList();
    for (int i = 0; i < el.size(); i++) em << el.at(i).toString();
    m_emojis = em.join(" ");
    int layer = sub(ready, "protocol").value("max_layer").toInt();
    QByteArray cfg = ready.value("config").toString().toUtf8();
    QByteArray log = (m_dataDir + "/voip.log").toUtf8();
    tone("");
    m_media = gv_start(cfg.constData(), (const unsigned char *)key.constData(), m_outgoing, eps.constData(), eps.size(),
                       ready.value("allow_p2p").toBool(), layer > 0 ? layer : gv_max_layer(), 0, log.constData());
    gv_mute(m_media, m_muted);
    if (m_speaker) gv_speaker(m_media, true);
    m_lastState = 0;
    setPhase("connecting", "Connecting…");
    m_poll->start();
}
void Calls::stopMedia() {
    m_poll->stop();
    if (m_media) { gv_stop(m_media); m_media = 0; qDebug() << "call: media stopped"; }
    m_tick->stop();
}
void Calls::poll() {
    if (!m_media) return;
    int s = gv_state(m_media), b = gv_bars(m_media);
    if (b != m_bars) { m_bars = b; Q_EMIT changed(); }
    if (s == m_lastState) return;
    m_lastState = s;
    if (s == GV_ESTABLISHED) setPhase("talking", "");
    else if (s == GV_RECONNECTING) setPhase("reconnecting", "Reconnecting…");
    else if (s == GV_FAILED) {
        qDebug() << "call: media failed, error" << gv_error(m_media);
        bool talked = m_talkStart.isValid();
        m_talked = seconds();
        M r = req("discardCall"); r["call_id"] = m_id; r["is_disconnected"] = true; r["duration"] = m_talked; r["is_video"] = false; r["connection_id"] = QString("0");
        m_store->send(r, "call:discard");
        stopMedia();
        tone(talked ? "end" : "");
        setPhase("ended", talked ? "Connection lost" : "Couldn't connect");
        m_close->start();
    }
}

// ---------------------------------------------------------------- ringing, tones
void Calls::ring(bool on) {
    if (m_demo) return;
    if (!on) {
        if (m_ringer) m_ringer->stop();
        if (m_vibra) m_vibra->stop();
        m_vibraTimer->stop();
        return;
    }
    bb::platform::NotificationMode::Type mode = bb::platform::NotificationGlobalSettings().mode();
    bool sound = mode == bb::platform::NotificationMode::Normal || mode == bb::platform::NotificationMode::PhoneOnly
                 || mode == bb::platform::NotificationMode::Unknown;
    bool vibrate = mode != bb::platform::NotificationMode::Silent && mode != bb::platform::NotificationMode::AlertsOff;
    if (sound && QFile::exists(RINGTONE)) {
        if (!m_ringer) {
            m_ringer = new bb::multimedia::MediaPlayer(this);
            m_ringer->setRepeatMode(bb::multimedia::RepeatMode::Track);
        }
        m_ringer->setSourceUrl(QUrl::fromLocalFile(RINGTONE));
        m_ringer->play();
    }
    if (vibrate) {
        if (!m_vibra) {
            m_vibra = new bb::device::VibrationController(this);
        }
        m_vibra->start(80, 1000);
        m_vibraTimer->start();
    }
    qDebug() << "call: ringing, sound" << sound << "vibrate" << vibrate;
}
// proximity: while a call is up on the earpiece, a face near the screen blanks it and blocks touches
void Calls::updateSensor() {
    bool want = m_id && !m_demo && !m_speaker && m_phase != "ended" && !(m_phase == "ringing" && !m_outgoing);
    if (want && !m_prox) {
        QtMobility::QProximitySensor *s = new QtMobility::QProximitySensor(this);
        connect(s, SIGNAL(readingChanged()), this, SLOT(proximity()));
        m_prox = s;
    }
    if (!m_prox) return;
    QtMobility::QProximitySensor *s = static_cast<QtMobility::QProximitySensor *>(m_prox);
    if (want && !s->isActive()) s->start();
    if (!want && s->isActive()) s->stop();
    if (!want && m_near) { m_near = false; Q_EMIT nearChanged(); }
}
void Calls::proximity() {
    QtMobility::QProximitySensor *s = static_cast<QtMobility::QProximitySensor *>(m_prox);
    bool n = s && s->reading() && s->reading()->close();
    if (n != m_near) { m_near = n; Q_EMIT nearChanged(); }
}
void Calls::buzz() { if (m_vibra) m_vibra->start(80, 1000); }
void Calls::tone(const QString &kind) {
    if (m_demo) return;
    int k = kind == "ringback" ? 1 : kind == "busy" ? 2 : kind == "end" ? 3 : 0;
    gv_tone(k, m_speaker);
}

void Calls::finish() {
    m_close->stop();
    if (m_phase != "ended" && m_id) return;
    reset();
    Q_EMIT changed();
}
void Calls::reset() {
    stopMedia(); ring(false);
    m_id = 0; m_user = 0; m_outgoing = false; m_muted = false; m_speaker = false;
    m_name.clear(); m_initials.clear(); m_color.clear(); m_photo.clear(); m_phase.clear(); m_status.clear(); m_emojis.clear();
    m_bars = 0; m_lastState = 0; m_talkStart = QDateTime(); m_talked = 0;
    updateSensor();
}

void Calls::demo(const QVariantMap &call) {
    // {"phase": ..., "outgoing": bool, "user": id, "seconds": n, "emojis": "..."}: the screen in a given state
    m_demo = true;
    reset();
    m_id = 1; m_user = I(call.value("user")); m_outgoing = call.value("outgoing").toBool();
    M card = m_store->userCard(m_user);
    m_name = card.value("name").toString(); m_initials = card.value("initials").toString();
    m_color = card.value("color").toString(); m_photo = card.value("photo").toString();
    m_emojis = call.value("emojis").toString(); m_bars = 3;
    QString ph = call.value("phase").toString();
    if (ph == "talking") { m_talkStart = QDateTime::currentDateTime().addSecs(-call.value("seconds").toInt()); m_tick->start(); }
    m_phase = ph;
    m_status = ph == "ringing" ? (m_outgoing ? "Ringing…" : "Incoming call") : ph == "talking" ? "" : call.value("status").toString();
    Q_EMIT changed();
}
