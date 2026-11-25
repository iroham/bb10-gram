#include "skin.h"
#include "tdclient.h"
#include <bb/cascades/Application>
#include <bb/cascades/Color>
#include <bb/cascades/ThemeSupport>
#include <bb/cascades/VisualStyle>
#include <QFile>

// accents: name, colour in the dark theme, colour in the light theme (a little deeper, for contrast on white)
static const char *const kAccents[][3] = {
    { "Blue",   "#ff5aa7e6", "#ff3d8fd1" },
    { "Green",  "#ff5fc56a", "#ff3fa54a" },
    { "Orange", "#ffef9b4a", "#ffe07f22" },
    { "Red",    "#ffe86a6a", "#ffd44c4c" },
    { "Purple", "#ffa98ae6", "#ff8a65d6" },
    { "Teal",   "#ff47c1c7", "#ff1fa3aa" },
};

SkinData *SkinData::instance() { static SkinData *s = new SkinData; return s; }

SkinData::SkinData() : m_mode("dark"), m_accent("Blue") { build(); }

QVariantList SkinData::accents() const {
    QVariantList l;
    for (size_t i = 0; i < sizeof kAccents / sizeof kAccents[0]; i++) {
        QVariantMap m; m["name"] = kAccents[i][0]; m["color"] = kAccents[i][m_mode == "light" ? 2 : 1]; l << m;
    }
    return l;
}

static void rgb(const QString &argb, int &r, int &g, int &b) {     // "#aarrggbb"
    r = argb.mid(3, 2).toInt(0, 16); g = argb.mid(5, 2).toInt(0, 16); b = argb.mid(7, 2).toInt(0, 16);
}
static QString tint(const QString &argb, double f) {     // the colour at strength f over white (light bubbles)
    int r, g, b;
    rgb(argb, r, g, b);
    r = (int)(255 - (255 - r) * f); g = (int)(255 - (255 - g) * f); b = (int)(255 - (255 - b) * f);
    return QString("#ff%1%2%3").arg(r, 2, 16, QChar('0')).arg(g, 2, 16, QChar('0')).arg(b, 2, 16, QChar('0'));
}
static QString shade(const QString &argb, double f) {     // darker (f < 1) or lighter
    int r0, g0, b0;
    rgb(argb, r0, g0, b0);
    int r = qBound(0, (int)(r0 * f), 255), g = qBound(0, (int)(g0 * f), 255), b = qBound(0, (int)(b0 * f), 255);
    return QString("#ff%1%2%3").arg(r, 2, 16, QChar('0')).arg(g, 2, 16, QChar('0')).arg(b, 2, 16, QChar('0'));
}

void SkinData::build() {
    bool light = m_mode == "light";
    QString accent = kAccents[0][light ? 2 : 1];
    for (size_t i = 0; i < sizeof kAccents / sizeof kAccents[0]; i++) if (m_accent == kAccents[i][0]) accent = kAccents[i][light ? 2 : 1];
    QHash<QString, QString> &c = m_c;
    c.clear();
    if (!light) {
        c["listBg"] = "#ff17212b"; c["chatBg"] = "#ff0e1621"; c["headerBg"] = "#ff1f2c3a"; c["panelBg"] = "#ff1f2c3a"; c["line"] = "#ff101921";
        c["text"] = "#ffffffff"; c["dim"] = "#ff7f91a4"; c["bubbleIn"] = "#ff182533"; c["bubbleOut"] = shade(accent, 0.48);
        c["badgeMuted"] = "#ff3e546a"; c["error"] = "#ffe5646a"; c["press"] = "#22ffffff"; c["link"] = shade(accent, 1.12);
        c["headerText"] = "#ffffffff"; c["headerDim"] = "#ff7f91a4"; c["icon"] = "#ffffffff"; c["subtle"] = "#ffb8c7d6";
        c["outMeta"] = shade(accent, 1.25); c["dayChip"] = "#ff1c2b3a"; c["mediaBg"] = "#ff0b121a"; c["keyBg"] = "#ff22384d";
        c["keyDown"] = "#ff3a5875"; c["pollTrack"] = "#ff4d6a85"; c["secret"] = "#ff7bc862";
    } else {
        c["listBg"] = "#ffffffff"; c["chatBg"] = "#ffdbe5ed"; c["headerBg"] = shade(accent, 0.82); c["panelBg"] = "#ffffffff"; c["line"] = "#ffe3e8ec";
        c["text"] = "#ff111111"; c["dim"] = "#ff7d8b98"; c["bubbleIn"] = "#ffffffff"; c["bubbleOut"] = tint(accent, 0.18);
        c["badgeMuted"] = "#ffb3bdc6"; c["error"] = "#ffd9474f"; c["press"] = "#1a000000"; c["link"] = accent;
        c["headerText"] = "#ffffffff"; c["headerDim"] = "#ffdbe8f3"; c["icon"] = "#ff7d8b98"; c["subtle"] = "#ff66788a";
        c["outMeta"] = accent; c["dayChip"] = "#ffc4d3df"; c["mediaBg"] = "#ffe6ecf1"; c["keyBg"] = "#ffe8eef3";
        c["keyDown"] = "#ffcdd9e3"; c["pollTrack"] = "#ffcfdae3"; c["secret"] = "#ff3fa54a";
    }
    c["accent"] = accent; c["badge"] = accent;
}

void SkinData::load(const QString &dataDir) {
    m_file = dataDir + "/theme.json";
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        QVariantMap m = TdClient::fromJson(f.readAll().constData()).toMap();
        if (m.value("mode").toString() == "light") m_mode = "light";
        if (!m.value("accent").toString().isEmpty()) m_accent = m.value("accent").toString();
    }
    build();
}

void SkinData::save() {
    QVariantMap m; m["mode"] = m_mode; m["accent"] = m_accent;
    QFile f(m_file);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(TdClient::toJson(m));
}

void SkinData::applyToCascades() {
    bb::cascades::Application *app = bb::cascades::Application::instance();
    if (!app) return;
    int r, g, b;
    rgb(get("accent"), r, g, b);
    app->themeSupport()->setVisualStyleAndPrimaryColor(m_mode == "light" ? bb::cascades::VisualStyle::Bright : bb::cascades::VisualStyle::Dark,
                                                       bb::cascades::Color::fromRGBA(r / 255.0f, g / 255.0f, b / 255.0f));
}

void SkinData::setMode(const QString &mode) {
    QString m = mode == "light" ? "light" : "dark";
    if (m == m_mode) return;
    m_mode = m; build(); save(); applyToCascades();
    Q_EMIT changed();
}

void SkinData::setAccent(const QString &name) {
    if (name == m_accent) return;
    m_accent = name; build(); save(); applyToCascades();
    Q_EMIT changed();
}

Skin::Skin(QObject *parent) : QObject(parent) {
    connect(SkinData::instance(), SIGNAL(changed()), this, SIGNAL(changed()));
}
