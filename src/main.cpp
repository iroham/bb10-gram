// Gram: native BB10 Telegram client. Cascades UI (assets/*.qml) over the Telegram library (libtdjson.so).
//   TdClient (tdclient.*) talks to the library, Store (store.*) keeps what the screens show.
// Tests: data/debug.json {"demo": "<file with fake updates>", "open": <chat id>, "say": "text"} (see tools/demo.py).
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>
#include <bb/system/InvokeTargetReply>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QTimer>
#include <QElapsedTimer>
#include "stickerplayer.h"
#include <bb/ApplicationInfo>
#include <QtDeclarative/qdeclarative.h>
#include <QtDeclarative/QDeclarativeContext>
#include <stdio.h>
#include "store.h"
#include "tdclient.h"
#include "media.h"
#include "applock.h"
#include "facelock.h"
#include "skin.h"

using namespace bb::cascades;

static void log_msg(QtMsgType, const char *m) { fprintf(stderr, "ui: %s\n", m); }

// Opening a Hub notification invokes org.bq.gram.open with the chat id as data.
class Invoked : public QObject {
    Q_OBJECT
public:
    Invoked(Store *s, QObject *parent) : QObject(parent), m_store(s), m_lock(0), m_td(0) {}
    AppLock *m_lock;
    TdClient *m_td;
public Q_SLOTS:
    void activeChanged() { if (m_lock) m_lock->windowActive(m_store->active()); }
    void lockSaved() { if (m_td && m_td->remote()) { QVariantMap c; c["@ui"] = "lock"; m_td->control(c); } }
    void raise() {                               // a call rings while the app is minimized: bring it to the front
        if (m_store->active()) return;
        bb::system::InvokeRequest r;
        r.setTarget("org.bq.gram.open"); r.setAction("bb.action.OPEN"); r.setMimeType("application/vnd.bq.gram.chat"); r.setData("call");
        bb::system::InvokeManager im;
        bb::system::InvokeTargetReply *rep = im.invoke(r);
        if (rep) rep->deleteLater();
    }
    void onInvoked(const bb::system::InvokeRequest &r) {
        qint64 id = r.data().toLongLong();
        qDebug() << "invoked:" << r.action() << id;
        if (id) m_store->openFromNotification(id);
    }
private:
    Store *m_store;
};

