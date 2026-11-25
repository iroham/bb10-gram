#include "stickerplayer.h"
#include "../port/anim.h"
#include <QTimer>
#include <QFile>
#include <QDebug>
#include <bb/cascades/Application>
#include <bb/ImageData>

int StickerPlayer::running = 0;
static const int MAX_RUNNING = 6;                // more than this on screen: the rest show their first frame

StickerPlayer::StickerPlayer(QObject *parent)
    : QObject(parent), m_size(340), m_frame(0), m_playing(true), m_appShown(true), m_hasImage(false), m_counted(false), m_anim(0) {
    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    bb::cascades::Application *app = bb::cascades::Application::instance();   // pause while minimized or hidden
    connect(app, SIGNAL(thumbnail()), this, SLOT(appHidden()));
    connect(app, SIGNAL(invisible()), this, SLOT(appHidden()));
    connect(app, SIGNAL(fullscreen()), this, SLOT(appShown()));
}
void StickerPlayer::appHidden() { m_appShown = false; update(); }
void StickerPlayer::appShown() { m_appShown = true; update(); }
StickerPlayer::~StickerPlayer() { close(); }
void StickerPlayer::close() {
    m_timer->stop();
    if (m_counted) { running--; m_counted = false; }
    if (m_anim) { gm_close(m_anim); m_anim = 0; }
}
void StickerPlayer::setSource(const QString &s) {
    QString path = s.startsWith("file://") ? s.mid(7) : s;
    if (path == m_source) return;
    close();
    m_source = path; m_frame = 0;
    Q_EMIT sourceChanged();
    if (path.isEmpty() || !QFile::exists(path)) return;
    m_anim = gm_open(QFile::encodeName(path).constData(), m_size);
    if (!m_anim || gm_frames(m_anim) <= 0) { close(); return; }
    tick();                                      // first frame at once
    update();
    qDebug() << "anim:" << path.section('/', -1) << "frames" << (m_anim ? gm_frames(m_anim) : 0) << "fps" << (m_anim ? gm_fps(m_anim) : 0)
             << "drawn" << m_hasImage << "running" << running;
}
void StickerPlayer::setPlaying(bool on) {
    if (m_playing == on) return;
    m_playing = on;
    update();
    Q_EMIT playingChanged();
}
void StickerPlayer::update() {
    bool run = m_anim && m_playing && m_appShown && gm_frames(m_anim) > 1 && (m_counted || running < MAX_RUNNING);
    if (run && !m_counted) { running++; m_counted = true; }
    if (!run && m_counted) { running--; m_counted = false; }
    double fps = m_anim ? gm_fps(m_anim) : 30;
    if (fps < 1 || fps > 60) fps = 30;
    if (run) { if (!m_timer->isActive()) m_timer->start((int)(1000 / fps)); }
    else m_timer->stop();
}
void StickerPlayer::tick() {
    if (!m_anim) return;
    bb::ImageData img(bb::PixelFormat::RGBA_Premultiplied, m_size, m_size);
    unsigned char *px = img.pixels();
    int bpl = img.bytesPerLine();
    if (gm_render(m_anim, m_frame, px, bpl)) { close(); return; }
    for (int y = 0; y < m_size; y++) {           // BGRA -> RGBA
        unsigned char *p = px + y * bpl;
        for (int x = 0; x < m_size; x++, p += 4) { unsigned char t = p[0]; p[0] = p[2]; p[2] = t; }
    }
    m_image = bb::cascades::Image(img);
    m_hasImage = true;
    Q_EMIT imageChanged();
    m_frame = (m_frame + 1) % gm_frames(m_anim);
}
