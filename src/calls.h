// Calls: one-to-one voice calls. Signalling goes through TDLib (createCall, acceptCall, discardCall, updateCall);
// the media runs in libgramvoip.so (libtgvoip + the phone's voice audio) once TDLib says callStateReady.
// QML sees it as store.calls:
//   active, incoming (ringing for us), callId, userId, name, initials, color, photo, phase, status, seconds,
//   emojis (4 key emoji once connected), muted, speaker, bars (0..4), ended
//   start(userId), accept(), hangUp(), setMuted(on), setSpeaker(on)
#ifndef CALLS_H
#define CALLS_H
#include <QObject>
#include <QVariant>
#include <QDateTime>
#include <QStringList>
class Store;
class QTimer;
struct gv_call;
namespace bb { namespace multimedia { class MediaPlayer; } }
namespace bb { namespace device { class VibrationController; } }

class Calls : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)          // a call screen should be up (also briefly after the end)
    Q_PROPERTY(bool incoming READ incoming NOTIFY changed)      // ringing for us: show Accept / Decline
    Q_PROPERTY(bool ended READ ended NOTIFY changed)
    Q_PROPERTY(QVariant callId READ callId NOTIFY changed)
    Q_PROPERTY(QVariant userId READ userId NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString initials READ initials NOTIFY changed)
    Q_PROPERTY(QString color READ color NOTIFY changed)
    Q_PROPERTY(QString photo READ photo NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)         // requesting ringing exchanging connecting talking reconnecting ended
    Q_PROPERTY(QString status READ status NOTIFY changed)       // the line under the name
    Q_PROPERTY(int seconds READ seconds NOTIFY tick)
    Q_PROPERTY(QString emojis READ emojis NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(bool speaker READ speaker NOTIFY changed)
    Q_PROPERTY(int bars READ bars NOTIFY changed)
    Q_PROPERTY(bool near READ near NOTIFY nearChanged)        // phone held to the ear (earpiece calls): blank the screen
public:
    Calls(Store *store, const QString &dataDir, QObject *parent = 0);
    ~Calls();
    bool active() const { return m_id != 0; }
    bool incoming() const { return m_id && !m_outgoing && m_phase == "ringing"; }
    bool ended() const { return m_phase == "ended"; }
    QVariant callId() const { return m_id; }
    QVariant userId() const { return m_user; }
    QString name() const { return m_name; }
    QString initials() const { return m_initials; }
    QString color() const { return m_color; }
    QString photo() const { return m_photo; }
    QString phase() const { return m_phase; }
    QString status() const { return m_status; }
    int seconds() const;
    QString emojis() const { return m_emojis; }
    bool muted() const { return m_muted; }
    bool speaker() const { return m_speaker; }
    int bars() const { return m_bars; }
    bool near() const { return m_near; }

    Q_INVOKABLE void start(const QVariant &userId);
    Q_INVOKABLE void accept();
    Q_INVOKABLE void hangUp();                    // decline when ringing, cancel when calling, end when talking
    Q_INVOKABLE void setMuted(bool on);
    Q_INVOKABLE void setSpeaker(bool on);
    Q_INVOKABLE void dismiss();                   // close the ended screen now

    void onUpdateCall(const QVariantMap &call);   // from the store
    void onAnswer(const QString &tag, const QVariantMap &r);
    Q_INVOKABLE void demo(const QVariantMap &call); // demo mode: show a call without TDLib
Q_SIGNALS:
    void changed();
    void tick();
    void message(const QString &text);            // short note for a toast
    void ringing();                               // incoming call: bring the window up
    void nearChanged();
private Q_SLOTS:
    void poll();
    void finish();
    void buzz();
    void proximity();
private:
    void updateSensor();
    QVariantMap protocol() const;
    void setPhase(const QString &phase, const QString &status = QString());
    void startMedia(const QVariantMap &ready);
    void stopMedia();
    void ring(bool on);
    void tone(const QString &kind);               // ringback, busy, end, "" = stop (earpiece, through libgramvoip)
    void reset();
    QString durationText() const;
    Store *m_store;
    QString m_dataDir;
    qint64 m_id, m_user;
    bool m_outgoing, m_muted, m_speaker, m_demo;
    QString m_name, m_initials, m_color, m_photo, m_phase, m_status, m_emojis;
    int m_bars, m_lastState;
    QDateTime m_talkStart;
    int m_talked;                                 // seconds, fixed once the call ended
    gv_call *m_media;
    QTimer *m_poll, *m_tick, *m_close;
    bb::multimedia::MediaPlayer *m_ringer;
    bb::device::VibrationController *m_vibra;
    QTimer *m_vibraTimer;
    QObject *m_prox;                              // QtMobility::QProximitySensor
    bool m_near;
};
#endif
