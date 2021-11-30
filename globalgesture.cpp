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
#include <QDBusInterface>
#include "screenedge.h"
#include "input.h"

#include "abstract_client.h"
#include "wayland_server.h"
#include "workspace.h"
#include "screens.h"
#include "taskmanager.h"

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
    Q_UNUSED(pid)
}

void GlobalGesture::pinchGestureBegin(int fingerCount, quint32 time)
{
    Q_UNUSED(fingerCount)
    Q_UNUSED(time)
}

void GlobalGesture::pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time)
{
    Q_UNUSED(scale)
    Q_UNUSED(angleDelta)
    Q_UNUSED(delta)
    Q_UNUSED(time)
}

void GlobalGesture::pinchGestureEnd(quint32 time)
{
    Q_UNUSED(time)
}

void GlobalGesture::pinchGestureCancelled(quint32 time)
{
    Q_UNUSED(time)
}

void GlobalGesture::swipeGestureBegin(int fingerCount, quint32 time, bool isTouch)
{
    Q_UNUSED(isTouch);
    if (fingerCount == 3) {
        _swipeTime = time;
        _swipeStarted = true;
        _lastSpead = QSize(0., 0.);
        _progress = 0.;
        _upGesture = false;

        taskManager->setTaskState(TaskManager::TS_Prepare);
    } else if (fingerCount == 4) {
        _screenShotStart = true;
    }
}

void GlobalGesture::swipeGestureUpdate(const QSizeF &delta, quint32 time, bool isTouch)
{
    Q_UNUSED(isTouch);
    if (_swipeStarted) {
        _swipeDelta += delta;

        if (time != _swipeTime) {
            _lastSpead = delta / (time - _swipeTime);
        }
        _swipeTime = time;
        // _progress = std::abs(_swipeDelta.height())/(screens()->size(0).height() - 50);
        // taskManager->updateMove(delta, std::abs(_swipeDelta.height())/(screens()->size(0).height() - 50));


        _swipeDelta -= delta;
        qreal limitation = 140;
        if(_swipeDelta.height() + delta.height()>=limitation) {
            _swipeDelta.setHeight(limitation);
            QSize d(delta.width(), limitation-_swipeDelta.height());
            taskManager->updateMove(d, limitation/(screens()->size(0).height() - 50));
        } else {
            _swipeDelta += delta;
            _progress = std::abs(_swipeDelta.height())/(screens()->size(0).height() - 50);
            taskManager->updateMove(delta, _progress);
        }
    } else if (_screenShotStart) {
        _swipeDelta += delta;
        if(_swipeDelta.height()>400) {
            takeScreenShot();
            _screenShotStart = false;
            _swipeDelta = QSizeF(0.0f, 0.0f);
        }
    }
}

void GlobalGesture::swipeGestureEnd(quint32 time, bool isTouch)
{
    Q_UNUSED(time);
    Q_UNUSED(isTouch)

    if (_swipeStarted) {
        taskManager->onGestueEnd(_swipeDelta, _lastSpead);
        _swipeStarted = false;
    }
    _screenShotStart  = false;
    _swipeDelta = QSizeF(0.0f, 0.0f);
    _swipeTime = 0;
    _progress = 0.;
}

void GlobalGesture::takeScreenShot()
{
    QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/org/jingos/screenshot"), QStringLiteral("org.jingos.screenshot"), QStringLiteral("screenshot"));
    QDBusConnection::sessionBus().send(message);
}

void GlobalGesture::minimumWindow()
{
    if (nullptr != Workspace::self() && nullptr != Workspace::self()->activeClient()&& waylandServer() && !waylandServer()->isScreenLocked()) {
        Workspace::self()->activeClient()->minimize(false);
    }
}


void GlobalGesture::swipeGestureCancelled(quint32 time, bool isTouch)
{
    Q_UNUSED(time)
    Q_UNUSED(isTouch)
    if (_swipeStarted) {
        taskManager->onGestueEnd(_swipeDelta, _lastSpead);
        _swipeStarted = false;
    }
    _screenShotStart  = false;
    _swipeDelta = QSizeF(0.0f, 0.0f);
    _swipeTime = 0;
}
}
