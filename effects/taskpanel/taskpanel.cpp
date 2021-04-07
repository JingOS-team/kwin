/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "taskpanel.h"
#include <math.h>
#include <QQuickItem>
#include <QQmlContext>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QMouseEvent>
#include <QAction>

namespace KWin
{
const qreal ROTATIONSCALE = 0.3;
const qreal ROTATIONSCALE_15 = ROTATIONSCALE / 15;
const qreal TARGETSCALEMULTIPLIER = std::pow(2.0f, 1/3);

const qreal MINI_SCALE_SHOW_LIST = 0.5;
const qreal MAX_SCALE_SHOW_LIST = 0.9;

const QString CLEAR_BTN_BG = "/usr/share/kwin_icons/task/jt_clear_%1.png";
const QString CLOSE_BTN_BG = "/usr/share/kwin_icons/task/jt_close_%1.png";

TaskPanel::TaskPanel()
    :  m_activateAction(new QAction(this))
    , m_activateAction2(new QAction(this))
{
    _resetTimer.setInterval(10 * 1000);
    connect(&_resetTimer, &QTimer::timeout, this, &TaskPanel::resetData);
    connect(&_taskList, &TaskList::isAnimatingChanged, this, &TaskPanel::animatingChanged);
    connect(taskManager, &TaskManager::move, this, &TaskPanel::onMove);
    connect(taskManager, &TaskManager::switchWidnow, this, &TaskPanel::slotSwitchWindow);
    connect(taskManager, &TaskManager::gestueEnd, this, &TaskPanel::slotGestueEnd);
    connect(taskManager, &TaskManager::taskStateChanged, this, &TaskPanel::onTaskStateChanged);

    connect(effects, &EffectsHandler::showingDesktopChanged, this, &TaskPanel::slotShowingDesktopChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &TaskPanel::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &TaskPanel::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowDeleted, this, &TaskPanel::slotWindowDeleted);

    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        if (keyboardGrab) {
            effects->ungrabKeyboard();
            keyboardGrab = false;
        }
        if (taskManager->getTaskState() == TaskManager::TS_Task) {
            closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
        }
    });

    QAction* a = m_activateAction;
    a->setObjectName(QStringLiteral("ShowTaskPanel"));
    a->setText(i18n("Show Task Panel"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
    _shortcut = KGlobalAccel::self()->shortcut(a);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F8, a);
    effects->registerTouchpadSwipeShortcut(SwipeDirection::Up, a);
    connect(a, &QAction::triggered, this, &TaskPanel::toggle);

    a = m_activateAction2;
    a->setObjectName(QStringLiteral("ShowTaskPanel_1"));
    a->setText(i18n("Show Task Panel "));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Escape);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::META + Qt::Key_Escape);
    _shortcut1 = KGlobalAccel::self()->shortcut(a);
    effects->registerGlobalShortcut(Qt::META + Qt::Key_Escape, a);
    connect(a, &QAction::triggered, this, &TaskPanel::toggle);

    QRect area = effects->clientArea(WorkArea, 0, effects->currentDesktop());
    _lastPos = area.topLeft();
}

void TaskPanel::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    _taskList.updateTime(presentTime);
    // We need to mark the screen windows as transformed. Otherwise the
    // whole screen won't be repainted, resulting in artefacts.
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, presentTime);
}

void TaskPanel::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    if (taskManager->getTaskState() == TaskManager::TS_Task) {
        for (EffectQuickScene *view : m_clearButtons) {
            //view->rootItem()->setOpacity(_animateProgress);
            effects->renderEffectQuickView(view);
        }
    }
}

void TaskPanel::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (_isActive) {
        if (w->isNormalWindow() || w->isDialog())
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

        if (w->isMinimized())
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);

        data.mask |= PAINT_WINDOW_TRANSFORMED;

        // Split windows at screen edges
        for (int screen = 0; screen < effects->numScreens(); screen++) {
            QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
            if (w->x() < screenGeom.x())
                data.quads = data.quads.splitAtX(screenGeom.x() - w->x());
            if (w->x() + w->width() > screenGeom.x() + screenGeom.width())
                data.quads = data.quads.splitAtX(screenGeom.x() + screenGeom.width() - w->x());
            if (w->y() < screenGeom.y())
                data.quads = data.quads.splitAtY(screenGeom.y() - w->y());
            if (w->y() + w->height() > screenGeom.y() + screenGeom.height())
                data.quads = data.quads.splitAtY(screenGeom.y() + screenGeom.height() - w->y());
        }
    }
    effects->prePaintWindow(w, data, presentTime);
}

