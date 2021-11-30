#ifndef GLOBALGESTUREFORWARDER_H
#define GLOBALGESTUREFORWARDER_H
#include <QThread>
#include <QDataStream>
#include <QLocalSocket>
#include <QSizeF>
class QTimer;

class GlobalGestureForwarder : public QObject
{
    Q_OBJECT
public:
    explicit GlobalGestureForwarder(QObject *parent = nullptr);
    static GlobalGestureForwarder* self() {
        return _self;
    }

    void pinchGestureBegin(int fingerCount, quint32 time);
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time);
    void pinchGestureEnd(quint32 time);
    void pinchGestureCancelled(quint32 time);

    void swipeGestureBegin(int fingerCount, quint32 time);
    void swipeGestureUpdate(const QSizeF &delta, quint32 time);
    void swipeGestureEnd(quint32 time);
    void swipeGestureCancelled(quint32 time);

    void init(int pid);
private:
    void connectToServer();
    void sendData();

private:
    QByteArray block;
    QDataStream *outStream;
    QLocalSocket *socket;
    QTimer *connectTimer;
    static GlobalGestureForwarder* _self;

    bool bConnected = false;
    friend class ConnectThread;
    int curServerPid = 0;
    QSizeF lastSwipDelta;
};

#endif // GLOBALGESTUREFORWARDER_H
