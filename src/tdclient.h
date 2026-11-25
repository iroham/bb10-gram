// TdClient: the app's connection to the Telegram library (libtdjson.so). Requests go out as QVariantMaps
// ({"@type": ...}), updates and answers come back as QVariantMaps on the UI thread.
#ifndef TDCLIENT_H
#define TDCLIENT_H
#include <QObject>
#include <QThread>
#include <QVariant>
#include <QHash>
#include <QLocalSocket>

class TdReceiver : public QThread {
    Q_OBJECT
public:
    explicit TdReceiver(QObject *parent = 0) : QThread(parent), m_stop(false) {}
    void stop() { m_stop = true; }
Q_SIGNALS:
    void received(const QVariantMap &object);      // one update or answer, already parsed (receiver thread)
protected:
    void run();
private:
    volatile bool m_stop;
};

class TdClient : public QObject {
    Q_OBJECT
public:
    explicit TdClient(const QString &dataDir, QObject *parent = 0);
    ~TdClient();
    void start();
    void restart();
    bool remote() const { return m_remote; }             // the library runs in the service (gramd)
    void control(const QVariantMap &message);            // {"@ui": ...} to the service
    // send a request; the answer (if wanted) arrives as answer(tag, object). tag "" = no answer wanted.
    Q_INVOKABLE void send(const QVariantMap &request, const QString &tag = QString());
    // run one of the library's local functions right away (text entities, formatting...)
    Q_INVOKABLE QVariantMap execute(const QVariantMap &request);
    static QByteArray toJson(const QVariant &v);
    static QVariant fromJson(const char *json);
Q_SIGNALS:
    void update(const QVariantMap &u);                          // u["@type"] = "updateNewMessage", ...
    void answer(const QString &tag, const QVariantMap &result); // result["@type"] may be "error"
Q_SIGNALS:
    void connectedChanged(bool remote);
private Q_SLOTS:
    void onReceived(const QVariantMap &object);
    void tryService();
    void onSockData();
    void onSockGone();
private:
    void startLocal();
    void sockConnected();
    int m_id;
    TdReceiver *m_receiver;
    QLocalSocket *m_sock;
    QString m_sockPath;
    QByteArray m_buf;
    bool m_remote;
    bool m_local;
    int m_tries;
};
#endif