void TaskPanel::slotGestueEnd(const QSizeF &delta, const QSizeF &spead)
{
    if (!_taskList.hasSetup()) {
        return;
    }

    if (taskManager->getTaskState() == TaskManager::TS_Move) {
       if (std::abs(delta.width()) > 200 && std::abs(delta.width()) > std::abs(delta.height()) * 1.2) {
            slotSwitchWindow(delta.width() > 0);
        } else if (spead.height() < -1.2) {
           showDesktop(false);
       } else {
            taskManager->setTaskState(TaskManager::TS_MoveEnd);
        }
    } else if (taskManager->getTaskState() == TaskManager::TS_Task && !_taskList.isResetingGrid() && !_isDragging) {
        _taskList.moveStop(spead);
        effects->addRepaintFull();
    }

    _motionProcessed = false;
}

void TaskPanel::toggle()
{
    if (taskManager->getTaskState() == TaskManager::TS_Task) {
        closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
    } else if (taskManager->getTaskState() == TaskManager::TS_None || taskManager->getTaskState() == TaskManager::TS_Move) {
        if (!_taskList.hasSetup()) {
            setupWindows(false);
        }

        if (isSystemUI(currentWindow()) && !_startMoveFromDesktop) {
            effects->setShowingDesktop(false);
            _startMoveFromDesktop = true;
        }

        toTaskGride(currentWindow());
    }
}

void TaskPanel::closeTask(EffectWindow *window)
{
    if (window == nullptr) {
        showDesktop(true);
    } else {
        _taskList.toNormalWindow(window);
        _activeWindow = window;
        effects->activateWindowWhithoutAnimation(_activeWindow);

        taskManager->setTaskState(TaskManager::TS_ToWindow);
    }

    QHash<EffectWindow*, WindowItem*> items = _taskList.getItems();
    for (auto it = items.begin(); it != items.end(); it++) {
        if (it.value()->w != window) {
            it.value()->w->setIsBackApp(true);
        }
    }
}

void TaskPanel::toTaskGride(EffectWindow * window)
{
    if (_taskList.getItems().size() > 0) {
        _taskList.toGrideModel(window);

        addClearBtn();

        taskManager->setTaskState(TaskManager::TS_TaskAnimating);
    } else {
        resetData();
    }
}


bool TaskPanel::isCloseState()
{
    return taskManager->getTaskState() == TaskManager::TS_ToWindow || taskManager->getTaskState() == TaskManager::TS_ToDesktop;
}

bool TaskPanel::isTaskPanelState()
{
    return taskManager->getTaskState() == TaskManager::TS_Task || taskManager->getTaskState() == TaskManager::TS_TaskAnimating;
}

void TaskPanel::slotSwitchWindow(bool toRight) {
    _motionProcessed = true;
    if (!_taskList.hasSetup()) {
        return;
    }

//    if (_lastScale < MINI_SCALE_SHOW_LIST) {
//        showDesktop(false);
//        return;
//    }

    bool swipeResult = true;

    if (!isTaskPanelState()) {
        taskManager->setTaskState(TaskManager::TS_Swip);
        if (toRight) {
            swipeResult = _taskList.nextPage(!isTaskPanelState());
        } else {
            swipeResult = _taskList.prePage(!isTaskPanelState());
        }
        if (!swipeResult && _startMoveFromDesktop) {
            showDesktop(true);
        }
        _resetTimer.start();
    }

    effects->addRepaintFull();
}

