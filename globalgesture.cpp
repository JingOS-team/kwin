/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "globalgesture.h"
#include "globalgesturetypes.h"
#include <QDebug>
#include <QTimer>
#include <QSizeF>
#include "screenedge.h"
#include "input.h"

#include "abstract_client.h"
#include "wayland_server.h"
#include "workspace.h"

#include "effects.h"
namespace KWin {
extern KWINEFFECTS_EXPORT EffectsHandler* effects;
GlobalGesture* GlobalGesture::_self = new GlobalGesture();
const QDataStream::Version QDATA_STREAM_VERSION = QDataStream::Qt_5_14;
GlobalGesture::GlobalGesture(QObject *parent)
    : QObject(parent)
{
}

void GlobalGesture::init(int pid)
{

}

void GlobalGesture::pinchGestureBegin(int fingerCount, quint32 time)
{

}

void GlobalGesture::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{

}

void GlobalGesture::pinchGestureEnd(quint32 time)
{

}

void GlobalGesture::pinchGestureCancelled(quint32 time)
{

}

void GlobalGesture::swipeGestureBegin(int fingerCount, quint32 time, bool isTouch)
{
    if (fingerCount == 3 || fingerCount == 4) {
        _swipeTime = time;
        _swipeStarted = true;
    }
}

void GlobalGesture::swipeGestureUpdate(const QSizeF &delta, quint32 time, bool isTouch)
{
    if (_swipeStarted) {
        _swipeDelta += delta;
        if (std::abs(_swipeDelta.height()) > 50 || std::abs(_swipeDelta.width()) > 100) {
            swipeGestureEnd(time, isTouch);
        }
    }
}

void GlobalGesture::swipeGestureEnd(quint32 time, bool isTouch)
{
    if (_swipeStarted) {
        qreal rate = std::abs(_swipeDelta.width()) / std::abs(_swipeDelta.height());
        if (std::abs(_swipeDelta.width()) / std::abs(_swipeDelta.height()) > 1.2) {
            effects->onTaskSwipe(_swipeDelta.width() > 0);
        } else if (!isTouch) {
            qreal delta = std::abs(_swipeDelta.width()) + std::abs(_swipeDelta.height());
            if (delta / (time - _swipeTime) > 1.0 && _swipeDelta.height() < 0) {
                if (nullptr != Workspace::self() && waylandServer() && !waylandServer()->isScreenLocked()) {
                    Workspace::self()->minimizeAllWindow();
                    ScreenEdges::self()->hideTaskManager();
                }
            } else {
                ScreenEdges::self()->onBottomGesture();
            }
        }
         _swipeStarted = false;
    }


    _swipeDelta = QSizeF(0.0f, 0.0f);
    _swipeTime = 0;
}

void GlobalGesture::minimumWindow()
{
    if (nullptr != Workspace::self() && nullptr != Workspace::self()->activeClient()&& waylandServer() && !waylandServer()->isScreenLocked()) {
        Workspace::self()->activeClient()->minimize(false);
    }
}


void GlobalGesture::swipeGestureCancelled(quint32 time, bool isTouch)
{
    if (_swipeStarted) {
        qreal rate = std::abs(_swipeDelta.width()) / std::abs(_swipeDelta.height());
        if (std::abs(_swipeDelta.width()) / std::abs(_swipeDelta.height()) > 1.2) {
            effects->onTaskSwipe(_swipeDelta.width() > 0);
        } else if (!isTouch) {
            qreal delta = std::abs(_swipeDelta.width()) + std::abs(_swipeDelta.height());
            if (delta / (time - _swipeTime) > 2.0 && _swipeDelta.height() < 0) {
                if (nullptr != Workspace::self() && waylandServer() && !waylandServer()->isScreenLocked()) {
                    Workspace::self()->setShowingDesktop(true);
                    ScreenEdges::self()->hideTaskManager();
                }
            } else {
                ScreenEdges::self()->onBottomGesture();
            }
        }
        _swipeStarted = false;
    }

    _swipeDelta = QSizeF(0.0f, 0.0f);
    _swipeTime = 0;
}
}
