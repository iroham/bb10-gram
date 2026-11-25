// AppLock: the optional 4-digit PIN in front of the app. Settings live in data/lock.json (mode 600): a random salt
// and the PBKDF2-SHA256 hash of the PIN (never the PIN), how long the app may stay in the background before it
// locks, and whether notifications hide names and text (the service, gramd, reads that part).
// Locks when the app starts and when it comes back to the front after more than `after` seconds away.
// Five wrong PINs in a row: wait 30 s (kept in the file, so restarting the app doesn't skip it).
#ifndef APPLOCK_H
#define APPLOCK_H
#include <QObject>
#include <QByteArray>
#include <QString>

class QTimer;

class AppLock : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool locked READ locked NOTIFY changed)
    Q_PROPERTY(int after READ after NOTIFY changed)              // seconds: 0 immediately, 60, 300, 3600
    Q_PROPERTY(bool hideText READ hideText NOTIFY changed)
    Q_PROPERTY(int wait READ wait NOTIFY waitChanged)            // seconds left before another try is allowed
public:
    AppLock(const QString &dataDir, QObject *parent = 0);
    bool enabled() const { return !m_hash.isEmpty(); }
    bool locked() const { return m_locked; }
    int after() const { return m_after; }
    bool hideText() const { return m_hide; }
    int wait() const;
    Q_INVOKABLE bool unlock(const QString &pin);    // false: wrong (or still waiting)
    Q_INVOKABLE void unlockByFace();                 // a verified face match unlocked (no PIN)
    Q_INVOKABLE void setPin(const QString &pin);     // turns the lock on, or changes the PIN
    Q_INVOKABLE void disable();
    Q_INVOKABLE void setAfter(int seconds);
    Q_INVOKABLE void setHideText(bool on);
    Q_INVOKABLE QString afterName(int seconds) const;
Q_SIGNALS:
    void changed();
    void waitChanged();
    void wrongPin(int triesLeft);                     // 0: now waiting
    void saved();                                     // lock.json written (the service re-reads it)
public Q_SLOTS:
    void windowActive(bool front);
private Q_SLOTS:
    void tick();
private:
    QByteArray hash(const QString &pin, const QByteArray &salt) const;
    void load();
    void save();
    QString m_file;
    QByteArray m_salt, m_hash;
    int m_after;
    bool m_hide;
    bool m_locked;
    int m_fails;
    qint64 m_waitUntil;       // ms since epoch
    qint64 m_awaySince;       // ms since epoch, 0 = in front
    QTimer *m_timer;
};
#endif
