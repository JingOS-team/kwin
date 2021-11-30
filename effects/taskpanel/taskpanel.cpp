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
#include <QDBusInterface>
#include <QMouseEvent>
#include <QAction>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingCallWatcher>

#include <unistd.h>

#include "utils.h"

#include "kwingltexture.h"
#include "kwinglutils.h"
#include "input_event.h"

namespace KWin
{
const qreal TASK_OPACITY = 0.13;
const qreal ROTATIONSCALE = 0.3;
const qreal ROTATIONSCALE_15 = ROTATIONSCALE / 15;
const qreal TARGETSCALEMULTIPLIER = std::pow(2.0f, 1/3);

const qreal MINI_SCALE_SHOW_LIST = 0.5;
const qreal MAX_SCALE_SHOW_LIST = 0.9;

const int TEXT_WIDTH = 200;
const QSize ICON_SIZE = QSize(40, 40);
const int TITLE_BOTTOM_MARGIN = 10;
const int ICON_TITLE_MARGIN = 18;
const int FONT_SIZE = 18;
const QString CLEAR_BTN_BG = "/usr/share/kwin_icons/task/jt_clear_%1.png";
const QString CLOSE_BTN_BG = "/usr/share/kwin_icons/task/jt_close_%1.png";
TaskPanel::TaskPanel()
    :  m_activateAction(new QAction(this))
    , m_activateAction2(new QAction(this))
{
    _resetTimer.setInterval(10 * 1000);
    connect(&_resetTimer, &QTimer::timeout, this, &TaskPanel::resetData);
    connect(&_taskList, &TaskList::isAnimatingChanged, this, &TaskPanel::animatingChanged);
    connect(&_taskList, &TaskList::itemsChanged, this, &TaskPanel::itemsChanged);
    connect(taskManager, &TaskManager::move, this, &TaskPanel::onMove);
    connect(taskManager, &TaskManager::switchWidnow, this, &TaskPanel::slotSwitchWindow);
    connect(taskManager, &TaskManager::gestueEnd, this, &TaskPanel::slotGestureEnd);
    connect(taskManager, &TaskManager::taskStateChanged, this, &TaskPanel::onTaskStateChanged);

    connect(effects, &EffectsHandler::showingDesktopChanged, this, &TaskPanel::slotShowingDesktopChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &TaskPanel::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &TaskPanel::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowDeleted, this, &TaskPanel::slotWindowDeleted);
    connect(effects, &EffectsHandler::triggerTask, this, &TaskPanel::toggle);
    connect(effects, &EffectsHandler::sigCloseTask, this, [this] (bool animate) {
        animate ? closeTask(nullptr) : resetData();
    });
    connect(effects, &EffectsHandler::screenAboutToLock, this, &TaskPanel::close);

    QAction* a = m_activateAction;
    a->setObjectName(QStringLiteral("ShowTaskPanel"));
    a->setText(i18n("Show Task Panel"));
    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
    _shortcut = KGlobalAccel::self()->shortcut(a);
    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F8, a);
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

    if (!m_clearTexture) {
        QImage image("/usr/share/kwin_icons/task/jt_clear_normal.png");
        m_clearTexture.reset(new GLTexture(image));
        m_clearTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    }
    if (!m_closeTexture) {
        QImage image("/usr/share/kwin_icons/task/jt_close_normal.png");
        m_closeTexture.reset(new GLTexture(image));
        m_closeTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    }

    QDBusConnection::sessionBus().connect("org.kde.LogoutPrompt",
                                          "/LogoutPrompt",
                                          "org.kde.LogoutPrompt",
                                          "toBeShow", this, SLOT(close()));

    connect(effects, &EffectsHandler::hasPointerChanged, this,
            [this] (bool set) {
        if (!set) {
            clearHoverWindow();
        }
    }
    );

    m_tableProximityOut = new QTimer();
    m_tableProximityOut->setInterval(500);
    m_tableProximityOut->setSingleShot(true);
    connect(m_tableProximityOut, &QTimer::timeout, this, &TaskPanel::clearHoverWindow);

    addCloseBtn();

    QString displayName = effects->getDisplayName();
    QString service = QString("com.basesogouimebs_service.hotel_%1_%2").arg(getuid()).arg(displayName.replace(":", ""));
    _sogoServiceInterface = new QDBusInterface ( service, "/", "com.basesogouimebs_interface_service" );
    connect(effects, &EffectsHandler::displayNameChanged, this, [this](const QString &displayName) {
        delete _sogoServiceInterface;
        QString _displayName = displayName;
        QString service = QString("com.basesogouimebs_service.hotel_%1_%2").arg(getuid()).arg(_displayName.replace(":", ""));
        _sogoServiceInterface = new QDBusInterface ( service, "/", "com.basesogouimebs_interface_service" );
    });
}