Q_DECL_EXPORT int main(int argc, char **argv) {
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    QString root = QDir::currentPath();                       // the app's sandbox
    SkinData::instance()->load(root + "/data");               // theme.json: dark/light + accent
    qputenv("CASCADES_THEME", SkinData::instance()->mode() == "light" ? "bright" : "dark");
    QByteArray log = (root + "/data/app.log").toUtf8();
    if (freopen(log.constData(), "w", stderr)) setvbuf(stderr, NULL, _IOLBF, 0);
    qInstallMsgHandler(log_msg);
    Application app(argc, argv);
    qmlRegisterType<QTimer>("gram", 1, 0, "QTimer");
    qmlRegisterType<StickerPlayer>("gram", 1, 0, "StickerPlayer");
    qmlRegisterUncreatableType<Skin>("gram", 1, 0, "Skin", "Skin is only used as an attached type (Skin.listBg)");
    SkinData::instance()->applyToCascades();
    QVariantMap debug;
    QFile df(root + "/data/debug.json");
    if (df.open(QIODevice::ReadOnly)) { QByteArray b = df.readAll(); debug = TdClient::fromJson(b.constData()).toMap(); }
    if (debug.contains("probeOgg")) {            // converter checks
        QByteArray in = debug.value("probeOgg").toString().toUtf8(), out = (root + "/data/probe.wav").toUtf8();
        qDebug() << "probe opus_to_wav" << opus_to_wav(in.constData(), out.constData()) << QFileInfo(root + "/data/probe.wav").size();
        debug["probeAudio"] = "file://" + root + "/data/probe.wav";
    }
    if (debug.contains("probeWebp")) {
        QByteArray in = debug.value("probeWebp").toString().toUtf8(), out = (root + "/data/probe.png").toUtf8();
        qDebug() << "probe webp_to_png" << webp_to_png(in.constData(), out.constData(), 512) << QFileInfo(root + "/data/probe.png").size();
        debug["probeImage"] = "file://" + root + "/data/probe.png";
    }
    TdClient *td = new TdClient(root + "/data", &app);
    Store *store = new Store(td, root + "/data", &app);
    bb::system::InvokeManager *im = new bb::system::InvokeManager(&app);
    Invoked *inv = new Invoked(store, &app);
    AppLock *lock = new AppLock(root + "/data", &app);
    FaceLock *faceLock = new FaceLock(root + "/data", &app);
    inv->m_lock = lock; inv->m_td = td;
    QObject::connect(store, SIGNAL(activeChanged()), inv, SLOT(activeChanged()));
    QObject::connect(lock, SIGNAL(saved()), inv, SLOT(lockSaved()));
    QObject::connect(im, SIGNAL(invoked(const bb::system::InvokeRequest&)), inv, SLOT(onInvoked(const bb::system::InvokeRequest&)));
    QObject::connect(store->calls(), SIGNAL(ringing()), inv, SLOT(raise()));
    QmlDocument *qml = QmlDocument::create("asset:///main.qml");
    QDeclarativeContext *ctx = qml->documentContext();
    ctx->setContextProperty("appVersion", bb::ApplicationInfo().version());
    ctx->setContextProperty("store", store);
    ctx->setContextProperty("appLock", lock);
    ctx->setContextProperty("faceLock", faceLock);
    ctx->setContextProperty("skin", SkinData::instance());
    ctx->setContextProperty("chatsModel", store->chats());
    ctx->setContextProperty("archiveModel", store->archive());
    ctx->setContextProperty("messagesModel", store->messages());
    ctx->setContextProperty("debug", debug);
    if (qml->hasErrors()) {
        foreach (const QDeclarativeError &e, qml->errors()) qWarning() << e.toString();
        return 1;
    }
    if (debug.contains("heapTest")) QTimer::singleShot(3000, store, SLOT(heapProbe()));
    if (debug.contains("storyTest")) QTimer::singleShot(20000, store, SLOT(storyProbe()));
    if (debug.contains("adminTest")) QTimer::singleShot(20000, store, SLOT(adminProbe()));
    if (debug.contains("lockTest") && !lock->enabled()) {      // test aid: the lock's PIN path, then off again
        QElapsedTimer t; t.start();
        lock->setPin("1357");
        qWarning() << "locktest: set in" << t.elapsed() << "ms, enabled" << lock->enabled();
        qWarning() << "locktest: wrong pin ->" << lock->unlock("0000") << ", right pin ->" << lock->unlock("1357") << ", locked" << lock->locked();
        QFile lf(root + "/data/lock.json");
        lf.open(QIODevice::ReadOnly);
        QVariantMap lm = TdClient::fromJson(lf.readAll().constData()).toMap();
        qWarning() << "locktest: file keys" << lm.keys() << "hash len" << lm.value("hash").toString().size() << "mode" << QString::number((int)lf.permissions(), 16);
        lf.close();
        lock->disable();
        qWarning() << "locktest: disabled ->" << lock->enabled();
    }
    if (debug.contains("qmlcheck")) {             // test aid: build every page once and log its errors (data/app.log)
        QStringList pages = QDir("app/native/assets").entryList(QStringList() << "*.qml");
        foreach (const QString &pg, pages) {
            if (pg == "main.qml") continue;
            QmlDocument *d = QmlDocument::create("asset:///" + pg).parent(&app);
            QDeclarativeContext *c = d->documentContext();
            c->setContextProperty("appVersion", bb::ApplicationInfo().version());
            c->setContextProperty("store", store); c->setContextProperty("appLock", lock); c->setContextProperty("skin", SkinData::instance());
            c->setContextProperty("chatsModel", store->chats()); c->setContextProperty("archiveModel", store->archive());
            c->setContextProperty("messagesModel", store->messages()); c->setContextProperty("debug", debug);
            QObject *o = d->hasErrors() ? 0 : d->createRootObject<QObject>();
            if (d->hasErrors()) foreach (const QDeclarativeError &e, d->errors()) qWarning() << "qmlcheck:" << e.toString();
            qWarning() << "qmlcheck:" << pg << (o ? "ok" : "FAILED");
            if (o) o->deleteLater();
        }
    }
    AbstractPane *pane = qml->createRootObject<AbstractPane>();
    if (!pane) return 1;
    Application::instance()->setScene(pane);
    if (debug.contains("demo")) store->loadDemo(debug.value("demo").toString());
    else td->start();
    return Application::exec();
}

#include "main.moc"
