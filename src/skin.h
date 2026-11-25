// Skin: the colour theme (Dark / Light) and accent colour, switchable while the app runs.
// QML uses it as an attached type from "import gram 1.0": Color.create(Skin.listBg) etc. Attached properties work
// everywhere, list rows included (context properties don't reach rows), and notify, so pages restyle at once.
// The choice is kept in data/theme.json; the Cascades controls (switches, dialogs) follow via ThemeSupport.
#ifndef SKIN_H
#define SKIN_H
#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtDeclarative/qdeclarative.h>

class SkinData : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY changed)           // dark | light
    Q_PROPERTY(QString accentName READ accentName NOTIFY changed)
    Q_PROPERTY(QVariantList accents READ accents CONSTANT)      // [{name, color}]
public:
    static SkinData *instance();
    void load(const QString &dataDir);
    QString mode() const { return m_mode; }
    QString accentName() const { return m_accent; }
    QVariantList accents() const;
    QString get(const char *key) const { return m_c.value(key); }
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void setAccent(const QString &name);
    void applyToCascades();
Q_SIGNALS:
    void changed();
private:
    SkinData();
    void build();
    void save();
    QString m_file, m_mode, m_accent;
    QHash<QString, QString> m_c;
};

class Skin : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString listBg READ listBg NOTIFY changed)
    Q_PROPERTY(QString chatBg READ chatBg NOTIFY changed)
    Q_PROPERTY(QString headerBg READ headerBg NOTIFY changed)
    Q_PROPERTY(QString panelBg READ panelBg NOTIFY changed)
    Q_PROPERTY(QString line READ line NOTIFY changed)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY changed)
    Q_PROPERTY(QString dim READ dim NOTIFY changed)
    Q_PROPERTY(QString bubbleIn READ bubbleIn NOTIFY changed)
    Q_PROPERTY(QString bubbleOut READ bubbleOut NOTIFY changed)
    Q_PROPERTY(QString badge READ badge NOTIFY changed)
    Q_PROPERTY(QString badgeMuted READ badgeMuted NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString press READ press NOTIFY changed)
    Q_PROPERTY(QString link READ link NOTIFY changed)
    Q_PROPERTY(QString headerText READ headerText NOTIFY changed)
    Q_PROPERTY(QString headerDim READ headerDim NOTIFY changed)
    Q_PROPERTY(QString icon READ icon NOTIFY changed)
    Q_PROPERTY(QString subtle READ subtle NOTIFY changed)
    Q_PROPERTY(QString outMeta READ outMeta NOTIFY changed)
    Q_PROPERTY(QString dayChip READ dayChip NOTIFY changed)
    Q_PROPERTY(QString mediaBg READ mediaBg NOTIFY changed)
    Q_PROPERTY(QString keyBg READ keyBg NOTIFY changed)
    Q_PROPERTY(QString keyDown READ keyDown NOTIFY changed)
    Q_PROPERTY(QString pollTrack READ pollTrack NOTIFY changed)
    Q_PROPERTY(QString secret READ secret NOTIFY changed)
public:
    explicit Skin(QObject *parent = 0);
    static Skin *qmlAttachedProperties(QObject *object) { return new Skin(object); }
    bool dark() const { return SkinData::instance()->mode() != "light"; }
    QString listBg() const { return SkinData::instance()->get("listBg"); }
    QString chatBg() const { return SkinData::instance()->get("chatBg"); }
    QString headerBg() const { return SkinData::instance()->get("headerBg"); }
    QString panelBg() const { return SkinData::instance()->get("panelBg"); }
    QString line() const { return SkinData::instance()->get("line"); }
    QString accent() const { return SkinData::instance()->get("accent"); }
    QString text() const { return SkinData::instance()->get("text"); }
    QString dim() const { return SkinData::instance()->get("dim"); }
    QString bubbleIn() const { return SkinData::instance()->get("bubbleIn"); }
    QString bubbleOut() const { return SkinData::instance()->get("bubbleOut"); }
    QString badge() const { return SkinData::instance()->get("badge"); }
    QString badgeMuted() const { return SkinData::instance()->get("badgeMuted"); }
    QString error() const { return SkinData::instance()->get("error"); }
    QString press() const { return SkinData::instance()->get("press"); }
    QString link() const { return SkinData::instance()->get("link"); }
    QString headerText() const { return SkinData::instance()->get("headerText"); }
    QString headerDim() const { return SkinData::instance()->get("headerDim"); }
    QString icon() const { return SkinData::instance()->get("icon"); }
    QString subtle() const { return SkinData::instance()->get("subtle"); }
    QString outMeta() const { return SkinData::instance()->get("outMeta"); }
    QString dayChip() const { return SkinData::instance()->get("dayChip"); }
    QString mediaBg() const { return SkinData::instance()->get("mediaBg"); }
    QString keyBg() const { return SkinData::instance()->get("keyBg"); }
    QString keyDown() const { return SkinData::instance()->get("keyDown"); }
    QString pollTrack() const { return SkinData::instance()->get("pollTrack"); }
    QString secret() const { return SkinData::instance()->get("secret"); }
Q_SIGNALS:
    void changed();
};
QML_DECLARE_TYPEINFO(Skin, QML_HAS_ATTACHED_PROPERTIES)
#endif
