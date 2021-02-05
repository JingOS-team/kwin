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
        auto ret = std::min((std::abs(delta.width()) + std::abs(delta.height())) / (std::abs(m_minimumDelta.width()) + std::abs(m_minimumDelta.height())), 1.0);
        return ret;
    }
    switch (m_direction) {
    case Direction::Up:
    case Direction::Down:
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
        emit gesture->cancelled(0);
    }
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
                if (swipeGesture->minimumX() > startPos.x()) {
                    continue;
                }
            }
            if (swipeGesture->maximumXIsRelevant()) {
                if (swipeGesture->maximumX() < startPos.x()) {
                    continue;
                }
            }
            if (swipeGesture->minimumYIsRelevant()) {
                if (swipeGesture->minimumY() > startPos.y()) {
                    continue;
                }
            }
            if (swipeGesture->maximumYIsRelevant()) {
                if (swipeGesture->maximumY() < startPos.y()) {
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

    SwipeGesture::Direction direction;
    if (std::abs(delta.width()) > std::abs(delta.height())) {
        // horizontal
        direction = delta.width() < 0 ? SwipeGesture::Direction::Left : SwipeGesture::Direction::Right;
    } else {
        // vertical
        direction = delta.height() < 0 ? SwipeGesture::Direction::Up : SwipeGesture::Direction::Down;
    }
    const QSizeF combinedDelta = std::accumulate(m_swipeUpdates.constBegin(), m_swipeUpdates.constEnd(), QSizeF(0, 0));
    for (auto it = m_activeSwipeGestures.begin(); it != m_activeSwipeGestures.end();) {
        auto g = qobject_cast<SwipeGesture*>(*it);
        //if (g->direction() == direction || g->direction() == SwipeGesture::Direction::All) {
        if (g->minimumDeltaReached(combinedDelta)) {
            qreal speed = 0.0;
            const QSizeF lastDelta = std::accumulate(m_swipeUpdates.constBegin() + m_startIndex, m_swipeUpdates.constEnd(), QSizeF(0, 0));
            if (std::abs(lastDelta.width()) > std::abs(lastDelta.height())) {
                speed = lastDelta.width() / (time - m_lastUpdateTime);
            } else {
                speed = lastDelta.height() / (time - m_lastUpdateTime);
            }
            emit g->triggered(time, speed);
            it = m_activeSwipeGestures.erase(it);
        } else {
            if (g->isMinimumDeltaRelevant()) {
                // jing_kwin gesture
                emit g->progress(g->minimumDeltaReachedProgress(combinedDelta), time);
                emit g->update(delta, time);
            }
            it++;
        }
        //        } else {
        //            // jing_kwin gesture
        //            emit g->cancelled(time);
        //            it = m_activeSwipeGestures.erase(it);
        //        }
    }
}

// jing_kwin gesture
void GestureRecognizer::cancelActiveSwipeGestures(quint32 time)
{
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        // jing_kwin gesture
        emit g->cancelled(time);
    }
    m_activeSwipeGestures.clear();
    // jing_kwin gesture
    m_lastUpdateTime = 0;
    m_startIndex = 0;
    m_endIndex = 0;
    m_lastEndUpdateTime = 0;
    // jing_kwin gesture end
}

// jing_kwin gesture
void GestureRecognizer::cancelSwipeGesture(quint32 time)
{
    // jing_kwin gesture
    cancelActiveSwipeGestures(time);
    m_swipeUpdates.clear();
    // jing_kwin gesture
    m_lastUpdateTime = 0;
    m_startIndex = 0;
    m_endIndex = 0;
    m_lastEndUpdateTime = 0;
    // jing_kwin gesture end
}

// jing_kwin gesture
void GestureRecognizer::endSwipeGesture(quint32 time)
{
    const QSizeF delta = std::accumulate(m_swipeUpdates.constBegin(), m_swipeUpdates.constEnd(), QSizeF(0, 0));
    for (auto g : qAsConst(m_activeSwipeGestures)) {
        // jing_kwin gesture
        SwipeGesture *swipeGesture = static_cast<SwipeGesture*>(g);
        if (swipeGesture->minimumDeltaReached(delta)) {
            qreal speed = 0.0;
            const QSizeF lastDelta = std::accumulate(m_swipeUpdates.constBegin() + m_startIndex, m_swipeUpdates.constEnd(), QSizeF(0, 0));
            if (std::abs(lastDelta.width()) > std::abs(lastDelta.height())) {
                speed = lastDelta.width() / (time - m_lastUpdateTime);
            } else {
                speed = lastDelta.height() / (time - m_lastUpdateTime);
            }
            emit g->triggered(time, speed);
        } else {
            emit g->cancelled(time);
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

}
