#include "touchgesture.h"
#include "screens.h"

const int GESTURE_BORDER_WIDTH = 1;
const qreal GESTURE_MINIMUM_DELTA_RATE = 0.06f;

QRect topRect(QSizeF screenSize) {
    return  QRect(0, 0, screenSize.width(), GESTURE_BORDER_WIDTH);
}
QRect rightRect(QSizeF screenSize) {
    return QRect(screenSize.width() - (GESTURE_BORDER_WIDTH), 0, screenSize.width(), screenSize.height());
}
QRect bottomRect(QSizeF screenSize) {
    return QRect(0, screenSize.height() - (GESTURE_BORDER_WIDTH), screenSize.width(), screenSize.height());
}
QRect leftRect(QSizeF screenSize) {
    return QRect(0, 0, (GESTURE_BORDER_WIDTH), screenSize.height());
}

TouchGestureRecognizer::TouchGestureRecognizer()
{

}

TouchGestureRecognizer::~TouchGestureRecognizer()
{

}

bool TouchGestureRecognizer::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_ASSERT(!_touchTrajectory.contains(id));
    QList<QPointF> points;
    points<<pos;
    _touchTrajectory.insert(id, points);
    _lasTime = time;

    if (startGesture(id, pos, time) > 0) {
        return true;
    }
    return false;
}

bool TouchGestureRecognizer::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_ASSERT(_touchTrajectory.contains(id));
    _touchTrajectory[id].append(pos);
    _lasTime = time;

    return false;
}

bool TouchGestureRecognizer::touchUp(qint32 id, quint32 time)
{

    _touchTrajectory.remove(id);
    _lasTime = 0;
    return false;
}

int TouchGestureRecognizer::startGesture(qint32 id, const QPointF &pos, quint32 time)
{
    int screen = screens()->number(pos);
    if (screen == -1)
        screen = screens()->current();

    QSizeF screenSize = screens()->size(screen);
    if (_touchTrajectory.size() == 1) {
        return startPanGesture(id, pos, time);
    } else if (_touchTrajectory.size() == 3) {
        return startMultiFingerGesture(id, pos, time);
    }

    return 0;
}
