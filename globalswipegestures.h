/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GLOBALSWIPGESTUREMGR_H
#define GLOBALSWIPGESTUREMGR_H

#include <QObject>
#include <QSize>
#include <QTimer>

#include "gestures.h"
#include "globalmotions.h"

namespace KWin {
class SwipeGesture;
class GestureRecognizer;

class GlobalSwipeGestures : public QObject
{
    Q_OBJECT
public:
    explicit GlobalSwipeGestures(QObject *parent = nullptr);

    void registerGestrure(GestureRecognizer *recongnizer);
    void unregisterGesture(GestureRecognizer *recongnizer);

    void registerMotion();
    void unregisterMotion();

    void setScreenSize(QSize screenSize);
protected:
    void initGesture(SwipeGesture *gesture);
    void initBackGesture();
    void initMinimumGesture();
    void initBottomGesture();

    void init3FingersGesture();

    void initBackMotion();
    void initMinimumMotion();
    void initBottomMotion();

    void initGesture(SwipeGesture *gesture, const QRect &startGeometry, const QSize &miniDelta, SwipeGesture::Direction direction);
    void initMotion(MouseMotion *motion, const QRect &startGeometry, const QSize &miniDelta, MouseMotion::Direction direction);
private slots:
    void onBackTriggered(quint32 time, const qreal &lastSpeed);
    void onBackProcess(qreal delta, quint32 time);
    void onBackCancelled(quint32 time);

    void onBottomTriggered(quint32 time, const qreal &lastSpeed);
    void onBottomProcess(qreal delta, quint32 time);
    void onBottomCancelled(quint32 time);

    void onMinimumTriggered(quint32 time, const qreal &lastSpeed);
    void onMinimumProcess(qreal delta, quint32 time);
    void onMinimumCancelled(quint32 time);

    void onMinimumMotionTriggered(quint32 time, const qreal &lastSpeed);
    void onBackMotionTriggered(quint32 time, const qreal &lastSpeed);
    void onBottomMotionTriggered(quint32 time, const qreal &lastSpeed);
    void onBottomMotionProcess(qreal delta, quint32 time);
    void onBottomMotionCancelled(quint32 time);

    void on3FingersSwipeStarted(quint32 time);
    void on3FingersSwipeTriggered(quint32 time, const qreal &lastSpeed);
    void on3FingersSwipeUpdate(QSizeF delta, quint32 time);
    void on3FingersSwipeCancelled(quint32 time);

Q_SIGNALS:
    void onBottomGestureToggled();
    void onHideTaskManager();
private:
    void onSideGestureTrigger(quint32 time);
    void closeWindow();
    void minimumWindow();
    void showDesktop();
    void back(quint32 time);
    void showTaskPanel();
    void showCloseNotice();
    void hideCloseNotice();
private:
    bool _hasTriggerClose = false;
    bool _swipeStarted = false;
    QSizeF _swipeDelta;
    QSize m_screenSize;
    SwipeGesture *m_backGesture = nullptr;
    SwipeGesture *m_minimumGesture = nullptr;
    SwipeGesture *m_bottomGesture = nullptr;

    SwipeGesture *m_3FingersSwipeGesture = nullptr;

    MouseMotion *m_backMotion = nullptr;
    MouseMotion *m_minimumMotion = nullptr;
    MouseMotion *m_bottomMotion = nullptr;

    QTimer _closeWindowTimer;
};
}

#endif // GLOBALSWIPGESTUREMGR_H
