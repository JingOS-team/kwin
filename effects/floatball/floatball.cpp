/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "floatball.h"

#include <QTimer>
#include <QQuickItem>
#include <QQmlContext>
#include <QMouseEvent>
#include <KConfigGroup>
#include <QMouseEvent>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>

#include <unistd.h>

#include "kwinglutils.h"
#include "kwingltexture.h"
#include "input_event.h"

namespace KWin
{

const int BALL_WIDTH = 79;
const int BALL_HEIGHT = 79;
const int BALL_DEFAULT_LEFT_MARGIN = 40;
const int BALL_DEFAULT_BOTTOM_MARGIN = 40;

#define RIGHT_EDGE_OFF_BALL(screen_left) (screen_left - BALL_WIDTH)
#define BOTTOM_EDGE_OFF_BALL(screen_bottom) (screen_bottom - BALL_WIDTH)
QString g_sogo_service_name = "";

FloatBall::FloatBall()
{
    if (!_closeTexture) {
        QImage image("/usr/share/kwin_icons/task/jianpan.png");

        _closeTexture.reset(new GLTexture(image));
        _closeTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    }

    QRect screenGeom = effects->clientArea(ScreenArea, 0, 0);

    auto config = KSharedConfig::openConfig(QStringLiteral("kwindisplayrc"))->group("Display");
    float x = config.readEntry("x", RIGHT_EDGE_OFF_BALL(screenGeom.right()) - BALL_DEFAULT_LEFT_MARGIN);
    float y = config.readEntry("y", BOTTOM_EDGE_OFF_BALL(screenGeom.bottom())- BALL_DEFAULT_BOTTOM_MARGIN);

    _ballGeometry = QRect(x, y, BALL_WIDTH, BALL_HEIGHT);

    _timer = new QTimer();
    _timer->setInterval(200);
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, [this]() {
        _waitTrigger = false;
    });

    QString displayName = effects->getDisplayName();
    g_sogo_service_name = QString("com.basesogouimebs_service.hotel_%1_%2").arg(getuid()).arg(displayName.replace(":", ""));
    _sogoServiceInterface = new QDBusInterface ( g_sogo_service_name, "/", "com.basesogouimebs_interface_service" );
    connect(effects, &EffectsHandler::displayNameChanged, this, [=](const QString &displayName) {
        delete _sogoServiceInterface;
        QString _displayName = displayName;
        g_sogo_service_name = QString("com.basesogouimebs_service.hotel_%1_%2").arg(getuid()).arg(_displayName.replace(":", ""));
        _sogoServiceInterface = new QDBusInterface ( g_sogo_service_name, "/", "com.basesogouimebs_interface_service" );
    });

    connect(effects, &EffectsHandler::windowActivated,
            this, [this] () {
        showInput(false);
    });

    QDBusPendingCall pcall = _sogoServiceInterface->asyncCall("HasFocus");

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [=](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<bool> reply = *call;
        if (reply.isError()) {
            qDebug()<<"showInput Error:"<<reply.error().message();
        }
        call->deleteLater();
        _hasInputFocus = reply.value();
        qDebug()<<"yanggx focusChanged _hasInputFocus:"<<_hasInputFocus;
    });
}

FloatBall::~FloatBall()
{
    delete _timer;
    delete _sogoServiceInterface;
}

void FloatBall::postPaintScreen()
{
    effects->postPaintScreen();
    effects->renderTexture(_closeTexture.data(), infiniteRegion(), _ballGeometry.toRect());
}

bool FloatBall::isActive() const
{
    return !effects->isScreenLocked() && effects->showFloatBall() && taskManager->isInitState();
}

bool FloatBall::supported()
{
    return true;
}

bool FloatBall::enabledByDefault()
{
    return true;
}

