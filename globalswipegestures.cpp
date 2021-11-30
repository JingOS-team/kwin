/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "globalswipegestures.h"

// KF5
#include <KLocalizedString>

#include "screenedge.h"
#include "input.h"

#include "abstract_client.h"
#include <workspace.h>
#include <QDBusInterface>
#include "wayland_server.h"
#include "globalgesture.h"
#include <QRect>
#include <QDebug>
#include "effects.h"
#include "taskmanager.h"

namespace KWin {

#if defined (__arm64__) || defined (__aarch64__)
const int MOTION_BORDER_WIDTH = 2;
const int GESTURE_BORDER_WIDTH = 2;
const int BORDER_SIZE = 5;
#else
const int MOTION_BORDER_WIDTH = 25;
const int GESTURE_BORDER_WIDTH = 25;
const int BORDER_SIZE = 50;
#endif
const qreal GESTURE_MINIMUM_DELTA_RATE = 0.06f;
QRect topRect(QSizeF screenSize, bool gesture) {
    Q_UNUSED(gesture)
    return  QRect(0, 0, screenSize.width(), BORDER_SIZE);
}
QRect rightRect(QSizeF screenSize, bool gesture) {
    Q_UNUSED(gesture)
    return QRect(screenSize.width() - BORDER_SIZE, 0, BORDER_SIZE, screenSize.height() - 25);
}
QRect bottomRect(QSizeF screenSize, bool gesture) {
    Q_UNUSED(gesture)
    return QRect(0, screenSize.height() - BORDER_SIZE, screenSize.width(), BORDER_SIZE);
}
QRect leftRect(QSizeF screenSize, bool gesture) {
    Q_UNUSED(gesture)
    return QRect(0, 0, BORDER_SIZE, screenSize.height() - 25);
}

GlobalSwipeGestures::GlobalSwipeGestures(QObject *parent)
    : QObject(parent)
    , m_backGesture(new SwipeGesture(this))
    , m_minimumGesture(new SwipeGesture(this))
    , m_bottomGesture(new SwipeGesture(this))
    , m_screenShotGesture(new SwipeGesture(this))
    , m_3FingersSwipeGesture(new SwipeGesture(this))
    , m_backMotion(new MouseMotion(this))
    , m_minimumMotion(new MouseMotion(this))
    , m_bottomMotion(new MouseMotion(this))
{
    _closeWindowTimer.setInterval(2 * 1000);
    _closeWindowTimer.setSingleShot(true);
    connect(&_closeWindowTimer, &QTimer::timeout, this, [this]() {
        _hasTriggerClose = false;
    });
}


void GlobalSwipeGestures::setScreenSize(QSize screenSize)
{
    m_screenSize = screenSize;
    initBackGesture();
    initMinimumGesture();
    initBottomGesture();

    initBackMotion();
    initMinimumMotion();
    initBottomMotion();

    init3FingersGesture();

    initScreenShotGesture();
}

void GlobalSwipeGestures::initGesture(SwipeGesture *gesture, const QRect& startGeometry, const QSize& miniDelta, qreal angle, SwipeGesture::Direction direction)
{
    gesture->setMinimumFingerCount(1);
    gesture->setMaximumFingerCount(1);

    gesture->setStartGeometry(startGeometry);
    gesture->setMinimumDelta(miniDelta);

    gesture->setMinimumSwipeAngle(angle);

    gesture->setDirection(direction);
}

void GlobalSwipeGestures::initMotion(MouseMotion *motion, const QRect &startGeometry, const QSize &miniDelta, MouseMotion::Direction direction)
{
    motion->setStartGeometry(startGeometry);
    motion->setMinimumDelta(miniDelta);

    motion->setDirection(direction);
}

void GlobalSwipeGestures::registerGestrure(GestureRecognizer *recongnizer)
{
    if (nullptr == recongnizer) {
        return;
    }

    recongnizer->registerGesture(m_backGesture);
    recongnizer->registerGesture(m_minimumGesture);
    recongnizer->registerGesture(m_bottomGesture);
    recongnizer->registerGesture(m_3FingersSwipeGesture);
    recongnizer->registerGesture(m_screenShotGesture);
}

void GlobalSwipeGestures::unregisterGesture(GestureRecognizer *recongnizer)
{
    if (nullptr == recongnizer) {
        return;
    }

    recongnizer->unregisterGesture(m_backGesture);
    recongnizer->unregisterGesture(m_bottomGesture);
    recongnizer->unregisterGesture(m_minimumGesture);
    recongnizer->unregisterGesture(m_3FingersSwipeGesture);
    recongnizer->unregisterGesture(m_screenShotGesture);
}

void GlobalSwipeGestures::registerMotion()
{
    if (MouseSwipeMotionMgr::self() == nullptr || MouseSwipeMotionMgr::self()->getMouseMotionRecognizer() == nullptr) {
        return;
    }

//    MouseSwipeMotionMgr::self()->getMouseMotionRecognizer()->registerMotion(m_backMotion);
//    MouseSwipeMotionMgr::self()->getMouseMotionRecognizer()->registerMotion(m_minimumMotion);
//    MouseSwipeMotionMgr::self()->getMouseMotionRecognizer()->registerMotion(m_bottomMotion);
}

void GlobalSwipeGestures::unregisterMotion()
{

    if (MouseSwipeMotionMgr::self() == nullptr || MouseSwipeMotionMgr::self()->getMouseMotionRecognizer() == nullptr) {
        return;
    }

//    MouseSwipeMotionMgr::self()->getMouseMotionRecognizer()->unregisterMotion(m_backMotion);
//    MouseSwipeMotionMgr::self()->getMouseMotionRecognizer()->unregisterMotion(m_minimumMotion);
//    MouseSwipeMotionMgr::self()->getMouseMotionRecognizer()->unregisterMotion(m_bottomMotion);
}

void GlobalSwipeGestures::initBackGesture()
{
    QSize size(m_screenSize.width(), 0);
    initGesture(m_backGesture, leftRect(m_screenSize, true), size * GESTURE_MINIMUM_DELTA_RATE, 25, SwipeGesture::Direction::All);

    connect(m_backGesture, &SwipeGesture::progress, this, &GlobalSwipeGestures::onBackProcess, Qt::QueuedConnection);
    connect(m_backGesture, &Gesture::cancelled, this, &GlobalSwipeGestures::onBackCancelled, Qt::QueuedConnection);
    connect(m_backGesture, &Gesture::triggered, this, &GlobalSwipeGestures::onBackTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::initMinimumGesture()
{
    QSize size(m_screenSize.width(), 0);
    initGesture(m_minimumGesture, rightRect(m_screenSize, true), size * GESTURE_MINIMUM_DELTA_RATE, 25, SwipeGesture::Direction::All);

    connect(m_minimumGesture, &SwipeGesture::progress, this, &GlobalSwipeGestures::onMinimumProcess, Qt::QueuedConnection);
    connect(m_minimumGesture, &Gesture::cancelled, this, &GlobalSwipeGestures::onMinimumCancelled, Qt::QueuedConnection);
    connect(m_minimumGesture, &Gesture::triggered, this, &GlobalSwipeGestures::onMinimumTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::initBottomGesture()
{
    m_bottomGesture->setMinimumFingerCount(1);
    m_bottomGesture->setMaximumFingerCount(1);

    m_bottomGesture->setStartGeometry(bottomRect(m_screenSize, true));
    m_bottomGesture->setMinimumDelta(m_screenSize - QSizeF(0, 10));

    m_bottomGesture->setDirection(SwipeGesture::Direction::Up);

    connect(m_bottomGesture, &SwipeGesture::started, this, &GlobalSwipeGestures::onBottomStart, Qt::QueuedConnection);
    connect(m_bottomGesture, &SwipeGesture::update, this, &GlobalSwipeGestures::onBottomUpdate, Qt::QueuedConnection);
    connect(m_bottomGesture, &Gesture::cancelled, this, &GlobalSwipeGestures::onBottomCancelled, Qt::QueuedConnection);
    connect(m_bottomGesture, &Gesture::triggered, this, &GlobalSwipeGestures::onBottomTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::init3FingersGesture()
{
    m_3FingersSwipeGesture->setMinimumFingerCount(3);
    m_3FingersSwipeGesture->setMaximumFingerCount(4);
    m_3FingersSwipeGesture->setDirection(SwipeGesture::Direction::All);
    m_3FingersSwipeGesture->setMinimumDelta(m_screenSize * 0.5);

    connect(m_3FingersSwipeGesture, &SwipeGesture::started, this, &GlobalSwipeGestures::on3FingersSwipeStarted, Qt::QueuedConnection);
    connect(m_3FingersSwipeGesture, &SwipeGesture::update, this, &GlobalSwipeGestures::on3FingersSwipeUpdate, Qt::QueuedConnection);
    connect(m_3FingersSwipeGesture, &Gesture::cancelled, this, &GlobalSwipeGestures::on3FingersSwipeCancelled, Qt::QueuedConnection);
    connect(m_3FingersSwipeGesture, &Gesture::triggered, this, &GlobalSwipeGestures::on3FingersSwipeTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::initScreenShotGesture()
{
    m_screenShotGesture->setMinimumFingerCount(3);
    m_screenShotGesture->setMaximumFingerCount(4);
    m_screenShotGesture->setDirection(SwipeGesture::Direction::Down);
    m_screenShotGesture->setMinimumDelta(m_screenSize * 0.2);
    m_screenShotGesture->setStartGeometry(QRect(QPoint(0, 0), QSize(m_screenSize.width(),  m_screenSize.height()/ 6)));

    connect(m_screenShotGesture, &Gesture::triggered, this, []() {
        QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/org/jingos/screenshot"), QStringLiteral("org.jingos.screenshot"), QStringLiteral("screenshot"));
        QDBusConnection::sessionBus().send(message);
    }, Qt::QueuedConnection);
}

void GlobalSwipeGestures::initBackMotion()
{
    initMotion(m_backMotion, leftRect(m_screenSize, false), m_screenSize * GESTURE_MINIMUM_DELTA_RATE, MouseMotion::Direction::Right);

    connect(m_backMotion, &MouseMotion::progress, this, &GlobalSwipeGestures::onBackProcess, Qt::QueuedConnection);
    connect(m_backMotion, &Gesture::cancelled, this, &GlobalSwipeGestures::onBackCancelled, Qt::QueuedConnection);
    connect(m_backMotion, &MouseMotion::motionTriggered, this, &GlobalSwipeGestures::onBackMotionTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::initMinimumMotion()
{
    initMotion(m_minimumMotion, rightRect(m_screenSize, false), m_screenSize * GESTURE_MINIMUM_DELTA_RATE, MouseMotion::Direction::Left);

    connect(m_minimumMotion, &MouseMotion::progress, this, &GlobalSwipeGestures::onMinimumProcess, Qt::QueuedConnection);
    connect(m_minimumMotion, &Gesture::cancelled, this, &GlobalSwipeGestures::onMinimumCancelled, Qt::QueuedConnection);
    connect(m_minimumMotion, &MouseMotion::motionTriggered, this, &GlobalSwipeGestures::onMinimumMotionTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::initBottomMotion()
{
    initMotion(m_bottomMotion, bottomRect(m_screenSize, false), m_screenSize * GESTURE_MINIMUM_DELTA_RATE, MouseMotion::Direction::Up);

    connect(m_bottomMotion, &MouseMotion::progress, this, &GlobalSwipeGestures::onBottomMotionProcess, Qt::QueuedConnection);
    connect(m_bottomMotion, &Gesture::cancelled, this, &GlobalSwipeGestures::onBottomMotionCancelled, Qt::QueuedConnection);
    connect(m_bottomMotion, &MouseMotion::motionTriggered, this, &GlobalSwipeGestures::onBottomMotionTriggered, Qt::QueuedConnection);
}

void GlobalSwipeGestures::onBackTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(lastSpeed);
    // back(time);
    onSideGestureTrigger(time);
}

void GlobalSwipeGestures::onBackProcess(qreal delta, quint32 time)
{
    Q_UNUSED(delta);
    Q_UNUSED(time);
}

void GlobalSwipeGestures::onBackCancelled(quint32 time)
{
    Q_UNUSED(time);
}

void GlobalSwipeGestures::onBottomStart(quint32 time)
{
    Q_UNUSED(time)
    _swipeDelta = QSizeF(0.0f, 0.0f);
    taskManager->setTaskState(TaskManager::TS_Prepare);
}

void GlobalSwipeGestures::onBottomTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time);
    taskManager->onGestueEnd(_swipeDelta, QSizeF(0, lastSpeed));
    _swipeDelta = QSizeF(0.0f, 0.0f);
}

void GlobalSwipeGestures::onBottomUpdate(QSizeF delta, qreal progress, quint32 time)
{
    Q_UNUSED(time);
    _swipeDelta += delta;
    taskManager->updateMove(delta, progress);
}

void GlobalSwipeGestures::onBottomCancelled(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time);
    taskManager->onGestueEnd(_swipeDelta, QSizeF(0, lastSpeed));
    _swipeDelta = QSizeF(0.0f, 0.0f);
}

void GlobalSwipeGestures::onMinimumTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(lastSpeed);
    //if (lastSpeed > 0.4)
        onSideGestureTrigger(time);
}

void GlobalSwipeGestures::onMinimumProcess(qreal delta, quint32 time)
{
    Q_UNUSED(delta);
    Q_UNUSED(time);
}

void GlobalSwipeGestures::onMinimumCancelled(quint32 time)
{
    Q_UNUSED(time);
}

void GlobalSwipeGestures::onBackMotionTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(lastSpeed);
     onSideGestureTrigger(time);
}

void GlobalSwipeGestures::onBottomMotionTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time);
    if (std::abs(lastSpeed) > 0.7) {
        showDesktop();
    } else {
        showTaskPanel();
    }
}

void GlobalSwipeGestures::onBottomMotionProcess(qreal delta, quint32 time)
{
    Q_UNUSED(delta);
    Q_UNUSED(time);
}

void GlobalSwipeGestures::onBottomMotionCancelled(quint32 time)
{
    Q_UNUSED(time);

}

void GlobalSwipeGestures::on3FingersSwipeStarted(quint32 time)
{
    Q_UNUSED(time);
    _swipeStarted = true;
    _swipeDelta = QSizeF(0.0f, 0.0f);
}

void GlobalSwipeGestures::on3FingersSwipeTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time)
    Q_UNUSED(lastSpeed);
    if (_3FingerType == TORIGHT || _3FingerType == TOLEFT) {
        taskManager->onGestueEnd(_swipeDelta, QSizeF(0, lastSpeed));
    } else if (_3FingerType == TOBOTTOM) {

    }
    _progress = 0;
    _swipeStarted = false;
    _swipeDelta = QSizeF(0.0f, 0.0f);
    _3FingerType = UNKNOWN;
}

void GlobalSwipeGestures::on3FingersSwipeUpdate(QSizeF delta, qreal progress, quint32 time)
{
    Q_UNUSED(time);
    _progress = progress;
    if (_swipeStarted) {
        _swipeDelta += delta;
    }
    if (progress >= 0.05 && _3FingerType == UNKNOWN) {
        if (std::abs(_swipeDelta.width()) > std::abs(_swipeDelta.height())) {
            if (_swipeDelta.width() > 0) {
                _3FingerType = TORIGHT;
            } else {
                _3FingerType = TOLEFT;
            }
            taskManager->setTaskState(TaskManager::TS_Prepare);

        } else {
            if (_swipeDelta.height() > 0) {
                _3FingerType = TOBOTTOM;
            } else {
                _3FingerType = TOTOP;
            }
        }
    }
    if (_3FingerType == TORIGHT || _3FingerType == TOLEFT) {
        taskManager->updateMove(delta, progress > 0.1 ? 0.1 : progress);
    }
}

void GlobalSwipeGestures::on3FingersSwipeCancelled(quint32 time)
{
    Q_UNUSED(time);
    if (_3FingerType == TORIGHT || _3FingerType == TOLEFT) {
        taskManager->onGestueEnd(_swipeDelta, QSizeF(0, 0));
    } else if (_3FingerType == TOBOTTOM && _progress > 0.3) {

    }
    _progress = 0;
    _swipeStarted = false;
    _swipeDelta = QSizeF(0.0f, 0.0f);
    _3FingerType = UNKNOWN;
}

void GlobalSwipeGestures::onSideGestureTrigger(quint32 time)
{
    if (Workspace::self() && Workspace::self()->isTopClientJingApp()) {
        back(time);
        if (_hasTriggerClose) {
            hideCloseNotice();
        }
    } else {
        closeWindow();
    }
}

void GlobalSwipeGestures::closeWindow()
{
    if (_hasTriggerClose) {
        hideCloseNotice();
        if (nullptr != Workspace::self() && nullptr != Workspace::self()->activeClient()&& waylandServer() && !waylandServer()->isScreenLocked() && !Workspace::self()->activeClient()->isDesktop()) {
            Workspace::self()->activeClient()->closeWindow();
        }
    } else if (Workspace::self()->activeClient()->isCloseable()){
        showCloseNotice();
    }
}

void GlobalSwipeGestures::onMinimumMotionTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time);
    Q_UNUSED(lastSpeed);
    minimumWindow();
}

void GlobalSwipeGestures::minimumWindow()
{
    if (nullptr != Workspace::self() && nullptr != Workspace::self()->activeClient()&& waylandServer() && !waylandServer()->isScreenLocked()) {
        Workspace::self()->activeClient()->minimize(false);
    }
}

void GlobalSwipeGestures::showDesktop()
{
    if (nullptr != Workspace::self() && waylandServer() && !waylandServer()->isScreenLocked()) {
        Workspace::self()->minimizeAllWindow();
    }
}

void GlobalSwipeGestures::back(quint32 time)
{
     input()->forwardBackKey(time);
}

void GlobalSwipeGestures::showTaskPanel()
{
    if (!waylandServer() || waylandServer()->isScreenLocked()) {
        return;
    }
    QDBusInterface iface( "org.kde.plasma.taskmanager", "/taskmanager", "org.kde.plasma.taskmanager", QDBusConnection::sessionBus());
    if (!iface.isValid()) {
       qDebug()<<Q_FUNC_INFO<< qPrintable(QDBusConnection::sessionBus(). lastError().message());
       return;
    }

    iface.asyncCall("setActivity", true);
}

void GlobalSwipeGestures::showCloseNotice()
{
    _hasTriggerClose = true;
    QDBusInterface iface( "org.jingos.toast", "/org/jingos/toast", "org.jingos.toast", QDBusConnection::sessionBus());
    if (!iface.isValid()) {
       qDebug()<<Q_FUNC_INFO<< qPrintable(QDBusConnection::sessionBus(). lastError().message());
       return;
    }

    iface.asyncCall("showText", i18n("Press again to close the window"));
    _closeWindowTimer.start();
}

void GlobalSwipeGestures::hideCloseNotice()
{
    _closeWindowTimer.stop();
    _hasTriggerClose = false;
}
}
