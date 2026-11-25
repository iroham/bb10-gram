#include "tdclient.h"
#include <bb/data/JsonDataAccess>
#include <QDebug>
#include <QTimer>
#include <bb/system/InvokeManager>
#include <bb/system/InvokeRequest>

extern "C" {
int tg_create_client_id(void);
void tg_send(int client_id, const char *request);
const char *tg_receive(double timeout);
const char *tg_execute(const char *request);
}

// Own encoder: whole numbers are always written in full (ids must never appear as 1e+12).
static void enc(const QVariant &v, QByteArray &o) {
    switch ((int)v.type()) {
    case QVariant::Map: {
        QVariantMap m = v.toMap(); bool first = true; o += '{';
        for (QVariantMap::const_iterator i = m.constBegin(); i != m.constEnd(); ++i) {
            if (!i.value().isValid()) continue;
            if (!first) o += ',';
            first = false; enc(i.key(), o); o += ':'; enc(i.value(), o);
        }
        o += '}'; break; }
    case QVariant::List: case QVariant::StringList: {
        QVariantList l = v.toList(); o += '[';
        for (int i = 0; i < l.size(); i++) { if (i) o += ','; enc(l.at(i), o); }
        o += ']'; break; }
    case QVariant::Bool: o += v.toBool() ? "true" : "false"; break;
    case QVariant::Int: case QVariant::LongLong: o += QByteArray::number(v.toLongLong()); break;
    case QVariant::UInt: case QVariant::ULongLong: o += QByteArray::number(v.toULongLong()); break;
    case QVariant::Double: {
        double d = v.toDouble();
        if (d == (double)(qint64)d && d > -9.0e15 && d < 9.0e15) o += QByteArray::number((qint64)d);
        else o += QByteArray::number(d, 'g', 17);
        break; }
    case QVariant::Invalid: o += "null"; break;
    default: {
        QString s = v.toString(); o += '"';
        for (int i = 0; i < s.size(); i++) {
            ushort c = s.at(i).unicode();
            if (c == '"') o += "\\\""; else if (c == '\\') o += "\\\\"; else if (c == '\n') o += "\\n";
            else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t";
            else if (c < 0x20) { char b[8]; qsnprintf(b, sizeof b, "\\u%04x", c); o += b; }
            else if (c >= 0xd800 && c < 0xdc00 && i + 1 < s.size()) { o += s.mid(i, 2).toUtf8(); i++; }
            else o += QString(s.at(i)).toUtf8();
        }
        o += '"'; }
    }
}
QByteArray TdClient::toJson(const QVariant &v) {
    QByteArray out;
    enc(v, out);
    return out;
}
QVariant TdClient::fromJson(const char *json) {
    bb::data::JsonDataAccess jda;
    QVariant v = jda.loadFromBuffer(QByteArray::fromRawData(json, qstrlen(json)));
    if (jda.hasError()) { qWarning() << "td: cannot parse:" << jda.error().errorMessage(); return QVariant(); }
    return v;
}

void TdReceiver::run() {
    while (!m_stop) {
        const char *r = tg_receive(1.0);
        if (!r) continue;
        QVariant v = TdClient::fromJson(r);
        if (v.type() == QVariant::Map) Q_EMIT received(v.toMap());
    }
}

TdClient::TdClient(const QString &dataDir, QObject *parent)
    : QObject(parent), m_id(0), m_receiver(new TdReceiver(this)), m_sock(new QLocalSocket(this)), m_sockPath(dataDir + "/gramd.sock"),
      m_remote(false), m_local(false), m_tries(0) {
    connect(m_receiver, SIGNAL(received(QVariantMap)), this, SLOT(onReceived(QVariantMap)), Qt::QueuedConnection);
    connect(m_sock, SIGNAL(readyRead()), this, SLOT(onSockData()));
    connect(m_sock, SIGNAL(disconnected()), this, SLOT(onSockGone()));
}
TdClient::~TdClient() { m_receiver->stop(); m_receiver->wait(3000); }