bool FloatBall::pointerEvent(QMouseEvent *e)
{
    if (!isActive()) {
        return false;
    }

    if ((e->type() != QEvent::MouseMove
         && e->type() != QEvent::MouseButtonPress
         && e->type() != QEvent::MouseButtonRelease))  // Block user input during animations
        return false;

    if (e->type() == QEvent::MouseButtonPress && e->buttons() == Qt::LeftButton) {
        if (_isTouchDown || !_ballGeometry.contains(e->pos())) {
            return false;
        }

        _lastTouchPos = e->pos();
        _isTouchDown = true;
        _waitTrigger = true;
        _timer->start();
        return true;
    } else if (e->type() == QEvent::MouseButtonRelease && e->button() == Qt::LeftButton) {
        if (!_isTouchDown) {
            return false;
        }

        if (_waitTrigger) {
            showInput(true);
        }
        _lastTouchPos = QPointF(0, 0);
        _isTouchDown = false;
        effects->addRepaintFull();
        return true;
    } else if (e->type() == QEvent::MouseMove) {
        if (!_isTouchDown) {
            return false;
        }

        moveBall(e->pos() - _lastTouchPos);

        _lastTouchPos = e->pos();
        effects->addRepaintFull();
        return true;
    }

    return false;
}

bool FloatBall::tabletToolEvent(TabletEvent *e)
{
    if (!isActive()) {
        return false;
    }

    const auto pos = e->globalPosF();
    qint32 time = e->timestamp();

    switch (e->type()) {
    case QEvent::TabletMove: {
        QMouseEvent event(QEvent::MouseMove, pos, e->pressure() > 0 ? Qt::LeftButton : Qt::NoButton, e->pressure() > 0 ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);

        event.setTimestamp(time);
        return pointerEvent(&event);
        break;
    }
    case QEvent::TabletEnterProximity: {
        break;
    }
    case QEvent::TabletLeaveProximity: {
        break;
    }
    case QEvent::TabletPress: {
        QMouseEvent event(QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        event.setTimestamp(time);
        return pointerEvent(&event);
        break;
    }
    case QEvent::TabletRelease: {
        QMouseEvent event(QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        event.setTimestamp(time);
        return  pointerEvent(&event);
        break;
    }
    default:
        break;
    }

    return false;
}

bool FloatBall::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (_isTouchDown || !_ballGeometry.contains(pos) || !isActive()) {
        return false;
    }

    _lastTouchPos = pos;
    _isTouchDown = true;
    _waitTrigger = true;
    _timer->start();
    return true;
}

bool FloatBall::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (!_isTouchDown || !isActive()) {
        return false;
    }

    moveBall(pos - _lastTouchPos);

    _lastTouchPos = pos;
    effects->addRepaintFull();
    return true;
}

bool FloatBall::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (!_isTouchDown || !isActive()) {
        return false;
    }

    if (_waitTrigger) {
        showInput(true);
    }
    _lastTouchPos = QPointF(0, 0);
    _isTouchDown = false;
    effects->addRepaintFull();
    return true;
}

void FloatBall::moveBall(const QPointF &delta)
{
    QRect screenGeom = effects->clientArea(ScreenArea, 0, 0);
    QRectF tmp =  _ballGeometry;
    tmp.translate(delta.x(), delta.y());
    if (tmp.x() >= 0 &&  tmp.x() <= RIGHT_EDGE_OFF_BALL(screenGeom.right())) {
        _ballGeometry.translate(delta.x(), 0);
    }

    if (tmp.y() >= 0 && tmp.y() <= BOTTOM_EDGE_OFF_BALL(screenGeom.bottom())) {
        _ballGeometry.translate(0, delta.y());
    }
}

void FloatBall::showInput(bool show)
{
    QDBusPendingCall pcall = _sogoServiceInterface->asyncCall("ShowKeyboard", show);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<QString, QByteArray> reply = *call;
        if (reply.isError()) {
            qDebug()<<"showInput Error:"<<reply.error().message();
        }
        call->deleteLater();
        effects->addRepaintFull();
    });
}

}