void TaskPanel::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (!_isActive) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    for (int screen = 0; screen < effects->numScreens(); screen++) {
        if (_taskList.isManageWindow(w) || _taskList.isRemoving(w)) {
            WindowItem *pItem = _taskList.getWindowItem(w);
            if (pItem == nullptr) {
                pItem = _taskList.getRemoveWindowItem(w);
            }
            if (pItem == nullptr) {
                return;
            }
            WindowQuadList screenQuads;
            foreach (const WindowQuad & quad, data.quads)
                screenQuads.append(quad);

            if (screenQuads.isEmpty())
                continue;

            WindowPaintData d = data;
            d.quads = screenQuads;
            d.setXScale(pItem->_curScale.width());
            d.setYScale(pItem->_curScale.height());
            d += QPointF(pItem->_curPos.x() - w->x(), pItem->_curPos.y() - w->y());
            effects->paintWindow(w, mask, effects->clientArea(ScreenArea, w->screen(), 0), d);
        } else {
            if (w->isDesktop()) {
                if (taskManager->getTaskState() == TaskManager::TS_ToDesktop) {
                    data.multiplyBrightness(_animateProgress * (1 - _curBrightness) + _curBrightness);
                } else if (isTaskPanelState() || taskManager->getTaskState() == TaskManager::TS_ToWindow) {
                    data.multiplyBrightness(0.2);
                } else if (taskManager->getTaskState() != TaskManager::TS_None && !_startMoveFromDesktop) {
                    _curBrightness = std::max(std::pow(1 - _moveProgress, 3), 0.2);
                    data.multiplyBrightness(_curBrightness);
                }
                effects->paintWindow(w, mask, region, data);
            }
        }
    }
}

void TaskPanel::postPaintWindow(EffectWindow *w)
{
    //w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

    if (effects->activeWindow() == nullptr || (w->pid() != effects->activeWindow()->pid() && w->windowClass().compare(effects->activeWindow()->windowClass()) != 0)) {
        if (w->isMinimized()) {
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        } else {
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }
    }
    effects->postPaintWindow(w);
}

QPointF TaskPanel::scalePos(const QPointF& pos, int screen, qreal scale) const
{
    if (screen == -1)
        screen = effects->screenNumber(pos.toPoint());
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

    QPointF point(pos.x() + screenGeom.width() * (1 - scale) / 2,
                  pos.y() + screenGeom.height() * (1 -scale) / 2);

    return point;
}

QSizeF TaskPanel::scaleSize(int screen, qreal scale) const
{
    QRect screenGeom = effects->clientArea(WorkArea, screen, 0);

    return QSizeF(screenGeom.width() * scale / 2, screenGeom.height() * scale / 2);
}

void TaskPanel::postPaintScreen()
{
    if (_isActive) {
        effects->addRepaintFull();
    }
    effects->postPaintScreen();
    if (_delayReset) {

        resetData();
        _delayReset = false;
    }
}

bool TaskPanel::isActive() const
{
    const_cast<TaskPanel*>(this)->_isActive = (taskManager->getTaskState() != TaskManager::TS_None)  && !effects->isScreenLocked();
    return _isActive;
}

