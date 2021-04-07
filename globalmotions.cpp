/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "globalmotions.h"
#include "globalmotions.h"
#include <QMouseEvent>
#include <QDateTime>

namespace KWin {

const quint32 GESTURE_INTERVAL = 500;
MouseMotion::MouseMotion(QObject *parent)
    : Gesture(parent)
{

}

void MouseMotion::setStartGeometry(const QRect &geometry)
{
    setMinimumX(geometry.x());
    setMinimumY(geometry.y());
    setMaximumX(geometry.x() + geometry.width());
    setMaximumY(geometry.y() + geometry.height());

    Q_ASSERT(m_maximumX >= m_minimumX);
    Q_ASSERT(m_maximumY >= m_minimumY);
}

qreal MouseMotion::minimumDeltaReachedProgress(const QSizeF &delta) const
{
    if (!m_minimumDeltaRelevant || m_minimumDelta.isNull()) {
        return 1.0;
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

bool MouseMotion::minimumDeltaReached(const QSizeF &delta) const
{
    return minimumDeltaReachedProgress(delta) >= 1.0 ;
}

MouseSwipeMotionRecognizer::MouseSwipeMotionRecognizer(QObject *parent)
    : QObject(parent)
{

}

MouseSwipeMotionRecognizer::~MouseSwipeMotionRecognizer()
{

}

void MouseSwipeMotionRecognizer::registerMotion(MouseMotion *motion)
{
    Q_ASSERT(!m_motions.contains(motion));
    auto connection = connect(motion, &QObject::destroyed, this, std::bind(&MouseSwipeMotionRecognizer::unregisterMotion, this, motion));
    m_destroyConnections.insert(motion, connection);
    m_motions << motion;
}

void MouseSwipeMotionRecognizer::unregisterMotion(MouseMotion *motion)
{
    auto it = m_destroyConnections.find(motion);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_motions.removeAll(motion);
    if (m_activeSwipeMotions.removeOne(motion)) {
        emit motion->cancelled(1, 0);
    }
}

int MouseSwipeMotionRecognizer::startSwipeMotion(const QPointF &startPos, qint64 time)
{
    int count = 0;
    // TODO: verify that no gesture is running
    for (MouseMotion *motion : qAsConst(m_motions)) {
        if (!motion) {
            continue;
        }
        if (motion->minimumXIsRelevant()) {
            if (motion->minimumX() > startPos.x()) {
                continue;
            }
        }
        if (motion->maximumXIsRelevant()) {
            if (motion->maximumX() < startPos.x()) {
                continue;
            }
        }
        if (motion->minimumYIsRelevant()) {
            if (motion->minimumY() > startPos.y()) {
                continue;
            }
        }
        if (motion->maximumYIsRelevant()) {
            if (motion->maximumY() < startPos.y()) {
                continue;
            }
        }
        // direction doesn't matter yet
        if (!m_activeSwipeMotions.contains(motion)) {
            m_activeSwipeMotions << motion;
        }
        count++;
        m_lastPos = m_endPos = m_startPos = startPos;
        m_lastEndUpdateTime = m_lastUpdateTime = time;
        emit motion->started(time);
    }
    return count;
}

void MouseSwipeMotionRecognizer::updateSwipeMotion(const QPointF &pos, qint64 time)
{
    if (m_lastUpdateTime <= 0 || (time - m_lastUpdateTime) < GESTURE_INTERVAL || m_activeSwipeMotions.size() < 0) {
        return;
    }

    MouseMotion::Direction direction;
    QSizeF delta(pos.x() - m_lastPos.x(), pos.y() - m_lastPos.y());
    if (std::abs(delta.width()) > std::abs(delta.height())) {
        m_lastAverageSeed = delta.width() / qreal(time - m_lastUpdateTime);
    } else {
        m_lastAverageSeed = delta.height() / qreal(time - m_lastUpdateTime);
    }

    m_lastUpdateTime = m_lastEndUpdateTime;
    m_lastEndUpdateTime = time;
    m_lastPos = m_endPos;
    m_endPos = pos;
}

void MouseSwipeMotionRecognizer::cancelSwipeMotion(qint64 time)
{
    for (auto g : qAsConst(m_activeSwipeMotions)) {
        emit g->cancelled(time - m_lastUpdateTime, 0);
    }
    m_lastAverageSeed = 0.0f;
    m_lastUpdateTime = 0;
    m_lastUpdateTime = 0;
    m_lastEndUpdateTime = 0;
    m_activeSwipeMotions.clear();
}

void MouseSwipeMotionRecognizer::endSwipeMotion(const QPointF &pos, qint64 time)
{
    if (time - m_lastUpdateTime < GESTURE_INTERVAL ||  0.0f == m_lastAverageSeed) {
        QSizeF lastDelta(pos.x() - m_lastPos.x(), pos.y() - m_lastPos.y());
        if (std::abs(lastDelta.width()) > std::abs(lastDelta.height())) {
            m_lastAverageSeed = lastDelta.width() / qreal(time - m_lastUpdateTime);
        } else {
            m_lastAverageSeed = lastDelta.height() / qreal(time - m_lastUpdateTime);
        }
    }

    QSizeF delta(pos.x() - m_startPos.x(), pos.y() - m_startPos.y());
    for (auto g : qAsConst(m_activeSwipeMotions)) {
        if (static_cast<MouseMotion*>(g)->minimumDeltaReached(delta)) {
            emit g->motionTriggered(time - m_lastUpdateTime, m_lastAverageSeed);
        } else {
            emit g->cancelled(time - m_lastUpdateTime, 0);
        }
    }
    m_activeSwipeMotions.clear();
    m_lastAverageSeed = 0.0f;
    m_lastUpdateTime = 0;
    m_lastUpdateTime = 0;
    m_lastEndUpdateTime = 0;
}


MouseSwipeMotionMgr* MouseSwipeMotionMgr::_self = nullptr;
MouseSwipeMotionMgr::MouseSwipeMotionMgr(QObject *parent)
    : QObject(parent)
    , m_recognizer(new MouseSwipeMotionRecognizer(this))
{
}

MouseSwipeMotionMgr::~MouseSwipeMotionMgr()
{
    _self = nullptr;
}

void MouseSwipeMotionMgr::onPointEvent(QMouseEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        onPressed(QPointF(event->x(), event->y()),  QDateTime::currentMSecsSinceEpoch());
        break;
    case QEvent::MouseButtonRelease:
        onRelease(QPointF(event->x(), event->y()),  QDateTime::currentMSecsSinceEpoch());
        break;
    case QEvent::MouseMove:
        onMotion(QPointF(event->x(), event->y()),  QDateTime::currentMSecsSinceEpoch());
        break;
    default:
        break;
    }
}

void MouseSwipeMotionMgr::onPressed(const QPointF &startPos, qint64 time)
{
    m_recognizer->startSwipeMotion(startPos, time);
}

void MouseSwipeMotionMgr::onMotion(const QPointF &startPos, qint64 time)
{
    m_recognizer->updateSwipeMotion(startPos, time);
}

void MouseSwipeMotionMgr::onRelease(const QPointF &startPos, qint64 time)
{
    m_recognizer->endSwipeMotion(startPos, time);
}

}
