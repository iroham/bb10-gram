#include "facelock.h"

#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QDir>
#include <QFile>

#include <bb/cascades/Application>
#include <bb/cascades/Window>

#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---- tunables (from on-device Phase 2/3 measurements) -----------------------
static const int   DOWN         = 4;        // detection downscale
static const int   MIN_BOX      = 150;      // require full-res face width >= this (px)
static const float ACCEPT_DIST  = 55.0f;    // chi-square accept threshold (stranger ~66)
static const int   ENROLL_TARGET = 12;      // samples to collect when enrolling (vary pose)
static const int   VERIFY_TIMEOUT_MS = 12000;
static const int   ENROLL_TIMEOUT_MS = 25000;
static const int   TICK_MS      = 60;       // GUI-thread processing cadence
static const qint64 SAMPLE_GAP_NS = 500000000LL; // >=500ms between enrol samples (spread for variety)

static int64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Camera-thread callback: just copy the latest grayscale frame in. No Qt here.
void facelock_frame_cb(const glance_frame_t *f, void *user)
{
    FaceLock *self = (FaceLock *)user;
    if (self && f && f->gray) self->pushFrame(f->gray, f->width, f->height);
}

FaceLock::FaceLock(const QString &dataDir, QObject *parent)
    : QObject(parent), m_dataDir(dataDir), m_status("Ready"), m_mode(Idle),
      m_faceSeen(false), m_cam(0), m_frame(0), m_frameW(0), m_frameH(0),
      m_frameCap(0), m_haveFrame(false), m_mutex(new QMutex()), m_timer(0),
      m_det(0), m_small(0), m_smallCap(0), m_loaded(0), m_collect(0),
      m_bestDist(1e30f), m_blinksAtStart(0), m_startNs(0), m_lastSampleNs(0),
      m_blinkSignalled(false)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(TICK_MS);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    glance_liveness_reset(&m_live);
}

FaceLock::~FaceLock()
{
    stop();
    if (m_det) glance_detector_free(m_det);
    free(m_small);
    free(m_frame);
    if (m_loaded) glance_enroll_free(m_loaded);
    if (m_collect) glance_enroll_free(m_collect);
    delete (QMutex *)m_mutex;
}

// ---- store ------------------------------------------------------------------
// Shared across our apps when the Shared folder is reachable; otherwise the
// app's own data dir. (A single secure shared path is finalised on-device with
// the Phase 5 helper; this keeps every app working today either way.)
QString FaceLock::storePath() const
{
    const QString shared = "/accounts/1000/shared/misc/faceunlock";
    QDir d(shared);
    if (d.exists() || QDir().mkpath(shared))
        return shared + "/owner.gfl";
    return m_dataDir + "/owner.gfl";
}

bool FaceLock::isEnrolled() const
{
    return enrollCount() > 0;
}

int FaceLock::enrollCount() const
{
    glance_enroll *e = glance_enroll_load(storePath().toUtf8().constData());
    if (!e) return 0;
    int n = glance_enroll_count(e);
    glance_enroll_free(e);
    return n;
}

// ---- lifecycle --------------------------------------------------------------
void FaceLock::setPreviewWindow(const QString &group, const QString &id)
{
    m_winGroup = group;
    m_winId = id;
}

void FaceLock::ensureBuffers(int w, int h)
{
    if (!m_det) {
        int md = 1280 / DOWN + 1;              // covers 1280 in the larger dim
        m_det = glance_detector_new(md, md);
    }
    int sneed = (w / DOWN + 1) * (h / DOWN + 1);
    if (m_smallCap < sneed) {
        m_small = (uint8_t *)realloc(m_small, (size_t)sneed);
        m_smallCap = sneed;
    }
}

