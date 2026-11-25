// FaceLock — a reusable face-based lock gate for native BB10 apps.
//
// Drop this + the Glance core (cam/detect/recog/liveness/cascade_data) and
// FaceGate.qml into an app, expose a FaceLock instance to QML, and put a
// FaceGate over your root scene. The gate drives the front camera, shows a live
// preview in a ForeignWindowControl, requires a blink (liveness), and matches
// the face against the enrolled template. It emits succeeded() / failed().
//
// All Qt work happens on the GUI thread: the camera callback (on the camera's
// own thread) only copies the latest grayscale frame under a mutex; a QTimer on
// the GUI thread runs detection / recognition / liveness and emits the signals.
// This keeps signal emission thread-safe and keeps heavy CPU off the capture
// thread so frames keep flowing.
//
// The template store is shared across our apps (owner enrols once in Glance):
// see storePath(). Templates are one-way LBPH histograms, never images.
#ifndef FACELOCK_H
#define FACELOCK_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <stdint.h>

class QTimer;

extern "C" {
#include "cam.h"
#include "detect.h"
#include "recog.h"
#include "liveness.h"
}

class FaceLock : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status  READ status     NOTIFY statusChanged)
    Q_PROPERTY(bool enrolled   READ isEnrolled NOTIFY enrolledChanged)
    Q_PROPERTY(int  enrollCount READ enrollCount NOTIFY enrolledChanged)
    Q_PROPERTY(bool busy       READ busy       NOTIFY busyChanged)
    Q_PROPERTY(bool faceSeen   READ faceSeen   NOTIFY faceSeenChanged)
public:
    // dataDir is the app's private data directory (fallback store location).
    explicit FaceLock(const QString &dataDir, QObject *parent = 0);
    ~FaceLock();

    QString status() const      { return m_status; }
    bool    isEnrolled() const;
    int     enrollCount() const;
    bool    busy() const        { return m_mode != Idle; }
    bool    faceSeen() const    { return m_faceSeen; }

    // The ForeignWindowControl's window-group name and window id, so the camera
    // renders its preview into that control. Call before startVerify/startEnroll.
    Q_INVOKABLE void setPreviewWindow(const QString &group, const QString &id);

    // Begin a verify session: find a live face and match it. Ends with
    // succeeded(distance) or failed(reason). No-op if not enrolled.
    Q_INVOKABLE void startVerify();
    // Begin an enrol session: collect several live samples and save them.
    // Ends with enrolledDone() or failed(reason).
    Q_INVOKABLE void startEnroll();
    // Stop any session and release the camera.
    Q_INVOKABLE void cancel();
    // Forget the enrolled template(s). Irreversible.
    Q_INVOKABLE void clearEnrollment();

    // The shared template path (created under the Shared folder when available,
    // else the app data dir). Public so apps can show/verify it.
    Q_INVOKABLE QString storePath() const;

Q_SIGNALS:
    void statusChanged();
    void enrolledChanged();
    void busyChanged();
    void faceSeenChanged();
    void blinkPrompt();                 // "please blink"
    void blinkOk();                     // a blink was observed
    void progress(int done, int total); // enrol sample progress
    void succeeded(int distance);       // verify matched
    void enrolledDone();                // enrol saved
    void failed(const QString &reason); // "no face" / "liveness" / "no match" / "timeout"

private Q_SLOTS:
    void tick();                        // GUI-thread frame processor

private:
    enum Mode { Idle, Verifying, Enrolling };

    void  start(Mode m);
    void  stop();
    void  setStatus(const QString &s);
    void  ensureBuffers(int w, int h);
    void  processFrame(const uint8_t *gray, int w, int h);
    void  finishOk(int distance);
    void  finishEnroll();
    void  finishFail(const QString &reason);
    void  keepScreenAwake(bool on);

    // copy the latest frame in from the camera thread (mutex-guarded)
    void  pushFrame(const uint8_t *gray, int w, int h);
    friend void facelock_frame_cb(const glance_frame_t *f, void *user);

    QString          m_dataDir;
    QString          m_status;
    Mode             m_mode;
    bool             m_faceSeen;

    // preview window
    QString          m_winGroup, m_winId;

    // camera + latest frame (mutex-guarded handoff from camera thread)
    glance_cam      *m_cam;
    uint8_t         *m_frame;       // latest grayscale, w*h
    int              m_frameW, m_frameH, m_frameCap;
    volatile bool    m_haveFrame;
    void            *m_mutex;       // QMutex* (kept opaque to avoid header cost)
    QTimer          *m_timer;

    // processing scratch
    glance_detector *m_det;
    uint8_t         *m_small;       // downscaled, m_smallCap bytes
    int              m_smallCap;

    // recognition / liveness state for the current session
    glance_enroll   *m_loaded;      // enrolled templates (verify)
    glance_enroll   *m_collect;     // samples being gathered (enroll)
    glance_liveness  m_live;
    float            m_bestDist;
    int              m_blinksAtStart;
    int64_t          m_startNs;
    int64_t          m_lastSampleNs;
    bool             m_blinkSignalled;
};

#endif