void TaskPanel::windowInputMouseEvent(QEvent *e)
{
    if (taskManager->getTaskState() != TaskManager::TS_Task || _taskList.isResetingGrid()) {
        return;
    }

    if ((e->type() != QEvent::MouseMove
         && e->type() != QEvent::MouseButtonPress
         && e->type() != QEvent::MouseButtonRelease))  // Block user input during animations
        return;

    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    for (EffectQuickScene *view : m_clearButtons) {
        view->forwardMouseEvent(me);
        if (e->isAccepted()) {
            return;
        }
    }

    effects->defineCursor(Qt::ClosedHandCursor);

    if (e->type() == QEvent::MouseButtonPress && me->buttons() == Qt::LeftButton) {
        EffectWindow* w =  windowAt(me->pos());
        bool systemUI = isSystemUI(w);
        if (w == nullptr || systemUI) {
            _activeWindow = nullptr;
        } else if (w != nullptr && !systemUI && (w->isMovable() || w->isMovableAcrossScreens())) {
            _activeWindow = w;
        }
        _isMoveing = false;
        _isDragging = false;
        _hasTouchDown = true;

        _lastTime = me->timestamp();
        _lastPressPos = me->pos();
        _startPressPos = me->pos();
        _lastSpead = 0;
    } else if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton) {
         _hasTouchDown = false;

         QPointF moveSize = QPointF(me->pos() - _startPressPos);
         if (_isDragging) {
             if (moveSize.y() < -100) {
                 _taskList.remove(_activeWindow);
             } else {
                 effects->setElevatedWindow(_activeWindow, false);
                 _taskList.windowToOriginal(_activeWindow, true);
             }
             _isDragging = false;
         } else if (_isMoveing) {
             slotGestueEnd(QSizeF(_lastPressPos.x() - _startPressPos.x(), _lastPressPos.y() - _startPressPos.y()), QSizeF(_lastSpead, 0));
         } else {
             EffectWindow* w =  windowAt(_lastPressPos);
             bool systemUI = isSystemUI(w);
             if ((w == nullptr || systemUI) && _activeWindow == nullptr) {
                 closeTask(nullptr);
             } else if (w != nullptr && !systemUI && (w->isMovable() || w->isMovableAcrossScreens())) {
                 if (_activeWindow == w){
                     closeTask(_activeWindow);
                 }
             }
         }

         _activeWindow = nullptr;

    } else if (e->type() == QEvent::MouseMove && me->buttons() == Qt::LeftButton) {
        if (!_hasTouchDown) {
            return;
        }

        QSizeF moveSize = QSizeF(me->pos().x() - _lastPressPos.x(), me->pos().y() - _lastPressPos.y());

        if (me->timestamp() != _lastTime) {
            _lastSpead = (qreal)(me->pos().x() - _lastPressPos.x()) / (me->timestamp() - _lastTime);
        }
        _lastTime = me->timestamp();
        _lastPressPos = me->pos();
        if (!_isMoveing && !_isDragging) {
            QPointF moveDistance = QPointF(me->pos() - _startPressPos);
            if (std::abs(moveDistance.y()) >= std::abs(moveDistance.x()) && _activeWindow != nullptr && (!_taskList.isSliding() || std::abs(moveDistance.y()) >= std::abs(moveDistance.x())*1.5)) {
                if (std::abs(moveDistance.y()) >= 50) {
                    _taskList.stopSlide();
                    _isDragging = true;
                    auto item = _taskList.getWindowItem(_activeWindow);
                    effects->setElevatedWindow(_activeWindow, true);
                    if (item != nullptr) {
                        item->_oriPos = item->_curPos;
                        item->_oriScale = item->_curScale;
                        item->_curScale *= 1.1;
                    }
                    _taskList.moveItem(QSizeF(moveDistance.x(), moveDistance.y()), _activeWindow);
                }
            } else if (std::abs(moveDistance.x()) > 50) {
                _isMoveing = true;
                onMove(QSizeF(moveDistance.x(), moveDistance.y()), 0);
            }
            return ;
        } else if (_isDragging) {
            _taskList.moveItem(moveSize, _activeWindow);
            return ;
        } else if (_isMoveing) {
            onMove(moveSize, 0);
            return ;
        }
    }
}

void TaskPanel::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (taskManager->getTaskState() != TaskManager::TS_Task) {
        return;
    }

    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (_shortcut.contains(e->key() + e->modifiers()) || _shortcut1.contains(e->key() + e->modifiers())) {
            toggle();
            return;
        }

        switch(e->key()) {
        // Wrap only on autorepeat
        case Qt::Key_Left:
            slotSwitchWindow(false);
            break;
        case Qt::Key_Right:
            slotSwitchWindow(true);
            break;
        case Qt::Key_Up:
            slotSwitchWindow(false);
            break;
        case Qt::Key_Down:
            slotSwitchWindow(true);
            break;
        case Qt::Key_Escape:
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
            closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
            return;
        default:
            break;
        }
    }
}

