// StickerPlayer: plays an animated sticker (.tgs or .webm file) for an ImageView in QML:
//   StickerPlayer { id: p; source: "/path/file.tgs"; size: 340; }
//   ImageView { image: p.image }
// Frames are rendered by libgramanim.so (rlottie / libvpx) at the shown size, looped.
#ifndef STICKERPLAYER_H
#define STICKERPLAYER_H
#include <QObject>
#include <QVariant>
#include <bb/cascades/Image>
struct gm_anim;
class QTimer;

class StickerPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int size READ size WRITE setSize)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(QVariant image READ image NOTIFY imageChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY imageChanged)      // a frame has been drawn
public:
    explicit StickerPlayer(QObject *parent = 0);
    ~StickerPlayer();
    QString source() const { return m_source; }
    void setSource(const QString &s);
    int size() const { return m_size; }
    void setSize(int s) { m_size = s > 16 && s <= 512 ? s : 340; }
    bool playing() const { return m_playing; }
    void setPlaying(bool on);
    QVariant image() const { return m_hasImage ? QVariant::fromValue(m_image) : QVariant(); }
    bool ready() const { return m_hasImage; }
    static int running;                          // players drawing right now (all of them)
Q_SIGNALS:
    void sourceChanged();
    void playingChanged();
    void imageChanged();
private Q_SLOTS:
    void tick();
    void appHidden();
    void appShown();
private:
    void close();
    void update();
    QString m_source;
    int m_size, m_frame;
    bool m_playing, m_appShown, m_hasImage, m_counted;
    gm_anim *m_anim;
    QTimer *m_timer;
    bb::cascades::Image m_image;
};
#endif
