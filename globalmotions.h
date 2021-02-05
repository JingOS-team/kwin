/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GLOBALMOUSESWIPEGESTURE_H
#define GLOBALMOUSESWIPEGESTURE_H
#include <QObject>
#include <QSizeF>
#include <QVector>
#include <QPointF>
#include "gestures.h"

class QMouseEvent;

namespace KWin {
class MouseMotion : public Gesture
{
    Q_OBJECT
public:
    enum class Direction {
        Down,
        Left,
        Up,
        Right
    };

    explicit MouseMotion(QObject *parent = nullptr);

    Direction direction() const {
        return m_direction;
    }
    void setDirection(Direction direction) {
        m_direction = direction;
    }

    void setMinimumX(int x) {
        m_minimumX = x;
        m_minimumXRelevant = true;
    }
    int minimumX() const {
        return m_minimumX;
    }
    bool minimumXIsRelevant() const {
        return m_minimumXRelevant;
    }
    void setMinimumY(int y) {
        m_minimumY = y;
        m_minimumYRelevant = true;
    }
    int minimumY() const {
        return m_minimumY;
    }
    bool minimumYIsRelevant() const {
        return m_minimumYRelevant;
    }

    void setMaximumX(int x) {
        m_maximumX = x;
        m_maximumXRelevant = true;
    }
    int maximumX() const {
        return m_maximumX;
    }
    bool maximumXIsRelevant() const {
        return m_maximumXRelevant;
    }
    void setMaximumY(int y) {
        m_maximumY = y;
        m_maximumYRelevant = true;
    }
    int maximumY() const {
        return m_maximumY;
    }
    bool maximumYIsRelevant() const {
        return m_maximumYRelevant;
    }
    void setStartGeometry(const QRect &geometry);

    QSizeF minimumDelta() const {
        return m_minimumDelta;
    }
    void setMinimumDelta(const QSizeF &delta) {
        m_minimumDelta = delta;
        m_minimumDeltaRelevant = true;
    }
    bool isMinimumDeltaRelevant() const {
        return m_minimumDeltaRelevant;
    }

    qreal minimumDeltaReachedProgress(const QSizeF &delta) const;
    bool minimumDeltaReached(const QSizeF &delta) const;
Q_SIGNALS:
        void progress(qreal, quint32);
        void motionTriggered(quint32 time, const qreal &lastSpeed);
private:
    Direction m_direction = Direction::Down;
    bool m_minimumXRelevant = false;
    int m_minimumX = 0;
    bool m_minimumYRelevant = false;
    int m_minimumY = 0;
    bool m_maximumXRelevant = false;
    int m_maximumX = 0;
    bool m_maximumYRelevant = false;
    int m_maximumY = 0;
    bool m_minimumDeltaRelevant = false;
    QSizeF m_minimumDelta;
};

class MouseSwipeMotionRecognizer : public QObject
{
    Q_OBJECT
public:
    explicit MouseSwipeMotionRecognizer(QObject *parent = nullptr);
    ~MouseSwipeMotionRecognizer() override;

    void registerMotion(MouseMotion *motion);
    void unregisterMotion(MouseMotion *motion);

    int startSwipeMotion(const QPointF &startPos, qint64 time);
    void updateSwipeMotion(const QPointF &pos, qint64 time);
    void cancelSwipeMotion(qint64 time);
    void endSwipeMotion(const QPointF &pos, qint64 time);

private:
    void cancelActiveSwipeMotions(quint32 time);
    QVector<MouseMotion*> m_motions;
    QVector<MouseMotion*> m_activeSwipeMotions;
    QMap<MouseMotion*, QMetaObject::Connection> m_destroyConnections;
    QPointF m_startPos;
    QPointF m_lastPos;
    QPointF m_endPos;
    quint64 m_lastUpdateTime = 0;
    quint64 m_lastEndUpdateTime = 0;
    qreal m_lastAverageSeed = 0.0f;
};

class MouseSwipeMotionMgr : public QObject
{
    Q_OBJECT
public:
    explicit MouseSwipeMotionMgr(QObject *parent = nullptr);
    virtual ~MouseSwipeMotionMgr();

    MouseSwipeMotionRecognizer* getMouseMotionRecognizer() {
        return m_recognizer;
    }

    void onPointEvent(QMouseEvent *event);

    static MouseSwipeMotionMgr* create(QObject *parent) {
        if (!_self)
            _self = new MouseSwipeMotionMgr(parent);
        return _self;
    }

    static MouseSwipeMotionMgr* self() {
        if (!_self)
            _self = new MouseSwipeMotionMgr(nullptr);
        return _self;
    }
protected:
    void onPressed(const QPointF &startPos, qint64 time);
    void onMotion(const QPointF &startPos, qint64 time);
    void onRelease(const QPointF &startPos, qint64 time);
private:
    MouseSwipeMotionRecognizer *m_recognizer;

    static MouseSwipeMotionMgr* _self;
};
}
#endif // GLOBALMOUSESWIPEGESTURE_H