// The service normally runs already (started at boot / install). If not, ask BB10 to start it and wait a little;
// without it the library runs inside the window as before (notifications then only while the window runs).
void TdClient::start() {
    tg_execute("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":1}");
    m_sock->connectToServer(m_sockPath);
    if (m_sock->waitForConnected(500)) { sockConnected(); return; }
    bb::system::InvokeManager im;
    bb::system::InvokeRequest r;
    r.setTarget("org.bq.gram.service");
    r.setAction("org.bq.gram.START");
    im.invoke(r);
    qDebug() << "td: service not running, asked BB10 to start it";
    QTimer::singleShot(700, this, SLOT(tryService()));
}
void TdClient::tryService() {
    if (m_remote || m_local) return;
    m_sock->abort();
    m_sock->connectToServer(m_sockPath);
    if (m_sock->waitForConnected(300)) { sockConnected(); return; }
    if (++m_tries < 8) { QTimer::singleShot(700, this, SLOT(tryService())); return; }
    qWarning() << "td: no service, running the library in the window";
    startLocal();
}
void TdClient::sockConnected() {
    qDebug() << "td: connected to the service";
    m_remote = true; m_tries = 0;
    QVariantMap q; q["@type"] = "getCurrentState";
    send(q, "@state");
    Q_EMIT connectedChanged(true);
}
void TdClient::onSockGone() {
    if (!m_remote) return;
    qWarning() << "td: service connection lost, reconnecting";
    m_remote = false;
    QTimer::singleShot(1000, this, SLOT(tryService()));
}
void TdClient::onSockData() {
    m_buf += m_sock->readAll();
    int nl;
    while ((nl = m_buf.indexOf('\n')) >= 0) {
        QByteArray line = m_buf.left(nl);
        m_buf.remove(0, nl + 1);
        if (line.isEmpty()) continue;
        QVariant v = fromJson(line.constData());
        if (v.type() != QVariant::Map) continue;
        QVariantMap m = v.toMap();
        if (m.value("@extra").toString() == "@state") {          // the snapshot: replayed as ordinary updates
            QVariantList l = m.value("updates").toList();
            qDebug() << "td: state from the service:" << l.size() << "updates";
            for (int i = 0; i < l.size(); i++) Q_EMIT update(l.at(i).toMap());
            continue;
        }
        onReceived(m);
    }
}
void TdClient::startLocal() {
    m_local = true;
    m_id = tg_create_client_id();
    m_receiver->start();
    QVariantMap q; q["@type"] = "getOption"; q["name"] = "version";
    send(q);
    Q_EMIT connectedChanged(false);
}
void TdClient::control(const QVariantMap &message) {
    if (!m_remote) return;
    QVariantMap m = message;
    m_sock->write(toJson(m) + "\n");
}
void TdClient::restart() {            // after "closed" (sign-out): a new client (the service does this itself)
    if (m_remote || !m_local) return;
    m_id = tg_create_client_id();
    QVariantMap q; q["@type"] = "getOption"; q["name"] = "version";
    send(q);
}
void TdClient::send(const QVariantMap &request, const QString &tag) {
    QVariantMap r = request;
    if (!tag.isEmpty()) r["@extra"] = tag;
    if (m_remote) { m_sock->write(toJson(r) + "\n"); return; }
    if (!m_local) return;                  // not connected yet: dropped (the snapshot brings the state)
    tg_send(m_id, toJson(r).constData());
}
QVariantMap TdClient::execute(const QVariantMap &request) {
    const char *r = tg_execute(toJson(request).constData());
    return r ? fromJson(r).toMap() : QVariantMap();
}
void TdClient::onReceived(const QVariantMap &object) {
    QVariant extra = object.value("@extra");
    if (extra.isValid()) Q_EMIT answer(extra.toString(), object);
    else Q_EMIT update(object);
}