bool TaskPanel::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (taskManager->getTaskState() != TaskManager::TS_Task || _taskList.isResetingGrid() || _hasTouchDown) {
        return false;
    }

    _lastTouchId = id;
    _isMoveing = false;
    _isDragging = false;
    _hasTouchDown = true;
    _lastPressPos = pos.toPoint();
    _startPressPos = pos.toPoint();
    _lastTime = time;
    _lastSpead = 0;
    _activeWindow = nullptr;

    QMouseEvent* me = new QMouseEvent(QEvent::MouseButtonPress, _lastPressPos, _lastPressPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    for (EffectQuickScene *view : m_clearButtons) {
        view->forwardMouseEvent(me);
        if (me->isAccepted()) {
            return true;
        }
    }

    EffectWindow* w =  windowAt(_lastPressPos);
    bool systemUI = isSystemUI(w);
    if (w == nullptr || systemUI) {
        _activeWindow = nullptr;
    } else if (w != nullptr && !systemUI && (w->isMovable() || w->isMovableAcrossScreens())) {
        _activeWindow = w;
    }

    return true;
}

bool TaskPanel::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (taskManager->getTaskState() != TaskManager::TS_Task || !_hasTouchDown || _lastTouchId != id) {
        return false;
    }

    QSizeF moveSize = QSizeF(pos.x() - _lastPressPos.x(), pos.y() - _lastPressPos.y());

    if (time != _lastTime) {
        _lastSpead = (qreal)(pos.toPoint().x() - _lastPressPos.x()) / (time - _lastTime);
    }
    _lastTime = time;
    _lastPressPos = pos.toPoint();

    if (!_isMoveing && !_isDragging) {
        QPointF moveDistance = QPointF(pos - _startPressPos);
        if (std::abs(moveDistance.y()) >= std::abs(moveDistance.x()) && _activeWindow != nullptr && (!_taskList.isSliding() || std::abs(moveDistance.y()) >= std::abs(moveDistance.x())*1.5)) {
            if (std::abs(moveDistance.y()) >= 50) {
                _taskList.stopSlide();
                _isDragging = true;
                auto item = _taskList.getWindowItem(_activeWindow);
                effects->setElevatedWindow(_activeWindow, true);
                if (item != nullptr) {
                    item->_oriPos = item->_curPos;
                    item->_oriScale = item->_curScale;
                    item->_curScale *= 1.1;
                }
                _taskList.moveItem(QSizeF(moveDistance.x(), moveDistance.y()), _activeWindow);
            }
        } else if (std::abs(moveDistance.x()) > 50) {
            _isMoveing = true;
            onMove(QSizeF(moveDistance.x(), moveDistance.y()), 0);
        }
        return true;
    } else if (_isDragging) {
        _taskList.moveItem(moveSize, _activeWindow);
        return true;
    } else if (_isMoveing) {
        onMove(moveSize, 0);
        return true;
    }

    QMouseEvent* me = new QMouseEvent(QEvent::MouseMove, _lastPressPos, _lastPressPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    for (EffectQuickScene *view : m_clearButtons) {
        view->forwardMouseEvent(me);
        if (me->isAccepted()) {
            return true;
        }
    }

    return true;
}

bool TaskPanel::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (taskManager->getTaskState() != TaskManager::TS_Task || !_hasTouchDown || _lastTouchId != id) {
        return false;
    }

    _lastTouchId = -1;
    _hasTouchDown = false;
    QMouseEvent* me = new QMouseEvent(QEvent::MouseButtonRelease, _lastPressPos, _lastPressPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    for (EffectQuickScene *view : m_clearButtons) {
        view->forwardMouseEvent(me);
        if (me->isAccepted()) {
            return true;
        }
    }

    QPointF moveSize = QPointF(_lastPressPos - _startPressPos);
    if (_isDragging) {
        if (moveSize.y() < -100) {
            _taskList.remove(_activeWindow);
        } else {
            effects->setElevatedWindow(_activeWindow, false);
            _taskList.windowToOriginal(_activeWindow, true);
        }
        _isDragging = false;
        return true;
    } else if (_isMoveing) {
        slotGestueEnd(QSizeF(_lastPressPos.x() - _startPressPos.x(), _lastPressPos.y() - _startPressPos.y()), QSizeF(_lastSpead, 0));
        return true;
    } else {
        EffectWindow* w =  windowAt(_lastPressPos);
        bool systemUI = isSystemUI(w);
        if ((w == nullptr || systemUI) && _activeWindow == nullptr) {
            closeTask(nullptr);
        } else if (w != nullptr && !systemUI && (w->isMovable() || w->isMovableAcrossScreens())) {
            if (_activeWindow == w){
                closeTask(_activeWindow);
            }
        }

        return true;
    }

    return true;
}

