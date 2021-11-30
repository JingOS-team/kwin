/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2017 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "gestures.h"

#include <QRect>
#include <functional>
#include <cmath>
#include <QDebug>
namespace KWin
{
// jing_kwin gesture
const quint32 GESTURE_INTERVAL = 30;
Gesture::Gesture(QObject *parent)
    : QObject(parent)
{
}

Gesture::~Gesture() = default;

SwipeGesture::SwipeGesture(QObject *parent)
    : Gesture(parent)
{
}

SwipeGesture::~SwipeGesture() = default;

PinchGesture::PinchGesture(QObject *parent)
    : Gesture(parent)
{
}

PinchGesture::~PinchGesture() = default;

qreal PinchGesture::calFingerDistance(const QMap<qint32, QPointF> &points)
{
    qreal distance = 0.f;
    QPointF lastpoint;
    for (auto it = points.begin(); it != points.end(); it++) {
        if (it != points.begin()) {
            double width = it.value().x() - (it-1).value().x();
            double height = it.value().y() - (it-1).value().y();
            distance +=  sqrt((height * height) + (width * width)); 
        }
    }
    return distance;
}

void SwipeGesture::setStartGeometry(const QRect &geometry)
{
    setMinimumX(geometry.x());
    setMinimumY(geometry.y());
    setMaximumX(geometry.x() + geometry.width());
    setMaximumY(geometry.y() + geometry.height());

    Q_ASSERT(m_maximumX >= m_minimumX);
    Q_ASSERT(m_maximumY >= m_minimumY);
}

qreal SwipeGesture::minimumDeltaReachedProgress(const QSizeF &delta) const
{
    if (!m_minimumDeltaRelevant || m_minimumDelta.isNull()) {
        return 1.0;
    }
    if (m_direction == Direction::All) {
        qreal ret = 1.0;
        if (m_minimumSwipeAngle) {
            qreal angle = std::atan(std::abs(delta.width())/std::abs(delta.height())) * 180/3.14;
            ret = std::min(angle/m_minimumSwipeAngle, ret);
        }
        ret = std::min((std::abs(delta.width()) + std::abs(delta.height())) / (std::abs(m_minimumDelta.width()) + std::abs(m_minimumDelta.height())), ret);
        return ret;
    }
    switch (m_direction) {
    case Direction::Up:
        if(delta.height()>0)
            return 0;
        return std::min(std::abs(delta.height()) / std::abs(m_minimumDelta.height()), 1.0);
    case Direction::Down:
        if(delta.height()<0)
            return 0;
        return std::min(std::abs(delta.height()) / std::abs(m_minimumDelta.height()), 1.0);
    case Direction::Left:
    case Direction::Right:
        return std::min(std::abs(delta.width()) / std::abs(m_minimumDelta.width()), 1.0);
    default:
        Q_UNREACHABLE();
    }
}

bool SwipeGesture::minimumDeltaReached(const QSizeF &delta) const
{
    return minimumDeltaReachedProgress(delta) >= 1.0;
}

GestureRecognizer::GestureRecognizer(QObject *parent)
    : QObject(parent)
{   
}

GestureRecognizer::~GestureRecognizer() = default;

void GestureRecognizer::registerGesture(KWin::Gesture* gesture)
{
    Q_ASSERT(!m_gestures.contains(gesture));
    auto connection = connect(gesture, &QObject::destroyed, this, std::bind(&GestureRecognizer::unregisterGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_gestures << gesture;
}

void GestureRecognizer::unregisterGesture(KWin::Gesture* gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_gestures.removeAll(gesture);
    if (m_activeSwipeGestures.removeOne(gesture)) {
        // jing_kwin gesture
        emit gesture->cancelled(0, 0);
    }
}

void GestureRecognizer::registerPinchGesture(Gesture *gesture)
{
    Q_ASSERT(!m_pinchGestures.contains(gesture));
    auto connection = connect(gesture, &QObject::destroyed, this, std::bind(&GestureRecognizer::unregisterPinchGesture, this, gesture));
    m_destroyPinchConnections.insert(gesture, connection);
    m_pinchGestures << gesture;
}

void GestureRecognizer::unregisterPinchGesture(Gesture *gesture)
{
    auto it = m_destroyPinchConnections.find(gesture);
    if (it != m_destroyPinchConnections.end()) {
        disconnect(it.value());
        m_destroyPinchConnections.erase(it);
    }
    m_pinchGestures.removeAll(gesture);
    emit gesture->cancelled(0, 0);
}

// jing_kwin gesture
int GestureRecognizer::startSwipeGesture(uint fingerCount, const QPointF &startPos, StartPositionBehavior startPosBehavior, quint32 time)
{
    int count = 0;
    // TODO: verify that no gesture is running
    for (Gesture *gesture : qAsConst(m_gestures)) {
        SwipeGesture *swipeGesture = qobject_cast<SwipeGesture*>(gesture);
        if (!gesture) {
            continue;
        }
        if (swipeGesture->minimumFingerCountIsRelevant()) {
            if (swipeGesture->minimumFingerCount() > fingerCount) {
                continue;
            }
        }
        if (swipeGesture->maximumFingerCountIsRelevant()) {
            if (swipeGesture->maximumFingerCount() < fingerCount) {
                continue;
            }
        }

        if (startPosBehavior == StartPositionBehavior::Relevant) {
            if (swipeGesture->minimumXIsRelevant()) {
                if (swipeGesture->minimumX() > std::ceil(startPos.x())) {
                    continue;
                }
            }
            if (swipeGesture->maximumXIsRelevant()) {
                if (swipeGesture->maximumX() < std::floor(startPos.x())) {
                    continue;
                }
            }
            if (swipeGesture->minimumYIsRelevant()) {
                if (swipeGesture->minimumY() > std::ceil(startPos.y())) {
                    continue;
                }
            }
            if (swipeGesture->maximumYIsRelevant()) {
                if (swipeGesture->maximumY() < std::floor(startPos.y())) {
                    continue;
                }
            }
        }
        // direction doesn't matter yet
        m_activeSwipeGestures << swipeGesture;
        count++;
        // jing_kwin gesture
        m_lastUpdateTime = m_lastEndUpdateTime = time;
        emit swipeGesture->started(time);
    }

    return count;
}

// jing_kwin gesture
void GestureRecognizer::updateSwipeGesture(const QSizeF &delta, quint32 time)
{
    m_swipeUpdates << delta;
    if (std::abs(delta.width()) < 1 && std::abs(delta.height()) < 1) {
        // some (touch) devices report sub-pixel movement on screen edges
        // this often cancels gestures -> ignore these movements
        return;
    }
    // determine the direction of the swipe
    if (delta.width() == delta.height()) {
        // special case of diagonal, this is not yet supported, thus cancel all gestures
        // jing_kwin gesture
        cancelActiveSwipeGestures(time);
        return;
    }
    // jing_kwin gesture
    if ((time - m_lastUpdateTime) > GESTURE_INTERVAL) {
        m_startIndex = m_endIndex;
        m_endIndex = m_swipeUpdates.size();
        m_lastUpdateTime = m_lastEndUpdateTime;
        m_lastEndUpdateTime = time;
    }
    // jing_kwin gesture end

//    SwipeGesture::Direction direction;
//    if (std::abs(delta.width()) > std::abs(delta.height())) {
//        // horizontal
//        direction = delta.width() < 0 ? SwipeGesture::Direction::Left : SwipeGesture::Direction::Right;
//    } else {
//        // vertical
//        direction = delta.height() < 0 ? SwipeGesture::Direction::Up : SwipeGesture::Direction::Down;
//    }
    bool isTriggerFlag = false;
    const QSizeF combinedDelta = std::accumulate(m_swipeUpdates.constBegin(), m_swipeUpdates.constEnd(), QSizeF(0, 0));
    for (auto it = m_activeSwipeGestures.begin(); it != m_activeSwipeGestures.end();) {
        auto g = qobject_cast<SwipeGesture*>(*it);
        //if (g->direction() == direction || g->direction() == SwipeGesture::Direction::All) {
        if (g->minimumDeltaReached(combinedDelta)) {
            qreal spead = 0.0;
            const QSizeF lastDelta = std::accumulate(m_swipeUpdates.constBegin() + m_startIndex, m_swipeUpdates.constEnd(), QSizeF(0, 0));
            if (std::abs(lastDelta.width()) > std::abs(lastDelta.height())) {
                spead = lastDelta.width() / (time - m_lastUpdateTime);
            } else {
                spead = lastDelta.height() / (time - m_lastUpdateTime);
            }
            emit g->triggered(time, spead);
            isTriggerFlag = true;
            it = m_activeSwipeGestures.erase(it);
        } else {
            if (g->isMinimumDeltaRelevant()) {
                // jing_kwin gesture
                qreal curProgress = g->minimumDeltaReachedProgress(combinedDelta);
                emit g->progress(curProgress, time);
                emit g->update(delta, curProgress, time);
            }
            it++;
        }
            //    } else {
            //        // jing_kwin gesture
            //        emit g->cancelled(time);
            //        it = m_activeSwipeGestures.erase(it);
            //    }
    }
    if (isTriggerFlag) {
        isTriggerFlag = false;

        for (auto it = m_activeSwipeGestures.begin(); it != m_activeSwipeGestures.end();){
            auto g = qobject_cast<SwipeGesture*>(*it);
            emit g->cancelled(time, 0);
            it = m_activeSwipeGestures.erase(it);
        }
    }
}

// jing_kwin gesture
bool GestureRecognizer::cancelActiveSwipeGestures(quint32 time, int fingerCount)
{
    if (fingerCount > 0) {
        for (auto g : qAsConst(m_activeSwipeGestures)) {
            // jing_kwin gesture
            SwipeGesture *sg = dynamic_cast<SwipeGesture*>(g);
            if (sg && sg->maximumFingerCount() >= fingerCount && sg->minimumFingerCount() <= fingerCount) {
                return false;
            }
        }
    }

    for (auto g : qAsConst(m_activeSwipeGestures)) {
        // jing_kwin gesture
        emit g->cancelled(time, 0);
    }

    m_activeSwipeGestures.clear();
    // jing_kwin gesture
    m_lastUpdateTime = 0;
    m_startIndex = 0;
    m_endIndex = 0;
    m_lastEndUpdateTime = 0;
    // jing_kwin gesture end

    return true;
}

// jing_kwin gesture
bool GestureRecognizer::cancelSwipeGesture(quint32 time, int fingerCount)
{
    // jing_kwin gesture
    if (!cancelActiveSwipeGestures(time, fingerCount)) {
        return false;
    }
    m_swipeUpdates.clear();
    // jing_kwin gesture
    m_lastUpdateTime = 0;
    m_startIndex = 0;
    m_endIndex = 0;
    m_lastEndUpdateTime = 0;
    // jing_kwin gesture end

    return true;
}

// jing_kwin gesture
void GestureRecognizer::endSwipeGesture(quint32 time)
{
    const QSizeF delta = std::accumulate(m_swipeUpdates.constBegin(), m_swipeUpdates.constEnd(), QSizeF(0, 0));
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        // jing_kwin gesture
        SwipeGesture *swipeGesture = static_cast<SwipeGesture*>(g);
        qreal spead = 0.0;
        const QSizeF lastDelta = std::accumulate(m_swipeUpdates.constBegin() + m_startIndex, m_swipeUpdates.constEnd(), QSizeF(0, 0));
        if (std::abs(lastDelta.width()) > std::abs(lastDelta.height())) {
            spead = lastDelta.width() / (time - m_lastUpdateTime);
        } else {
            spead = lastDelta.height() / (time - m_lastUpdateTime);
        }
        if (swipeGesture->minimumDeltaReached(delta)) {
            emit g->triggered(time, spead);
        } else {
            emit g->cancelled(time, spead);
        }
    }
    m_activeSwipeGestures.clear();
    m_swipeUpdates.clear();
    m_lastUpdateTime = 0;
    m_startIndex = 0;
    m_endIndex = 0;
    m_lastEndUpdateTime = 0;
    // jing_kwin gesture
}

// jing_kwin gesture
int GestureRecognizer::startPinchGesture(qint32 id, const QPointF &pos, quint32 time)
{
    auto it = m_fingerPoints.find(id);
    if (it == m_fingerPoints.end()) {
        m_fingerPoints.insert(id, pos);
    } else {
        it.value() = pos;
    }
    return m_fingerPoints.size();
}

// jing_kwin gesture
void GestureRecognizer::updatePinchGesture(qint32 id, const QPointF &pos, quint32 time)
{
    auto it = m_fingerPoints.find(id);
    if (it != m_fingerPoints.end()) {
        it.value() = pos;
    } else {
        m_fingerPoints.insert(id, pos);
    }

    for (Gesture *gesture : qAsConst(m_pinchGestures)) {
        PinchGesture *pinchGesture = qobject_cast<PinchGesture*>(gesture);
        if (!gesture) {
            continue;
        }
        if (pinchGesture->maximumFingerCountIsRelevant()) {
            if (pinchGesture->maximumFingerCount() < m_fingerPoints.size()) {
                continue;
            }
        }
        if (pinchGesture->minimumFingerCountIsRelevant()) {
            if (pinchGesture->minimumFingerCount() > m_fingerPoints.size()) {
                continue;
            }
        }
        m_activePinchGestures << pinchGesture;
    }

    if (m_isPinchTrigger == false) {
        for (auto it = m_activePinchGestures.begin(); it != m_activePinchGestures.end(); it++) {
            auto g = qobject_cast<PinchGesture*>(*it);
            if (g->isReachIncrement(PinchGesture::calFingerDistance(m_fingerPoints))) {
                emit g->triggered(0, 0);
                m_isPinchTrigger = true;
                // it = m_activePinchGestures.erase(it);
                // cancelPinchGesture(0);
                // cancelSwipeGesture(0, 0);
                break;
            }
        }
    }
}

// jing_kwin gesture
bool GestureRecognizer::cancelPinchGesture(qint32 touches)
{
    Q_UNUSED(touches);
    m_isPinchTrigger = false;
    m_fingerPoints.clear();
    for (auto it = m_activePinchGestures.begin(); it != m_activePinchGestures.end(); it++) {
        auto g = qobject_cast<PinchGesture*>(*it);
        emit g->cancelled(0, 0);
    }
    m_activePinchGestures.clear();
    return false;
}

// jing_kwin gesture
void GestureRecognizer::endPinchGesture(qint32 id, quint32 time)
{
    cancelPinchGesture(0);
    // Q_UNUSED(time);
    // auto it = m_fingerPoints.find(id);
    // if (it != m_fingerPoints.end()) {
    //     m_fingerPoints.erase(it);
    // }
}

}
