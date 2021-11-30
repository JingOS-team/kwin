#include "globalgestureforwarder.h"
#include "globalgesturetypes.h"
#include <QDebug>
#include <QTimer>
#include <QSizeF>

GlobalGestureForwarder* GlobalGestureForwarder::_self = new GlobalGestureForwarder();

const QDataStream::Version QDATA_STREAM_VERSION = QDataStream::Qt_5_14;
GlobalGestureForwarder::GlobalGestureForwarder(QObject *parent)
    : QObject(parent),
      socket(new QLocalSocket(this)) ,
      connectTimer(new QTimer(this))
{

    outStream = new QDataStream(&block, QIODevice::WriteOnly);
    outStream->setVersion(QDATA_STREAM_VERSION);

    connect(socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, [this](QLocalSocket::LocalSocketError socketError) {
        switch (socketError) {
        case QLocalSocket::ServerNotFoundError:
           qCritical()<<tr("The host was not found. Please make sure "
                                        "that the server is running and that the "
                                        "server name is correct.");
            break;
        case QLocalSocket::ConnectionRefusedError:
            qCritical()<<tr("The connection was refused by the peer. "
                                        "Make sure the fortune server is running, "
                                        "and check that the server name "
                                        "is correct.");
            break;
        case QLocalSocket::PeerClosedError:
            break;
        default:
            qCritical()<<"The following error occurred:"<<socket->errorString();
        }
        QTimer::singleShot(50, this, [this] {
            connectToServer();
        });
    });

    connect(socket, &QLocalSocket::connected, this, [this]() {
        qDebug()<<"connection success";
        bConnected = true;
    });

    connect(socket, &QLocalSocket::disconnected, this, [this] {
        qDebug()<<"connection is broken.";
        QTimer::singleShot(50, this, [this] {
            bConnected = false;
            connectToServer();
        });
    });
}

void GlobalGestureForwarder::init(int pid)
{
    curServerPid = pid;
    connectToServer();
}

void GlobalGestureForwarder::pinchGestureBegin(int fingerCount, quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(GESTURE_PINCH_BEGIN) + sizeof(fingerCount) + sizeof(time));
        *outStream << GESTURE_PINCH_BEGIN << fingerCount << time;
        sendData();
    }
}

void GlobalGestureForwarder::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(qreal)*4 + sizeof(time) + sizeof(GESTURE_PINCH_UPDATE));
        *outStream << GESTURE_PINCH_UPDATE << scale << angleDelta << delta.width() << delta.height() << time;
        sendData();
    }
}

void GlobalGestureForwarder::pinchGestureEnd(quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(GESTURE_PINCH_END) + sizeof(time));
        *outStream << GESTURE_PINCH_END << time;
        sendData();
    }
}

void GlobalGestureForwarder::pinchGestureCancelled(quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(GESTURE_PINCH_CANCELL) + sizeof(time));
        *outStream << GESTURE_PINCH_CANCELL << time;
        sendData();
    }
}

void GlobalGestureForwarder::swipeGestureBegin(int fingerCount, quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(GESTURE_SWIPE_BEGIN) + sizeof(fingerCount) + sizeof(time));
        *outStream << GESTURE_SWIPE_BEGIN << fingerCount << time;
        sendData();
        lastSwipDelta = QSizeF(0,0);
    }
}

void GlobalGestureForwarder::swipeGestureUpdate(const QSizeF &delta, quint32 time)
{
    if (bConnected && delta != lastSwipDelta) {
        *outStream << (quint32)(sizeof(qreal)*2 + sizeof(time) + sizeof(GESTURE_SWIPE_UPDATE));
        *outStream << GESTURE_SWIPE_UPDATE << delta.width() << delta.height() << time;
        sendData();
        lastSwipDelta = QSizeF(delta);
    }
}

void GlobalGestureForwarder::swipeGestureEnd(quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(GESTURE_SWIPE_END) + sizeof(time));
        *outStream << GESTURE_SWIPE_END << time;
        sendData();
        lastSwipDelta = QSizeF(0,0);
    }
}

void GlobalGestureForwarder::swipeGestureCancelled(quint32 time)
{
    if (bConnected) {
        *outStream << (quint32)(sizeof(GESTURE_SWIPE_CANCELL) + sizeof(time));
        *outStream << GESTURE_SWIPE_CANCELL << time;
        sendData();
        lastSwipDelta = QSizeF(0,0);
    }
}

void GlobalGestureForwarder::connectToServer()
{
    socket->abort();
    QString serverName = QString(GESTURE_SERVER_NAME).arg(curServerPid);
    const char* name = serverName.toStdString().c_str();
    socket->connectToServer(serverName);
}

void GlobalGestureForwarder::sendData()
{
    if (bConnected) {
        socket->write(block);
        block.clear();
        (*outStream).device()->seek(0);
    }
}