bool TaskPanel::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void TaskPanel::onMove(const QSizeF &delta, qreal progress)
{
    EffectWindow *w = effects->activeWindow();
    if (!_taskList.hasSetup() || isCloseState() || (w && w->isLogoutWindow())) {
        return;
    }

    _moveProgress = progress;
    qreal scale = std::max((1 - _moveProgress), 0.3);

    QSizeF size = scaleSize(0, _lastScale - scale);
    _lastScale = scale;

    if (taskManager->getTaskState() == TaskManager::TS_Prepare) {
        taskManager->setTaskState(TaskManager::TS_Move);
    }

    if (isTaskPanelState()) {
        if (taskManager->getTaskState() == TaskManager::TS_Task && !_taskList.isResetingGrid() && !_isDragging) {
            _taskList.translateItemsOnDirection(delta.width(), delta.height(), false);
            if (!_motionProcessed && progress > 0.4 && std::abs(delta.height()) > std::abs(delta.width())) {
                closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
            }
        }
    } else if (_startMoveFromDesktop && progress > 0.4) {
        _motionProcessed = true;
        toggle();
    } else if (!_startMoveFromDesktop && (!_taskList.isAnimating() || taskManager->getTaskState() == TaskManager::TS_Move)) {
        _taskList.scaleItems(QSizeF(scale, scale));
        size += QSizeF(delta.width() * 0.5, delta.height() * 0.3);
        _lastPos += QPointF(size.width(), size.height());

        if (scale < MINI_SCALE_SHOW_LIST) {
            if (!_hideList) {
                _taskList.hideList(currentWindow());
                _hideList = true;
            }
            _taskList.translateItem(_lastPos.x(), _lastPos.y(), currentWindow());
        } else if (_hideList) {
            _taskList.showList(_lastPos.x(), _lastPos.y(), currentWindow());
            _hideList = false;
        } else {
            _taskList.translateItems(_lastPos.x(), _lastPos.y(),  currentWindow(), _lastScale);
        }

        _resetTimer.start();
    }
    effects->addRepaintFull();
}


void TaskPanel::setupWindows(bool animate)
{
    EffectWindowList windows;
    if (_taskList.hasSetup()) {
        _taskList.clear();
    }
    foreach (EffectWindow * w, effects->stackingOrder()) {
        if (isManageWindowType(w) && !isSystemUI(w)) {
            windows.push_front(w);
        }
    }

    _taskList.setupWindowItems(windows, 0, currentWindow(), animate);
    if (animate) {
        _isSetupAnimate = true;
    }
    _resetTimer.start();
}

void TaskPanel::onTaskStateChanged(TaskManager::TaskState taskState, TaskManager::TaskState oldState)
{
    EffectWindow *w = effects->activeWindow();
    if ((w && w->isLogoutWindow())) {
        taskManager->setTaskState(oldState, false);
        return;
    }
    effects->setCloseWindowToDesktop(taskState == TaskManager::TS_None);
    if (taskState != TaskManager::TS_None && !_taskList.hasSetup()) {
        setupWindows(false);
        if (isSystemUI(currentWindow())) {
            effects->setShowingDesktop(false);
            _startMoveFromDesktop = true;
        }
    }

    if (taskState == TaskManager::TS_Move) {
        _lastScale = 1.;
        _lastPos = QPointF(0., 0.);
    } else if (taskState == TaskManager::TS_MoveEnd) {
        if (_startMoveFromDesktop) {
            showDesktop(true);
        } else  if (_lastScale < MINI_SCALE_SHOW_LIST && !isSystemUI(currentWindow())) {
            showDesktop(false);
        } else if (_lastScale >= MAX_SCALE_SHOW_LIST) {
            _taskList.toOriginal(true);
        } else {
            toTaskGride(currentWindow());
        }
    } else if (isTaskPanelState()) {
        _resetTimer.stop();
        if (!keyboardGrab) {
            keyboardGrab = effects->grabKeyboard(this);
            effects->startMouseInterception(this, Qt::PointingHandCursor);
            effects->setActiveFullScreenEffect(this);
        }
    } else {
        if (taskState == TaskManager::TS_None) {
            QRect area = effects->clientArea(WorkArea, 0, effects->currentDesktop());
            _lastPos = area.topLeft();
            _moveProgress = 0;
            _animateProgress = 0;
            _lastScale = 1;
            _hideList = false;
            _taskList.initCurValue();
            clearBtns();

            if (keyboardGrab)
                effects->ungrabKeyboard();
            keyboardGrab = false;

            effects->stopMouseInterception(this);
            effects->setActiveFullScreenEffect(nullptr);
            _startMoveFromDesktop = false;
        }
    }
    effects->addRepaintFull();
}

