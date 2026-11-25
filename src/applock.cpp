#include "applock.h"
#include "pinhash.h"
#include "tdclient.h"
#include <QDateTime>
#include <QFile>
#include <QTimer>
#include <QVariantMap>

static qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }
enum { kIterations = 20000, kMaxFails = 5, kWaitMs = 30000 };

AppLock::AppLock(const QString &dataDir, QObject *parent)
    : QObject(parent), m_file(dataDir + "/lock.json"), m_after(60), m_hide(true), m_locked(false), m_fails(0), m_waitUntil(0), m_awaySince(0) {
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    load();
    m_locked = enabled();
    if (wait() > 0) m_timer->start();
}

QByteArray AppLock::hash(const QString &pin, const QByteArray &salt) const {
    QByteArray p = pin.toUtf8();
    unsigned char out[32];
    gram_pbkdf2_sha256((const unsigned char *)p.constData(), p.size(), (const unsigned char *)salt.constData(), salt.size(), kIterations, out, 32);
    return QByteArray((const char *)out, 32);
}

void AppLock::load() {
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly)) return;
    QVariantMap m = TdClient::fromJson(f.readAll().constData()).toMap();
    if (!m.value("on").toBool()) return;
    m_salt = QByteArray::fromHex(m.value("salt").toByteArray());
    m_hash = QByteArray::fromHex(m.value("hash").toByteArray());
    if (m_salt.size() != 16 || m_hash.size() != 32) { m_salt.clear(); m_hash.clear(); return; }
    m_after = m.contains("after") ? m.value("after").toInt() : 60;
    m_hide = m.contains("hideText") ? m.value("hideText").toBool() : true;
    m_fails = m.value("fails").toInt();
    m_waitUntil = m.value("waitUntil").toLongLong();
}

void AppLock::save() {
    QVariantMap m;
    m["on"] = enabled();
    if (enabled()) {
        m["salt"] = QString(m_salt.toHex()); m["hash"] = QString(m_hash.toHex()); m["iter"] = (int)kIterations;
        m["after"] = m_after; m["hideText"] = m_hide; m["fails"] = m_fails; m["waitUntil"] = m_waitUntil;
    }
    QFile f(m_file);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        f.write(TdClient::toJson(m));
        f.close();
    }
    Q_EMIT saved();
}

int AppLock::wait() const {
    qint64 left = m_waitUntil - nowMs();
    return left > 0 ? (int)((left + 999) / 1000) : 0;
}

bool AppLock::unlock(const QString &pin) {
    if (!enabled()) { m_locked = false; Q_EMIT changed(); return true; }
    if (wait() > 0) return false;
    if (hash(pin, m_salt) == m_hash) {
        bool hadFails = m_fails != 0;
        m_fails = 0; m_waitUntil = 0; m_locked = false;
        if (hadFails) save();
        Q_EMIT changed();
        return true;
    }
    m_fails++;
    int left = kMaxFails - m_fails;
    if (left <= 0) { m_fails = 0; m_waitUntil = nowMs() + kWaitMs; m_timer->start(); Q_EMIT waitChanged(); left = 0; }
    save();
    Q_EMIT wrongPin(left);
    return false;
}

void AppLock::unlockByFace() {
    // Called only after libfacelock verified a live, matching face. Treat it as
    // a successful unlock (same state change as a correct PIN); never writes or
    // checks the PIN. A no-op if the lock isn't on.
    if (!enabled()) { m_locked = false; Q_EMIT changed(); return; }
    bool hadFails = m_fails != 0;
    m_fails = 0; m_waitUntil = 0; m_locked = false;
    if (hadFails) save();
    Q_EMIT changed();
}

void AppLock::setPin(const QString &pin) {
    QFile r("/dev/urandom");
    QByteArray salt;
    if (r.open(QIODevice::ReadOnly)) salt = r.read(16);
    if (salt.size() != 16) { salt.resize(16); for (int i = 0; i < 16; i++) salt[i] = (char)(qrand() ^ (nowMs() >> (i % 8))); }
    m_salt = salt;
    m_hash = hash(pin, salt);
    m_fails = 0; m_waitUntil = 0; m_locked = false;
    save();
    Q_EMIT changed();
}

void AppLock::disable() {
    m_salt.clear(); m_hash.clear(); m_locked = false; m_fails = 0; m_waitUntil = 0;
    save();
    Q_EMIT changed();
}

void AppLock::setAfter(int seconds) { m_after = seconds < 0 ? 0 : seconds; save(); Q_EMIT changed(); }
void AppLock::setHideText(bool on) { m_hide = on; save(); Q_EMIT changed(); }

QString AppLock::afterName(int s) const {
    if (s <= 0) return "Immediately";
    if (s < 3600) return QString("After %1 minute%2").arg(s / 60).arg(s / 60 == 1 ? "" : "s");
    return QString("After %1 hour%2").arg(s / 3600).arg(s / 3600 == 1 ? "" : "s");
}

void AppLock::windowActive(bool front) {
    if (!front) { if (!m_awaySince) m_awaySince = nowMs(); return; }
    qint64 away = m_awaySince ? nowMs() - m_awaySince : 0;
    m_awaySince = 0;
    if (enabled() && !m_locked && away >= (qint64)m_after * 1000 && (m_after > 0 || away > 0)) { m_locked = true; Q_EMIT changed(); }
}

void AppLock::tick() {
    if (wait() <= 0) m_timer->stop();
    Q_EMIT waitChanged();
}