void FaceLock::start(Mode m)
{
    if (m_mode != Idle) cancel();

    // reset per-session state
    glance_liveness_reset(&m_live);
    m_bestDist = 1e30f;
    m_faceSeen = false;
    m_blinkSignalled = false;
    m_startNs = now_ns();
    m_lastSampleNs = 0;
    m_haveFrame = false;

    if (m == Verifying) {
        if (m_loaded) glance_enroll_free(m_loaded);
        m_loaded = glance_enroll_load(storePath().toUtf8().constData());
        if (!m_loaded || glance_enroll_count(m_loaded) == 0) {
            setStatus("Not enrolled");
            emit failed("not enrolled");
            return;
        }
    } else { // Enrolling
        if (m_collect) glance_enroll_free(m_collect);
        m_collect = glance_enroll_new();
    }

    char err[160] = {0};
    QByteArray g = m_winGroup.toUtf8(), id = m_winId.toUtf8();
    const char *gg = m_winGroup.isEmpty() ? 0 : g.constData();
    const char *ii = m_winId.isEmpty() ? 0 : id.constData();
    m_cam = glance_cam_open_front_win(1280, 720, gg, ii, err, sizeof err);
    if (!m_cam) {
        qWarning("facelock: camera open failed: %s", err);
        setStatus(QString("Camera error: ") + err);
        emit failed(QString("camera: ") + err);
        return;
    }
    int rc = glance_cam_start(m_cam, facelock_frame_cb, this);
    if (rc != 0) {
        qWarning("facelock: start_viewfinder failed: %d", rc);
        setStatus("Camera busy");
        glance_cam_close(m_cam); m_cam = 0;
        emit failed(QString("camera start: %1").arg(rc));
        return;
    }

    m_mode = m;
    keepScreenAwake(true);
    setStatus(m == Verifying ? "Look at the camera" : "Enrolling… look at the camera");
    emit busyChanged();
    m_timer->start();
}

void FaceLock::startVerify() { start(Verifying); }
void FaceLock::startEnroll() { start(Enrolling); }

void FaceLock::stop()
{
    if (m_timer) m_timer->stop();
    if (m_cam) { glance_cam_stop(m_cam); glance_cam_close(m_cam); m_cam = 0; }
    keepScreenAwake(false);
    bool was = (m_mode != Idle);
    m_mode = Idle;
    if (m_faceSeen) { m_faceSeen = false; emit faceSeenChanged(); }
    if (was) emit busyChanged();
}

void FaceLock::cancel()
{
    if (m_mode == Idle) return;
    stop();
    setStatus("Cancelled");
}

void FaceLock::clearEnrollment()
{
    QFile::remove(storePath());
    emit enrolledChanged();
    setStatus("Enrollment cleared");
}

// ---- camera-thread handoff --------------------------------------------------
void FaceLock::pushFrame(const uint8_t *gray, int w, int h)
{
    QMutexLocker lock((QMutex *)m_mutex);
    int need = w * h;
    if (m_frameCap < need) {
        uint8_t *n = (uint8_t *)realloc(m_frame, (size_t)need);
        if (!n) return;
        m_frame = n; m_frameCap = need;
    }
    memcpy(m_frame, gray, (size_t)need);
    m_frameW = w; m_frameH = h;
    m_haveFrame = true;
}

// ---- GUI-thread processing --------------------------------------------------
void FaceLock::tick()
{
    // timeout?
    int elapsed = (int)((now_ns() - m_startNs) / 1000000LL);
    int limit = (m_mode == Verifying) ? VERIFY_TIMEOUT_MS : ENROLL_TIMEOUT_MS;
    if (elapsed > limit) { finishFail("timeout"); return; }

    // grab the latest frame
    static uint8_t *proc = 0; static int procCap = 0; int w = 0, h = 0;
    {
        QMutexLocker lock((QMutex *)m_mutex);
        if (!m_haveFrame) return;
        w = m_frameW; h = m_frameH;
        int need = w * h;
        if (procCap < need) { proc = (uint8_t *)realloc(proc, (size_t)need); procCap = need; }
        if (!proc) return;
        memcpy(proc, m_frame, (size_t)need);
        m_haveFrame = false;
    }
    processFrame(proc, w, h);
}