void TaskPanel::animatingChanged(bool isAnimating, qreal animateProgress)
{
    if (_isSetupAnimate) {
        _isSetupAnimate = false;
        return;
    }

    if (!isAnimating) {
        switch (taskManager->getTaskState()) {
        case TaskManager::TS_Move:
            // 双侧贴合动画
            break;
        case TaskManager::TS_MoveEnd:
            // 回弹动画
            taskManager->setTaskState(TaskManager::TS_None);
            break;
        case TaskManager::TS_Swip:
            // 切换动画
            _motionProcessed = false;
            taskManager->setTaskState(TaskManager::TS_None);
            break;
        case TaskManager::TS_ToDesktop:
            // 显示桌面动画
            resetData();
            effects->setShowingDesktop(true);
            break;
        case TaskManager::TS_TaskAnimating:
            // 切换为任务管理器 或 任务管理器换页
            taskManager->setTaskState(TaskManager::TS_Task);
            break;
        case TaskManager::TS_ToWindow:
            _delayReset = true;
            break;
        }
    } else {
        if (taskManager->getTaskState() == TaskManager::TS_ToDesktop) {
            if (animateProgress >= 0.7) {
                effects->setShowingDesktop(true);
            }
        }
    }

    _animateProgress = animateProgress;
}

void TaskPanel::resetData()
{
    _taskList.clear();
    _resetTimer.stop();
    _curBrightness = 1;
    taskManager->setTaskState(TaskManager::TS_None);
}

bool TaskPanel::isRelevantWithPresentWindows(EffectWindow *w) const
{
    if (w->isSpecialWindow() || w->isUtility()) {
        return false;
    }

    if (w->isSkipSwitcher()) {
        return false;
    }

    if (w->isDeleted()) {
        return false;
    }

    if (!w->acceptsFocus()) {
        return false;
    }

    if (!w->isOnCurrentActivity()) {
        return false;
    }

    return true;
}

void TaskPanel::slotShowingDesktopChanged(bool show)
{
    if (show && _taskList.hasSetup() && taskManager->getTaskState() != TaskManager::TS_ToDesktop) {
        resetData();
    }
}

void TaskPanel::slotWindowAdded(EffectWindow *w)
{
    if (_taskList.hasSetup() && !_taskList.isManageWindow(w) && isManageWindowType(w)) {
        if (taskManager->getTaskState() != TaskManager::TS_Task) {
            resetData();
        } else {
            if (isManageWindowType(w) && !isSystemUI(w)) {
                taskManager->setTaskState(TaskManager::TS_TaskAnimating);
                _taskList.addWindowToGrid(w);
            }
        }
    }
}

void TaskPanel::slotWindowDeleted(EffectWindow *w)
{
   if (_taskList.hasSetup() && _taskList.isManageWindow(w)) {
       if (taskManager->getTaskState() != TaskManager::TS_Task) {
           resetData();
       } else {
           _taskList.removeWindowFromGrid(w);
       }
    }

   if (isTaskPanelState() && _taskList.itemsCount() == 0) {
       closeTask(nullptr);
   }
}

void TaskPanel::showDesktop(bool direct)
{
    _taskList.hideItem(direct ? nullptr : currentWindow());
    taskManager->setTaskState(TaskManager::TS_ToDesktop);
}