void TaskPanel::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    _taskList.updateTime(presentTime);
    // We need to mark the screen windows as transformed. Otherwise the
    // whole screen won't be repainted, resulting in artefacts.
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    if (taskManager->getTaskState() == TaskManager::TS_Task) {
        _hoverWindow = windowAt(_lastMousePos);
        _closeView->rootContext()->setContextProperty("window", _hoverWindow);
        if (_hoverWindow) {
            auto item = _taskList.getWindowItem(_hoverWindow);
            QRectF rect(item->_curPos, _taskList.itemSize() * item->_itemScale);
            int btnW = 38;
            int btnH = 38;
            int btnLM = 9;
            int btnTM = 9;
            rect = QRectF(rect.right() - btnLM - btnW, rect.top() + btnTM, btnW, btnH);
            _closeView->setGeometry(rect.toRect());
            _closeView->show();
        } else {
            _closeView->hide();
        }
    }

    effects->prePaintScreen(data, presentTime);
}

void TaskPanel::paintScreen(int mask, const QRegion &region, ScreenPaintData &data)
{
    effects->paintScreen(mask, region, data);

    if (taskManager->getTaskState() == TaskManager::TS_Task) {
        for (EffectQuickScene *view : m_clearButtons) {
            //view->rootItem()->setOpacity(_animateProgress);
            // effects->renderEffectQuickView(view);
            effects->renderTexture(m_clearTexture.data(), infiniteRegion(), view->geometry());
        }

        if (_hoverWindow && _closeView) {
            //view->rootItem()->setOpacity(_animateProgress);
            //effects->renderEffectQuickView(_closeView);
            effects->renderTexture(m_closeTexture.data(), infiniteRegion(), _closeView->geometry());
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

void TaskPanel::slotGestureEnd(const QSizeF &delta, const QSizeF &spead)
{
    if (isCloseState()) {
        return;
    }
    if (!_taskList.hasSetup()) {
        if (_moveProgress > 0.1 && !_motionProcessed && taskManager->getTaskState() != TaskManager::TS_None ) {
            showToast();
        }
        resetData();
        return;
    }

    if (isMovingState() || taskManager->getTaskState() == TaskManager::TS_Prepare) {
        if (std::abs(delta.width()) > 200 && std::abs(delta.width()) > std::abs(delta.height()) * 1.2) {
            slotSwitchWindow(delta.width() > 0);
        } else if (spead.height() < -1.2) {
            if (!_hideList) {
                _taskList.hideList(currentWindow(), false);
                _hideList = true;
            }
            showDesktop(false);
        } else if (isMovingState()) {
            taskManager->setTaskState(TaskManager::TS_MoveEnd);
        } else {
            taskManager->setTaskState(TaskManager::TS_None);
        }
    } else if (taskManager->getTaskState() == TaskManager::TS_Task && !_taskList.isResetingGrid()) {
        if (!_isDragging && !_taskList.isAnimating()) {
            _taskList.moveStop(spead);
            effects->addRepaintFull();
        }
    } else if (taskManager->getTaskState() == TaskManager::TS_Prepare) {
        taskManager->setTaskState(TaskManager::TS_None);
    }

    _motionProcessed = false;
}

void TaskPanel::close()
{
    if (keyboardGrab) {
        effects->ungrabKeyboard();
        keyboardGrab = false;
    }
    if (isTaskPanelState()) {
        effects->stopMouseInterception(this);
        closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
    }
}

void TaskPanel::toggle()
{
    if (effects->isSetupMode() || effects->isScreenLocked()) {
        taskManager->setTaskState(TaskManager::TS_None);
        return;
    }

    if (taskManager->getTaskState() == TaskManager::TS_Task) {
        closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
    } else if (taskManager->getTaskState() == TaskManager::TS_None || isMovingState()) {
        if (!_taskList.hasSetup()) {
            if (!setupWindows(false)) {
                resetData();
                showToast();
            }
        }

        if (isSystemUI(currentWindow()) && !_startMoveFromDesktop) {
            effects->setShowingDesktop(false);
            _startMoveFromDesktop = true;
        }

        toTaskGride(currentWindow());
    }
}

void TaskPanel::clearHoverWindow()
{
    _hoverWindow = nullptr;
    _lastMousePos = QPoint(0, 0);
    effects->addRepaintFull();
}

void TaskPanel::closeTask(EffectWindow *window)
{
    if (!isTaskPanelState()) {
        return;
    }
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
    _hoverWindow = nullptr;
    if (_taskList.getItems().size() > 0) {
        _taskList.toGrideModel(window);

        _screenScale = effects->screenScale(0);
        addClearBtn();
        taskManager->setTaskState(TaskManager::TS_TaskAnimating);
    } else {
        resetData();
        showToast();
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

bool TaskPanel::isMovingState()
{
    return taskManager->getTaskState() == TaskManager::TS_Move;
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
    if (!_isActive || !w) {
        effects->paintWindow(w, mask, region, data);
        return;
    }

    qreal opacity = 1.0;
    if (taskManager->getTaskState() == TaskManager::TS_ToDesktop) {
        opacity =  1 - _animateProgress;
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

            if (isTaskPanelState()) {
                QRectF screenArea = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
                bool toShow = QRectF(pItem->_curPos, _taskList.itemSize() * pItem->_itemScale).intersects(screenArea);
                if (!toShow) {
                    return;
                }
            }

            //window bg
            if (!isCloseState() && (!isTaskPanelState() || (std::abs(pItem->_windowPosTranslate.x()) > 5 || std::abs(pItem->_windowPosTranslate.y()) > 5))) {
                GLVertexBuffer* vbo = GLVertexBuffer::streamingBuffer();
                vbo->reset();

                ShaderBinder binder(ShaderTrait::UniformColor);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                vbo->setUseColor(true);
                vbo->setColor(QColor(0xef, 0xf0, 0xf1));
                QVector<float> verts;
                QRegion paintRegin;
                if (taskManager->getTaskState() == TaskManager::TS_Task) {
                    paintRegin = QRect(pItem->_curPos.toPoint(), _taskList.itemSize().toSize() * pItem->_itemScale);
                } else {
                    QRect screenArea = effects->clientArea(ScreenArea, w->screen(), 0);
                    paintRegin = QRect(pItem->_curPos.toPoint(), QSize(screenArea.width() * pItem->_curScale.width(), screenArea.height() * pItem->_curScale.height()));
                }

                verts.reserve(paintRegin.rectCount() * 12);
                for (const QRect &r : paintRegin) {
                    verts << r.x() + r.width() << r.y();
                    verts << r.x() << r.y();
                    verts << r.x() << r.y() + r.height();
                    verts << r.x() << r.y() + r.height();
                    verts << r.x() + r.width() << r.y() + r.height();
                    verts << r.x() + r.width() << r.y();
                }
                vbo->setData(verts.size() / 2, 2, verts.data(), nullptr);
                vbo->render(GL_TRIANGLES);
                glDisable(GL_BLEND);
            }

            // window
            WindowQuadList screenQuads;
            foreach (const WindowQuad & quad, data.quads)
                screenQuads.append(quad);

            if (screenQuads.isEmpty())
                continue;

            WindowPaintData d = data;
            d.quads = screenQuads;
            d.setXScale(pItem->_curScale.width());
            d.setYScale(pItem->_curScale.height());
            if (taskManager->getTaskState() == TaskManager::TS_ToDesktop && w != _hidingWindow) {
                d.setOpacity(opacity * (1 - _animateProgress) * 0.5);
            } else {
                d.setOpacity(opacity);
            }
            QPointF windowMove = QPointF(pItem->windowPosX() - w->x(), pItem->windowPosY() - w->y());
            d += windowMove;
            effects->paintWindow(w, mask, effects->clientArea(ScreenArea, w->screen(), 0), d);

            // panel
            auto panel = effects->panel();
            if (!isTaskPanelState() && taskManager->getTaskState() != TaskManager::TS_ToDesktop && panel) {
                WindowPrePaintData panelPrePaintData;
                panelPrePaintData.mask = mask | PAINT_WINDOW_TRANSLUCENT;
                // panel->resetPaintingEnabled();
                panelPrePaintData.paint = infiniteRegion(); // no clipping, so doesn't really matter
                panelPrePaintData.clip = QRegion();
                panelPrePaintData.quads = panel->buildQuads();
                // preparation step
                effects->prePaintWindow(panel, panelPrePaintData, std::chrono::milliseconds::zero());

                WindowPaintData panelData(panel, data.screenProjectionMatrix());

                panelData.quads = panelPrePaintData.quads;
                panelData.setXScale(pItem->_curScale.width());
                panelData.setYScale(pItem->_curScale.height());
                panelData += windowMove;
                effects->paintWindow(panel, mask, effects->clientArea(ScreenArea, w->screen(), 0), panelData);
            }

            // icon title
            if (isTaskPanelState() && m_taskData.contains(w)) {
                QPoint iconPoint(pItem->_curPos.x(), pItem->_curPos.y() - ICON_SIZE.height() - TITLE_BOTTOM_MARGIN);
                auto  taskData = m_taskData.constFind(w);
                if (taskData != m_taskData.end()) {
                    taskData->iconFrame->setPosition(iconPoint);
                    if (effects->compositingType() == KWin::OpenGL2Compositing && data.shader) {
                        //const float a = 1.0 * data.opacity();
                        //data.shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
                    }
                    taskData->iconFrame->render(region, opacity, 0);

                    QPoint textPoint = iconPoint + QPoint(ICON_SIZE.width() + ICON_TITLE_MARGIN,  (taskData->iconFrame->geometry().height() - taskData->textFrame->geometry().height()) / 2);
                    taskData->textFrame->setPosition(textPoint);
                    if (effects->compositingType() == KWin::OpenGL2Compositing && data.shader) {
                        //const float a = 1.0 * data.opacity();
                        //data.shader->setUniform(GLShader::ModulationConstant, QVector4D(a, a, a, a));
                    }
                    taskData->textFrame->render(region, opacity, 0);
                }
            }

        } else {
            if (w->isDesktop() || w->isWallPaper()) {
                if (taskManager->getTaskState() == TaskManager::TS_ToDesktop) {
                    data.multiplyBrightness(_animateProgress * (1 - _curBrightness) + _curBrightness);
                } else if (isTaskPanelState() || taskManager->getTaskState() == TaskManager::TS_ToWindow) {
                    _curBrightness = TASK_OPACITY;
                    data.multiplyBrightness(_curBrightness);
                } else if (taskManager->getTaskState() != TaskManager::TS_None && !_startMoveFromDesktop) {
                    _curBrightness = std::max(std::pow(1 - _moveProgress, 3), TASK_OPACITY);
                    data.multiplyBrightness(_curBrightness);
                }
                effects->paintWindow(w, mask, region, data);
            } else if (w->jingWindowType() == JingWindowType::TYPE_VOICE_INTERACTION) {
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
    const_cast<TaskPanel*>(this)->_isActive = !effects->isSetupMode() && (taskManager->getTaskState() != TaskManager::TS_None && taskManager->getTaskState() != TaskManager::TS_Prepare)  && !effects->isScreenLocked();
    return _isActive;
}

void TaskPanel::mouseEvent(QEvent *e)
{
    if (taskManager->getTaskState() != TaskManager::TS_Task || _taskList.isResetingGrid() || _hasTouchDown) {
        return;
    }

    if (e->type() == QEvent::Wheel && !_hasPressDown) {
        QWheelEvent *we = dynamic_cast<QWheelEvent*>(e);

        QPointF numPixels = we->pixelDelta();
        QPointF numDegrees = we->angleDelta() / 8;

        if (!numPixels.isNull()) {
            _wheelDelta += numPixels;
            onMove(QSize(numPixels.x(), numPixels.y()) , 0);
            _wheelSpeed = numPixels;
            _wheelZeroTimes = 0;
        } else if (!numDegrees.isNull()) {
            QSize screenSize = effects->screenSize(0);
            QPointF numSteps = (numDegrees / 15 * screenSize.width() * 10 / 120) * -1;
            _wheelDelta += numSteps;
            _wheelSpeed = numSteps * 2/ (we->timestamp() - _lastWheelTime + 1);
            _wheelZeroTimes = 0;
            onMove(QSize(numSteps.x(), numSteps.y()) , 0);
        } else if (we->delta() != 0){
            _wheelDelta += QPoint(we->delta() * -1, 0);
            _wheelSpeed = QPoint(we->delta() * -1, 0);
            onMove(QSize(we->delta() * -1, 0) , 0);
            _wheelZeroTimes = 0;
        } else {
            if (++_wheelZeroTimes == 2) {
                slotGestureEnd(QSizeF(_wheelDelta.x(), _wheelDelta.y()), QSizeF(_wheelSpeed.x(), _wheelSpeed.y()));
                _wheelDelta = QPoint(0, 0);
                _wheelSpeed = QPoint(0, 0);
                _wheelZeroTimes = 0;
            }
        }

        _lastWheelTime = we->timestamp();
    }
    if ((e->type() != QEvent::MouseMove
         && e->type() != QEvent::MouseButtonPress
         && e->type() != QEvent::MouseButtonRelease))  // Block user input during animations
        return;

    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    for (EffectQuickScene *view : m_clearButtons) {
        view->forwardMouseEvent(me);
        if (e->isAccepted()) {
            _hasPressDown = false;
            return;
        }
    }
    if (_hoverWindow) {
        _closeView->forwardMouseEvent(me);
        if (_closeView->geometry().contains(me->pos()) && e->isAccepted()) {
            _hasPressDown = false;
            return;
        }
    }

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
        _hasPressDown = true;

        _lastTime = me->timestamp();
        _lastPressPos = me->pos();
        _startPressPos = me->pos();
        _lastSpead = 0;
    } else if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton && _hasPressDown) {
        _hasPressDown = false;

        QPointF moveSize = QPointF(me->pos() - _startPressPos);
        if (_isDragging) {
            if (moveSize.y() < -100 / _screenScale) {
                _taskList.remove(_activeWindow);
            } else {
                effects->setElevatedWindow(_activeWindow, false);
                _taskList.windowToOriginal(_activeWindow, true);
                auto item = _taskList.getWindowItem(_activeWindow);
                if(item) {
                    item->_itemScale = 1;
                }
            }
            _isDragging = false;
        } else if (_isMoveing) {
            slotGestureEnd(QSizeF(_lastPressPos.x() - _startPressPos.x(), _lastPressPos.y() - _startPressPos.y()), QSizeF(_lastSpead, 0));
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

    } else if (e->type() == QEvent::MouseMove) {
        if (me->buttons() == Qt::LeftButton) {
            if (!_hasPressDown) {
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
                if (std::abs(moveDistance.y()) >= std::abs(moveDistance.x()) && _activeWindow != nullptr && (!_taskList.isSliding() || std::abs(moveDistance.y()) >= std::abs(moveDistance.x())*1.5) && !_taskList.isAnimating()) {
                    if (std::abs(moveDistance.y()) >= 50 / _screenScale) {
                        _taskList.stopSlide();
                        _isDragging = true;
                        auto item = _taskList.getWindowItem(_activeWindow);
                        effects->setElevatedWindow(_activeWindow, true);
                        if (item != nullptr) {
                            item->_oriPos = item->_curPos;
                            item->_oriScale = item->_curScale;
                            item->_curScale *= 1 + (0.1 / _screenScale);
                            item->_itemScale = 1 + (0.1 / _screenScale);
                        }
                        _taskList.moveItem(QSizeF(moveDistance.x(), moveDistance.y()), _activeWindow);
                    }
                } else if (std::abs(moveDistance.x()) > 50 / _screenScale) {
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
        } else {
            _lastMousePos = me->pos();
            _hoverWindow = windowAt(_lastMousePos);
            _closeView->rootContext()->setContextProperty("window", _hoverWindow);
        }
    }
}

void TaskPanel::windowInputMouseEvent(QEvent *e)
{
    if (_hasTouchDown || _hasTabletDown) {
        return;
    }

    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    if (e->type() == QEvent::MouseButtonPress && me->buttons() == Qt::LeftButton) {
        if (_hasMouseDown) {
            return;
        }
        _hasMouseDown = true;
    } else if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton && _hasMouseDown) {
        _hasMouseDown = false;
    }

    mouseEvent(e);
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

bool TaskPanel::tabletToolEvent(TabletEvent *e)
{
    if (taskManager->getTaskState() != TaskManager::TS_Task || _taskList.isResetingGrid() || _hasMouseDown || _hasTouchDown) {
        return false;
    }

    const auto pos = e->globalPosF();
    qint32 time = e->timestamp();

    switch (e->type()) {
    case QEvent::TabletMove: {
        QMouseEvent event(QEvent::MouseMove, pos, e->pressure() > 0 ? Qt::LeftButton : Qt::NoButton, e->pressure() > 0 ? Qt::LeftButton : Qt::NoButton, Qt::NoModifier);

        event.setTimestamp(time);
        mouseEvent(&event);
        break;
    }
    case QEvent::TabletEnterProximity: {
        m_tableProximityOut->stop();
        break;
    }
    case QEvent::TabletLeaveProximity: {
        if (_hoverWindow) {
            m_tableProximityOut->start();
        }
        if (_hasTabletDown) {
            QMouseEvent event(QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            event.setTimestamp(time);
            mouseEvent(&event);
        }
        _hasTabletDown = false;
        break;
    }
    case QEvent::TabletPress: {
        _hasTabletDown = true;
        QMouseEvent event(QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        event.setTimestamp(time);
        mouseEvent(&event);
        break;
    }
    case QEvent::TabletRelease: {
        _hasTabletDown = false;
        QMouseEvent event(QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        event.setTimestamp(time);
        mouseEvent(&event);
        break;
    }
    default:
        break;
    }

    return true;
}

bool TaskPanel::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (!isTaskPanelState()) {
        return false;
    } else  if (_hasTouchDown || _hasPressDown) {
        return true;
    }

    _lastTouchId = id;
    _isMoveing = false;
    _isDragging = false;
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
    if (_hoverWindow) {
        _closeView->forwardMouseEvent(me);
        if (_closeView->geometry().contains(me->pos()) && me->isAccepted()) {
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

    _hasTouchDown = true;
    return true;
}

bool TaskPanel::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id);
    Q_UNUSED(time);
    if (!isTaskPanelState()) {
        return false;
    } else  if (!_hasTouchDown || _lastTouchId != id) {
        return true;
    }

    QSizeF moveSize = QSizeF(pos.x() - _lastPressPos.x(), pos.y() - _lastPressPos.y());

    if (time != _lastTime) {
        _lastSpead = (qreal)(pos.toPoint().x() - _lastPressPos.x()) / (time - _lastTime);
    }
    _lastTime = time;
    _lastPressPos = pos.toPoint();

    if (!_isMoveing && !_isDragging) {
        QPointF moveDistance = QPointF(pos - _startPressPos);
        if (std::abs(moveDistance.y()) >= std::abs(moveDistance.x()) && _activeWindow != nullptr && (!_taskList.isSliding() || std::abs(moveDistance.y()) >= std::abs(moveDistance.x())*1.5) && !_taskList.isAnimating()) {
            if (std::abs(moveDistance.y()) >= 50 / _screenScale) {
                _taskList.stopSlide();
                _isDragging = true;
                auto item = _taskList.getWindowItem(_activeWindow);
                effects->setElevatedWindow(_activeWindow, true);
                if (item != nullptr) {
                    item->_oriPos = item->_curPos;
                    item->_oriScale = item->_curScale;
                    item->_curScale *= 1 + (0.1 / _screenScale);
                    item->_itemScale = 1 + (0.1 / _screenScale);
                }
                _taskList.moveItem(QSizeF(moveDistance.x(), moveDistance.y()), _activeWindow);
            }
        } else if (std::abs(moveDistance.x()) > 50 / _screenScale) {
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

    if (_hoverWindow) {
        _closeView->forwardMouseEvent(me);
        if (_closeView->geometry().contains(me->pos()) && me->isAccepted()) {
            return true;
        }
    }

    return true;
}

bool TaskPanel::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(time);
    if (!isTaskPanelState()) {
        return false;
    } else if (_lastTouchId != id) {
        return true;
    }

    _lastTouchId = -1;
    if (!_hasTouchDown) {
        QMouseEvent* me = new QMouseEvent(QEvent::MouseButtonRelease, _lastPressPos, _lastPressPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        for (EffectQuickScene *view : m_clearButtons) {
            view->forwardMouseEvent(me);
            if (me->isAccepted()) {
                return true;
            }
        }

        if (_hoverWindow) {
            _closeView->forwardMouseEvent(me);
            if (_closeView->geometry().contains(me->pos()) && me->isAccepted()) {
                return true;
            }
        }

        return true;
    }
    _hasTouchDown = false;

    QPointF moveSize = QPointF(_lastPressPos - _startPressPos);
    if (_isDragging) {
        if (moveSize.y() < -100 / _screenScale) {
            _taskList.remove(_activeWindow);
        } else {
            effects->setElevatedWindow(_activeWindow, false);
            _taskList.windowToOriginal(_activeWindow, true);
            auto item = _taskList.getWindowItem(_activeWindow);
            if(item) {
                item->_itemScale = 1;
            }
        }
        _isDragging = false;
    } else if (_isMoveing) {
        slotGestureEnd(QSizeF(_lastPressPos.x() - _startPressPos.x(), _lastPressPos.y() - _startPressPos.y()), QSizeF(_lastSpead, 0));
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

    return true;
}

void TaskPanel::touchCancel()
{
    touchUp(_lastTouchId, 0);
}

bool TaskPanel::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

bool TaskPanel::enabledByDefault()
{
    return true;
}

void TaskPanel::onMove(const QSizeF &delta, qreal progress)
{
    if (effects->isSetupMode() || effects->isScreenLocked() || isCloseState()) {
        return;
    }

    EffectWindow *w = effects->activeWindow();
    _moveProgress = progress;
    if (!_taskList.hasSetup() || isCloseState() || (w && w->isLogoutWindow())) {
        return;
    }

    qreal scale = std::max((1 - _moveProgress * 0.5), 0.3);

    QSizeF size = scaleSize(0, _lastScale - scale);
    _lastScale = scale;

    if (taskManager->getTaskState() == TaskManager::TS_Prepare) {
        taskManager->setTaskState(TaskManager::TS_Move);
    }

    if (isTaskPanelState()) {
        if (taskManager->getTaskState() == TaskManager::TS_Task && !_taskList.isResetingGrid()) {
            if (_taskList.isSliding() || (!_taskList.isAnimating() && !_isDragging)) {
                _taskList.translateItemsOnDirection(delta.width(), delta.height(), false);
            }
            if (!_motionProcessed && progress > 0.8 && std::abs(delta.height()) > std::abs(delta.width())) {
                _motionProcessed = true;
                closeTask(_startMoveFromDesktop ? nullptr : currentWindow());
            }
        }
    } else if (_startMoveFromDesktop && progress > 0.1) {
        _motionProcessed = true;
        toggle();
    } else if (!_startMoveFromDesktop && (!_taskList.isAnimating() || isMovingState())) {
        _taskList.scaleItems(QSizeF(scale, scale));
        size += QSizeF(delta.width() * 0.25, delta.height() * 0.15);
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


bool TaskPanel::setupWindows(bool animate)
{
    EffectWindowList windows;
    if (_taskList.hasSetup()) {
        clearTaskData();
        _taskList.clear();
    }
    foreach (EffectWindow * w, effects->stackingOrder()) {
        if (isManageWindowType(w) && !isSystemUI(w)) {
            windows.push_front(w);
            setupTaskData(w);
        }
    }

    _taskList.setupWindowItems(windows, 0, currentWindow(), animate);
    if (animate) {
        _isSetupAnimate = true;
    }
    _resetTimer.start();

    return !windows.isEmpty();
}

void TaskPanel::setupTaskData(EffectWindow *w)
{
    auto taskData = m_taskData.insert(w, TaskData());
    taskData->textFrame = effects->effectFrame(EffectFrameNone, false);

    QFont font;
    font.setBold(true);
    font.setPointSize(FONT_SIZE);

    taskData->textFrame->setFont(font);
    taskData->textFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (!m_titleMetrics) {
        m_titleMetrics = new QFontMetrics(font);
    }
    taskData->iconFrame = effects->effectFrame(EffectFrameNone, false);
    taskData->iconFrame->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    taskData->iconFrame->setIcon(w->icon());
    taskData->iconFrame->setIconSize(ICON_SIZE);

    QString string = m_titleMetrics->elidedText(w->title(), Qt::ElideRight, TEXT_WIDTH);
    if (string != taskData->textFrame->text())
        taskData->textFrame->setText(string);
}

void TaskPanel::onTaskStateChanged(TaskManager::TaskState taskState, TaskManager::TaskState oldState)
{
    if ((effects->isSetupMode() || effects->isScreenLocked()) && taskState != TaskManager::TS_None) {
        taskManager->setTaskState(TaskManager::TS_None);
        return;
    }

    EffectWindow *w = effects->activeWindow();
    if ((w && w->isLogoutWindow()) && taskState != TaskManager::TS_None) {
        taskManager->setTaskState(oldState, false);
        return;
    }
    effects->setCloseWindowToDesktop(taskState == TaskManager::TS_None);
    if (taskState != TaskManager::TS_None && !isCloseState() && !_taskList.hasSetup()) {
        setupWindows(false);
        if (isSystemUI(currentWindow()) && !_taskList.isEmpty()) {
            effects->setShowingDesktop(false);
            _startMoveFromDesktop = true;
        }
    }

    if (w) {
        if (oldState == TaskManager::TS_None && taskState != TaskManager::TS_None) {
            w->beginTaskMode();
        } else if (taskState == TaskManager::TS_None) {
            w->endTaskMode();
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
            QTimer::singleShot(250, this, [this]() {
                effects->startMouseInterception(this, Qt::ArrowCursor);
            });
            effects->setActiveFullScreenEffect(this);
        }
    } else if (taskState == TaskManager::TS_None) {
        QRect area = effects->clientArea(WorkArea, 0, effects->currentDesktop());
        _lastPos = area.topLeft();
        _lastMousePos = QPoint(0, 0);
        _moveProgress = 0;
        _animateProgress = 0;
        _lastScale = 1;
        _hideList = false;
        _taskList.initCurValue();
        clearBtns();
        if (keyboardGrab)
            effects->ungrabKeyboard();
        keyboardGrab = false;

        QTimer::singleShot(250, this, [this]() {
            effects->stopMouseInterception(this);
        });
        effects->setActiveFullScreenEffect(nullptr);
        _startMoveFromDesktop = false;
    }
    effects->addRepaintFull();
}

void TaskPanel::hideInput()
{
    QDBusPendingCall pcall = _sogoServiceInterface->asyncCall("ShowKeyboard", false);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pcall, this);

    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *call) {
        QDBusPendingReply<QString, QByteArray> reply = *call;
        if (reply.isError()) {
            qDebug()<<"showInput Error:"<<reply.error().message();
        }
        call->deleteLater();
        effects->addRepaintFull();
    });
}

void TaskPanel::itemsChanged()
{
    if (isTaskPanelState() && _taskList.itemsCount() == 0) {
        closeTask(nullptr);
    }
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
    if (_activeWindow) {
        effects->setElevatedWindow(_activeWindow, false);
    }
    _taskList.clear();
    clearTaskData();
    _resetTimer.stop();
    _curBrightness = 1;
    _motionProcessed = false;
    _hasTouchDown = false;
    _hasPressDown = false;
    _activeWindow = nullptr;
    _hoverWindow = nullptr;
    taskManager->setTaskState(TaskManager::TS_None);
    effects->clearElevatedWindow();
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
        // resetData();
    }
}

void TaskPanel::slotWindowAdded(EffectWindow *w)
{
    if (_taskList.hasSetup() && !_taskList.isManageWindow(w) && isManageWindowType(w)) {
        if (taskManager->getTaskState() != TaskManager::TS_Task) {
            resetData();
        } else {
            if (isManageWindowType(w) && !isSystemUI(w)) {
                qDebug()<<"TaskPanel-1 slotWindowAdded:"<<_taskList.isAnimating();
                if (_taskList.isAnimating()) {
                    _taskList.updateTime(std::chrono::milliseconds::zero(), true);
                }
                addToList(w);
            }
        }
    }
}

void TaskPanel::slotWindowDeleted(EffectWindow *w)
{
    if (w == _activeWindow) {
        _activeWindow = nullptr;
        effects->setElevatedWindow(w, false);
    }
    if (w == _hoverWindow) {
        _hoverWindow = nullptr;
    }
    if (_taskList.hasSetup() && _taskList.isManageWindow(w)) {
        if (!isTaskPanelState() && taskManager->getTaskState() != TaskManager::TS_ToDesktop) {
            resetData();
        } else {
            qDebug()<<"TaskPanel-1 slotWindowDeleted:"<<_taskList.isAnimating();
            if (_taskList.isAnimating()) {
                _taskList.updateTime(std::chrono::milliseconds::zero(), true);
            }
            removeFromList(w);
        }
    }
}

void TaskPanel::showDesktop(bool direct)
{
    hideInput();
    _hidingWindow = direct ? nullptr : currentWindow();
    _taskList.hideItem(_hidingWindow);
    taskManager->setTaskState(TaskManager::TS_ToDesktop);
    effects->showDockBg(false, false);
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
        QRectF rect(item->_curPos, _taskList.itemSize());
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

    w = effects->foregroundTopLevel();
    if (w && (isManageWindowType(w) || w->isDesktop())) {
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
    return (w->screen() == 0) &&isRelevantWithPresentWindows(w) && (w->isNormalWindow() || (w->isDialog() && !w->hasParent())) &&
            !w->windowClass().contains("org.kde.plasmashell") && !w->windowClass().contains("org.kde.kioclient") && !w->windowClass().contains("rg.kde.polkit-kde-authentication-agent")
            && !w->windowClass().contains("userguide") && !w->title().isEmpty() && !w->icon().isNull();
}

bool TaskPanel::isSystemUI(EffectWindow *w)
{
    return effects->isSystemUI(w);
}

void TaskPanel::addToList(EffectWindow *w)
{
    taskManager->setTaskState(TaskManager::TS_TaskAnimating);
    _taskList.addWindowToGrid(w);
    setupTaskData(w);
}

void TaskPanel::removeFromList(EffectWindow *w)
{
    _taskList.removeWindowFromGrid(w);
    delete m_taskData[w].iconFrame;
    delete m_taskData[w].textFrame;
    m_taskData.remove(w);
}

void TaskPanel::addCloseBtn()
{
    QSize size;
    _closeView = new EffectQuickScene(this);

    connect(_closeView, &EffectQuickView::repaintNeeded, this, []() {
        effects->addRepaintFull();
    });

    _closeView->rootContext()->setContextProperty("taskpanel", this);
    _closeView->rootContext()->setContextProperty("btn_img", CLOSE_BTN_BG.arg("normal"));
    _closeView->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/taskpanel/close.qml"))));

    QQuickItem *rootItem = _closeView->rootItem();
    if (!rootItem) {
        delete _closeView;
        _closeView = nullptr;
        return;
    }

    _closeView->show();
}

void TaskPanel::addClearBtn()
{
    QSize screenSize = effects->screenSize(0);

    auto it = m_clearButtons.begin();
    const int n =  effects->numScreens();
    for (int i = 0; i < n; ++i) {
        EffectQuickScene *view;
        //QSize size;
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

            // size = QSize(rootItem->implicitWidth(), rootItem->implicitHeight());
            m_clearButtons.append(view);
            it = m_clearButtons.end(); // changed through insert!
        } else {
            view = *it;
            ++it;
        }

        const QRect screenRect = effects->clientArea(FullScreenArea, i, 1);
        QSize size(120 / _screenScale, 120 / _screenScale);
        view->show(); // pseudo show must happen before geometry changes
        const QPoint position(screenRect.left() + (screenRect.width() - size.width()) / 2,
                              screenRect.bottom() - size.height() - 15 / 472.0 * screenSize.height());
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
    QList<EffectWindow *> windows = _taskList.getItems().keys();
    _taskList.deleteAllData();
    closeTask(nullptr);
    effects->killWindows(windows);
}

void TaskPanel::clearTaskData()
{
    auto itor = m_taskData.begin();
    while (itor != m_taskData.end()) {
        delete (*itor).iconFrame;
        delete (*itor).textFrame;
        itor++;
    }
    m_taskData.clear();
}

void TaskPanel::showToast()
{
    QDBusInterface iface( "org.jingos.toast", "/org/jingos/toast", "org.jingos.toast", QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        qWarning()<<Q_FUNC_INFO<< qPrintable(QDBusConnection::sessionBus(). lastError().message());
        return;
    }

    iface.asyncCall("showText", i18n("There are no running tasks"));
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