void FaceLock::processFrame(const uint8_t *gray, int w, int h)
{
    ensureBuffers(w, h);
    if (!m_det || !m_small) return;

    int sw = w / DOWN, sh = h / DOWN;
    for (int y = 0; y < sh; ++y) {
        const uint8_t *src = gray + (size_t)(y * DOWN) * w;
        uint8_t *dst = m_small + (size_t)y * sw;
        for (int x = 0; x < sw; ++x) dst[x] = src[x * DOWN];
    }

    glance_rect faces[8];
    int n = glance_detect(m_det, m_small, sw, sh, 1.15f, 0, 0, 2, faces, 8);

    bool haveFace = false;
    glance_rect b = {0,0,0,0};
    if (n > 0) {
        b.x = faces[0].x * DOWN; b.y = faces[0].y * DOWN;
        b.w = faces[0].w * DOWN; b.h = faces[0].h * DOWN;
        haveFace = (b.w >= MIN_BOX);
    }
    if (haveFace != m_faceSeen) { m_faceSeen = haveFace; emit faceSeenChanged(); }
    if (!haveFace) return;

    // normalise once; feed liveness and (verify) recognition
    uint8_t face[GLANCE_FACE * GLANCE_FACE];
    glance_normalize(gray, w, h, b, face);

    int blinksBefore = m_live.blinks;
    glance_liveness_feed_face(&m_live, face);
    if (m_live.blinks > blinksBefore) emit blinkOk();

    glance_template t;
    if (glance_template_build(gray, w, h, b, &t) != 0) return;

    if (m_mode == Verifying) {
        int which = -1;
        float d = glance_enroll_match(m_loaded, &t, &which);
        if (d < m_bestDist) m_bestDist = d;
        bool live = glance_liveness_passed(&m_live);
        if (m_bestDist > ACCEPT_DIST)
            setStatus("Keep looking…");
        else if (!live)
            setStatus("Confirming…");
        else
            setStatus("Matched");
        if (live && m_bestDist <= ACCEPT_DIST) finishOk((int)(m_bestDist + 0.5f));
    } else { // Enrolling
        int64_t now = now_ns();
        int have = glance_enroll_count(m_collect);
        if ((m_lastSampleNs == 0 || now - m_lastSampleNs >= SAMPLE_GAP_NS) &&
            have < ENROLL_TARGET) {
            if (glance_enroll_add(m_collect, &t) == 0) {
                m_lastSampleNs = now;
                have++;
                emit progress(have, ENROLL_TARGET);
                setStatus(QString("Captured %1 of %2 — slowly move your head a little")
                          .arg(have).arg(ENROLL_TARGET));
            }
        }
        // done when we have enough samples AND a blink proved liveness
        if (have >= ENROLL_TARGET && glance_liveness_passed(&m_live)) finishEnroll();
    }
}

void FaceLock::finishOk(int distance)
{
    stop();
    setStatus("Unlocked");
    emit succeeded(distance);
}

void FaceLock::finishEnroll()
{
    glance_enroll *c = m_collect; m_collect = 0;
    stop();
    if (!c || glance_enroll_count(c) == 0) {
        if (c) glance_enroll_free(c);
        setStatus("Enrol failed");
        emit failed("no samples");
        return;
    }
    int rc = glance_enroll_save(c, storePath().toUtf8().constData());
    int cnt = glance_enroll_count(c);
    glance_enroll_free(c);
    if (rc != 0) { setStatus("Save failed"); emit failed("save"); return; }
    setStatus(QString("Enrolled (%1 samples)").arg(cnt));
    emit enrolledChanged();
    emit enrolledDone();
}

void FaceLock::finishFail(const QString &reason)
{
    stop();
    if (reason == "timeout")
        setStatus(m_faceSeen ? "Couldn't confirm — try again" : "No face — try again");
    else
        setStatus(reason);
    emit failed(reason);
}

void FaceLock::setStatus(const QString &s)
{
    if (s == m_status) return;
    m_status = s;
    emit statusChanged();
}

void FaceLock::keepScreenAwake(bool on)
{
    // The camera powers down when the screen goes off; hold it awake while a
    // session runs so capture keeps going.
    bb::cascades::Window *w = bb::cascades::Application::instance()->mainWindow();
    if (w) w->setScreenIdleMode(on ? bb::cascades::ScreenIdleMode::KeepAwake
                                   : bb::cascades::ScreenIdleMode::Normal);
}