bool TaskPanel::isShowingDesktop()
{
    EffectWindowList windows = effects->stackingOrder();
    for (auto it = windows.rbegin(); it != windows.rend(); it++) {
        if ((*it)->isNormalWindow() && !(*it)->windowClass().contains("org.kde.plasmashell") && (*it)->isPaintingEnabled()) {
            return false;
        }
        if (isSystemUI(*it)) {
            return true;
        }
    }

    return true;
}

EffectWindow *TaskPanel::windowAt(QPoint pos)
{
    QHash<EffectWindow*, WindowItem*> items = _taskList.getItems();
    for (auto it = items.begin(); it != items.end(); it++) {
        auto item = it.value();
        QRectF rect(item->_curPos, QSize(item->w->size().width() * item->_curScale.width(), item->w->size().height() * item->_curScale.height()));
        if (rect.contains(pos)) {
            return it.key();
        }
    }

    return nullptr;
}

EffectWindow *TaskPanel::currentWindow()
{
    EffectWindow *w = effects->activeWindow();
    if (nullptr == w) {
        return nullptr;
    }
    if (isManageWindowType(w) || w->isDesktop()) {
        return w;
    }

    EffectWindowList windows = effects->stackingOrder();
    for (auto it = windows.rbegin(); it != windows.rend(); it++) {
        if (*it && (isManageWindowType(*it) || (*it)->isDesktop())) {
            return (*it);
        }
    }

    return nullptr;
}

bool TaskPanel::isManageWindowType(EffectWindow *w)
{
    return (w->screen() == 0) &&isRelevantWithPresentWindows(w) && (w->isNormalWindow() || w->isDialog()) && !w->isTransient() &&
            !w->windowClass().contains("org.kde.plasmashell") && !w->windowClass().contains("org.kde.kioclient") && !w->windowClass().contains("rg.kde.polkit-kde-authentication-agent")
            && !w->windowClass().contains("userguide");
}

bool TaskPanel::isSystemUI(EffectWindow *w)
{
    if (w == nullptr) {
        return false;
    }
    if (w->isDesktop()) {
        return true;
    }
    EffectWindow *desktop = effects->getDesktopWindow(effects->currentDesktop());
    if (desktop == nullptr) {
        return false;
    }

    return (w->pid() == desktop->pid() || w->windowClass().compare(desktop->windowClass()) == 0);
}

void TaskPanel::addClearBtn()
{
    auto it = m_clearButtons.begin();
    const int n =  effects->numScreens();
    for (int i = 0; i < n; ++i) {
        EffectQuickScene *view;
        if (it == m_clearButtons.end()) {
            view = new EffectQuickScene(this);

            connect(view, &EffectQuickView::repaintNeeded, this, []() {
                effects->addRepaintFull();
            });

            view->rootContext()->setContextProperty("taskpanel", this);
            view->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/taskpanel/clear.qml"))));

            QQuickItem *rootItem = view->rootItem();
            if (!rootItem) {
                delete view;
                continue;
            }

            m_clearButtons.append(view);
            it = m_clearButtons.end(); // changed through insert!
        } else {
            view = *it;
            ++it;
        }
        QSize size(120, 120);
        const QRect screenRect = effects->clientArea(FullScreenArea, i, 1);
        view->show(); // pseudo show must happen before geometry changes
        const QPoint position(screenRect.right() - screenRect.width()/2 - size.width()/2,
                              screenRect.bottom() - size.height() - 36);
        view->setGeometry(QRect(position, size));
    }
    while (it != m_clearButtons.end()) {
        (*it)->deleteLater();
        it = m_clearButtons.erase(it);
    }
}

bool TaskPanel::isOnClearButton(const QPoint &pos)
{
    foreach(EffectQuickScene* view, m_clearButtons) {
        QPointF potg = view->rootItem()->mapFromGlobal(pos);
        if (view->rootItem()->contains(potg)) {
            return true;
        }
    }

    return false;
}

void TaskPanel::clearWindows()
{
    auto items = _taskList.getItems();
    for (auto it = items.begin(); it != items.end(); it++) {
        it.key()->kill();
    }

    closeTask(nullptr);
}

void TaskPanel::clearBtns()
{
    auto itor =  m_clearButtons.begin();
    while (itor != m_clearButtons.end()) {
        (*itor)->deleteLater();
        itor = m_clearButtons.erase(itor);
    }
}

}
