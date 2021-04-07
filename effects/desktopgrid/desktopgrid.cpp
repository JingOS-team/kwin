/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>
    SPDX-FileCopyrightText: 2009 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "desktopgrid.h"
// KConfigSkeleton
#include "desktopgridconfig.h"

#include "screenedge.h"
#include "../presentwindows/presentwindows_proxy.h"
#include "../effect_builtins.h"

#include <QAction>
#include <QApplication>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <netwm_def.h>
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QVector2D>
#include <QMatrix4x4>
#include <epoxy/gl.h>

#include <QQuickItem>
#include <QQmlContext>
#include <KWaylandServer/surface_interface.h>

#include <cmath>

namespace KWin
{

int index = 0;
enum
{
    Left_Bottom_X,
    Left_Bottom_Y,
    Right_Bottom_X,
    Right_Bottom_Y,
    Right_Top_X,
    Right_Top_Y,
    Left_Top_X,
    Left_Top_Y,
    Pos_Max
};

// WARNING, TODO: This effect relies on the desktop layout being EWMH-compliant.

const QString CLEAR_BTN_BG = "/usr/share/kwin_icons/task/jt_clear_%1.png";
const QString CLOSE_BTN_BG = "/usr/share/kwin_icons/task/jt_close_%1.png";
DesktopGridEffect::DesktopGridEffect()
    : activated(false)
    , timeline()
    , keyboardGrab(false)
    , wasWindowMove(false)
    , wasWindowCopy(false)
    , wasDesktopMove(false)
    , isValidMove(false)
    , windowMove(nullptr)
    , windowMoveDiff()
    , windowMoveElevateTimer(new QTimer(this))
    , lastPresentTime(std::chrono::milliseconds::zero())
    , gridSize()
    , orientation(Qt::Horizontal)
    , activeCell(1, 1)
    , scale()
    , unscaledBorder()
    , scaledSize()
    , scaledOffset()
    , m_proxy(nullptr)
    , m_activateAction(new QAction(this))
{
    initConfig<DesktopGridConfig>();
    // Load shortcuts
//    QAction* a = m_activateAction;
//    a->setObjectName(QStringLiteral("ShowDesktopGrid"));
//    a->setText(i18n("Show Desktop Grid"));
//    KGlobalAccel::self()->setDefaultShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
//    KGlobalAccel::self()->setShortcut(a, QList<QKeySequence>() << Qt::CTRL + Qt::Key_F8);
//    shortcut = KGlobalAccel::self()->shortcut(a);
//    effects->registerGlobalShortcut(Qt::CTRL + Qt::Key_F8, a);
//    effects->registerTouchpadSwipeShortcut(SwipeDirection::Up, a);
//    connect(a, &QAction::triggered, this, &DesktopGridEffect::toggle);

    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, &DesktopGridEffect::globalShortcutChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &DesktopGridEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &DesktopGridEffect::slotWindowClosed);
    connect(effects, &EffectsHandler::windowDeleted, this, &DesktopGridEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::numberDesktopsChanged, this, &DesktopGridEffect::slotNumberDesktopsChanged);
    connect(effects, &EffectsHandler::windowFrameGeometryChanged, this, &DesktopGridEffect::slotWindowFrameGeometryChanged);
    connect(effects, &EffectsHandler::numberScreensChanged, this, &DesktopGridEffect::setup);

    connect(effects, &EffectsHandler::screenAboutToLock, this, [this]() {
        setActive(false);
        windowMoveElevateTimer->stop();
        if (keyboardGrab) {
            effects->ungrabKeyboard();
            keyboardGrab = false;
        }
    });

    windowMoveElevateTimer->setInterval(QApplication::startDragTime());
    windowMoveElevateTimer->setSingleShot(true);
    connect(windowMoveElevateTimer, &QTimer::timeout, this, [this]() {
        effects->setElevatedWindow(windowMove, true);
        wasWindowMove = true;
    });

    // Load all other configuration details
    reconfigure(ReconfigureAll);
}

DesktopGridEffect::~DesktopGridEffect()
{
}

void DesktopGridEffect::reconfigure(ReconfigureFlags)
{
    DesktopGridConfig::self()->read();

    foreach (ElectricBorder border, borderActivate) {
        effects->unreserveElectricBorder(border, this);
    }
    borderActivate.clear();
    foreach (int i, DesktopGridConfig::borderActivate()) {
        borderActivate.append(ElectricBorder(i));
        effects->reserveElectricBorder(ElectricBorder(i), this);
    }

    // TODO: rename zoomDuration to duration
    zoomDuration = animationTime(DesktopGridConfig::zoomDuration() != 0 ? DesktopGridConfig::zoomDuration() : 300);
    timeline.setEasingCurve(QEasingCurve::InOutSine);
    timeline.setDuration(zoomDuration);

    border = 0;//DesktopGridConfig::borderWidth();
    desktopNameAlignment = Qt::Alignment(DesktopGridConfig::desktopNameAlignment());
    layoutMode = DesktopGridConfig::layoutMode();
    customLayoutRows = DesktopGridConfig::customLayoutRows();
    clickBehavior = DesktopGridConfig::clickBehavior();

    // deactivate and activate all touch border
    const QVector<ElectricBorder> relevantBorders{ElectricLeft, ElectricTop, ElectricRight, ElectricBottom};
    for (auto e : relevantBorders) {
        effects->unregisterTouchBorder(e, m_activateAction);
    }
    const auto touchBorders = DesktopGridConfig::touchBorderActivate();
    for (int i : touchBorders) {
        if (!relevantBorders.contains(ElectricBorder(i))) {
            continue;
        }
        effects->registerTouchBorder(ElectricBorder(i), m_activateAction);
    }
}

//-----------------------------------------------------------------------------
// Screen painting

void DesktopGridEffect::prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime)
{
    index++;
    // The animation code assumes that the time diff cannot be 0, let's work around it.
    int time;
    if (lastPresentTime.count()) {
        time = std::max(1, int((presentTime - lastPresentTime).count()));
    } else {
        time = 1;
    }
    lastPresentTime = presentTime;

    if (timeline.currentValue() != 0 || activated || (isUsingPresentWindows() && isMotionManagerMovingWindows())) {
        if (activated)
            timeline.setCurrentTime(timeline.currentTime() + time);
        else
            timeline.setCurrentTime(timeline.currentTime() - time);
        for (int i = 0; i < effects->numberOfDesktops(); i++) {
            if (i == highlightedDesktop - 1)
                hoverTimeline[i]->setCurrentTime(hoverTimeline[i]->currentTime() + time);
            else
                hoverTimeline[i]->setCurrentTime(hoverTimeline[i]->currentTime() - time);
        }
        if (isUsingPresentWindows()) {
            QList<WindowAnimationManager>::iterator i;
            for (i = m_animateManagers.begin(); i != m_animateManagers.end(); ++i)
                (*i).updateTime(presentTime);
        }
        // PAINT_SCREEN_BACKGROUND_FIRST is needed because screen will be actually painted more than once,
        // so with normal screen painting second screen paint would erase parts of the first paint
        if (timeline.currentValue() != 0 || (isUsingPresentWindows() && isMotionManagerMovingWindows()))
            data.mask |= PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_BACKGROUND_FIRST;
        if (!activated && timeline.currentValue() == 0 && !(isUsingPresentWindows() && isMotionManagerMovingWindows()))
            finish();
    }

    for (auto const &w : effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant(true));

        if (w != windowMove && m_windowCloseButtons.contains(w)) {
            QList<WindowAnimationManager>::iterator it;
            for (it = m_animateManagers.begin(); it != m_animateManagers.end(); ++it) {
                if ((*it).isManaging(w)) {
                    Motions* motions = (*it).motions(w);
                    const QRectF wGeo = QRectF(motions->getCurPos(), QSizeF(w->width() * motions->getCurScale().x(), w->height() * motions->getCurScale().y()));
                    const QPointF wPos = scalePos(wGeo.topLeft().toPoint(), sourceDesktop, w->screen());
                    const QSize wSize(scale[w->screen()] *(float)wGeo.width(),
                            scale[w->screen()] *(float)wGeo.height());
                    QRect rect = QRect(wPos.toPoint(), wSize);
                    int btnW = 44;
                    int btnH = 44;
                    int btnLM = 9;
                    int btnTM = 9;
                    rect = QRect(rect.right() - btnLM - btnW, rect.top() + btnTM, btnW, btnH);
                    m_windowCloseButtons[w]->setGeometry(rect);
                }
            }
        }
    }

    effects->prePaintScreen(data, presentTime);
}

void DesktopGridEffect::paintScreen(int mask, const QRegion &region, ScreenPaintData& data)
{
    if (timeline.currentValue() == 0 && !isUsingPresentWindows()) {
        effects->paintScreen(mask, region, data);
        return;
    }

    for (int desktop = 1; desktop <= effects->numberOfDesktops(); desktop++) {
        ScreenPaintData d = data;
        paintingDesktop = desktop;
        effects->paintScreen(mask, region, d);
    }

    for (EffectQuickScene *view : m_clearButtons) {
        view->rootItem()->setOpacity(timeline.currentValue());
        effects->renderEffectQuickView(view);
    }

    if (isUsingPresentWindows() && windowMove && wasWindowMove) {
        // the moving window has to be painted on top of all desktops
        QPoint diff = cursorPos() - m_windowMoveStartPoint;
        QRect geo = m_windowMoveGeometry.translated(diff);
        WindowPaintData d(windowMove, data.projectionMatrix());
        d *= QVector2D((qreal)geo.width() / (qreal)windowMove->width(), (qreal)geo.height() / (qreal)windowMove->height());
        d += QPoint(geo.left() - windowMove->x(), geo.top() - windowMove->y());
        effects->drawWindow(windowMove, PAINT_WINDOW_TRANSFORMED | PAINT_WINDOW_LANCZOS, infiniteRegion(), d);
    }

    if (desktopNameAlignment) {
        for (int screen = 0; screen < effects->numScreens(); screen++) {
            QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
            int desktop = 1;
            foreach (EffectFrame * frame, desktopNames) {
                QPointF posTL(scalePos(screenGeom.topLeft(), desktop, screen));
                QPointF posBR(scalePos(screenGeom.bottomRight(), desktop, screen));
                QRect textArea(posTL.x(), posTL.y(), posBR.x() - posTL.x(), posBR.y() - posTL.y());
                textArea.adjust(textArea.width() / 10, textArea.height() / 10,
                                -textArea.width() / 10, -textArea.height() / 10);
                int x, y;
                if (desktopNameAlignment & Qt::AlignLeft)
                    x = textArea.x();
                else if (desktopNameAlignment & Qt::AlignRight)
                    x = textArea.right();
                else
                    x = textArea.center().x();
                if (desktopNameAlignment & Qt::AlignTop)
                    y = textArea.y();
                else if (desktopNameAlignment & Qt::AlignBottom)
                    y = textArea.bottom();
                else
                    y = textArea.center().y();
                frame->setPosition(QPoint(x, y));
                frame->render(region, timeline.currentValue(), 0.7);
                ++desktop;
            }
        }
    }

    for (EffectQuickScene *view : m_windowCloseButtons) {
        //view->rootItem()->setOpacity(timeline.currentValue());
        effects->renderEffectQuickView(view);
    }
}

void DesktopGridEffect::postPaintScreen()
{
    bool resetLastPresentTime = true;

    foreach(WindowAnimationManager manager , m_animateManagers) {
        if (!manager.hasFinished()) {
            effects->addRepaintFull();
            break;
        } else {
            if (index > 1) {
                qDebug()<<"DesktopGridEffect::postPaintScreen:"<<index;
            }
            index = 0;
        }
    }
    if (activated ? timeline.currentValue() != 1 : timeline.currentValue() != 0) {
        effects->addRepaintFull(); // Repaint during zoom
        resetLastPresentTime = false;
    }
    if (isUsingPresentWindows() && isMotionManagerMovingWindows()) {
        effects->addRepaintFull();
        resetLastPresentTime = false;
    }
    if (activated) {
        for (int i = 0; i < effects->numberOfDesktops(); i++) {
            if (hoverTimeline[i]->currentValue() != 0.0 && hoverTimeline[i]->currentValue() != 1.0) {
                // Repaint during soft highlighting
                effects->addRepaintFull();
                resetLastPresentTime = false;
                break;
            }
        }
    }

    if (resetLastPresentTime) {
        lastPresentTime = std::chrono::milliseconds::zero();
    }

    for (auto &w : effects->stackingOrder()) {
        w->setData(WindowForceBlurRole, QVariant());
    }

    effects->postPaintScreen();
}

//-----------------------------------------------------------------------------
// Window painting

void DesktopGridEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, std::chrono::milliseconds presentTime)
{
    if (timeline.currentValue() != 0 || (isUsingPresentWindows() && isMotionManagerMovingWindows())) {
        if (w->isOnDesktop(paintingDesktop)) {
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            if (w->isMinimized() && isUsingPresentWindows())
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
            if (windowMove && wasWindowMove && windowMove->findModal() == w)
                w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        } else
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }
    effects->prePaintWindow(w, data, presentTime);
}

void DesktopGridEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (timeline.currentValue() != 0 || (isUsingPresentWindows() && isMotionManagerMovingWindows())) {
        if (isUsingPresentWindows() && w == windowMove && wasWindowMove &&
                ((!wasWindowCopy && sourceDesktop == paintingDesktop) ||
                 (sourceDesktop != highlightedDesktop && highlightedDesktop == paintingDesktop))) {
            return; // will be painted on top of all other windows
        }

        qreal xScale = data.xScale();
        qreal yScale = data.yScale();

        if (w->isDesktop()) {
            data.multiplyBrightness(0.2);
        } else {
            data.multiplyBrightness(1.0 - (0.3 * (1.0 - hoverTimeline[paintingDesktop - 1]->currentValue())));
        }

        for (int screen = 0; screen < effects->numScreens(); screen++) {
            QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

            QRectF transformedGeo = w->geometry();
            // Display all quads on the same screen on the same pass
            WindowQuadList screenQuads;
            bool quadsAdded = false;
            if (isUsingPresentWindows()) {
                WindowAnimationManager& manager = m_animateManagers[(paintingDesktop-1)*(effects->numScreens())+screen ];
                if (manager.isManaging(w)) {
                    foreach (const WindowQuad & quad, data.quads)
                        screenQuads.append(quad);
                    Motions* motions = manager.motions(w);
                    transformedGeo = QRectF(motions->getCurPos(), QSizeF(w->width() * motions->getCurScale().x(), w->height() * motions->getCurScale().y()));

                    windowLayouts[w].curGeometry = transformedGeo;
                    windowLayouts[w].curScale = motions->getCurScale();
                    quadsAdded = true;
                    if (timeline.currentValue() == 1.0)
                        mask |= PAINT_WINDOW_LANCZOS;
                } else if (w->screen() != screen) {
                    quadsAdded = true; // we don't want parts of overlapping windows on the other screen
                } else if (!w->isDesktop()){
                    return;
                }
                if (w->isDesktop())
                    quadsAdded = false;
            }
            if (!quadsAdded) {
                foreach (const WindowQuad & quad, data.quads) {
                    QRect quadRect(
                                w->x() + quad.left(), w->y() + quad.top(),
                                quad.right() - quad.left(), quad.bottom() - quad.top()
                                );
                    if (quadRect.intersects(screenGeom))
                        screenQuads.append(quad);
                }
            }
            if (screenQuads.isEmpty())
                continue; // Nothing is being displayed, don't bother
            WindowPaintData d = data;
            d.quads = screenQuads;

            QPointF newPos = scalePos(transformedGeo.topLeft().toPoint(), paintingDesktop, screen);
            d.setXScale(interpolate(1, xScale * scale[screen] * (float)transformedGeo.width() / (float)w->geometry().width(), 1));
            d.setYScale(interpolate(1, yScale * scale[screen] * (float)transformedGeo.height() / (float)w->geometry().height(), 1));
            d += QPoint(qRound(newPos.x() - w->x()), qRound(newPos.y() - w->y()));

            if (isUsingPresentWindows() && (w->isDock() || w->isSkipSwitcher())) {
                // fade out panels if present windows is used
                d.multiplyOpacity((1.0 - timeline.currentValue()));
            }
            if (isUsingPresentWindows() && w->isMinimized()) {
                d.multiplyOpacity(timeline.currentValue());
            }

            if (effects->compositingType() == XRenderCompositing) {
                // More exact clipping as XRender displays the entire window instead of just the quad
                QPointF screenPosF = scalePos(screenGeom.topLeft(), paintingDesktop).toPoint();const
                        QPoint screenPos(
                            qRound(screenPosF.x()),
                            qRound(screenPosF.y())
                            );
                QSize screenSize(
                            qRound(interpolate(screenGeom.width(), scaledSize[screen].width(), 1)),
                            qRound(interpolate(screenGeom.height(), scaledSize[screen].height(), 1))
                            );
                PaintClipper pc(effects->clientArea(ScreenArea, screen, 0) & QRect(screenPos, screenSize));
                effects->paintWindow(w, mask, region, d);
            } else {
                if (w->isDesktop() && timeline.currentValue() == 1.0) {
                    // desktop windows are not in a motion manager and can always be rendered with
                    // lanczos sampling except for animations
                    mask |= PAINT_WINDOW_LANCZOS;
                }
                effects->paintWindow(w, mask, effects->clientArea(ScreenArea, screen, 0), d);
            }

//            if (w->isDesktop()) {
//                for (EffectQuickScene *view : maskView) {
//                    view->rootItem()->setOpacity(timeline.currentValue());
//                    view->setGeometry(screenGeom);
//                    effects->renderEffectQuickView(view);
//                }
//            }
        }
    } else
        effects->paintWindow(w, mask, region, data);
}

void DesktopGridEffect::drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data)
{
    effects->drawWindow(w, mask, region, data);
    if (w->isDock()) {
        for (int screen = 0; screen < effects->numScreens(); screen++) {
            QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
            QRectF transformedGeo = w->geometry();
            // Display all quads on the same screen on the same pass
            if (isUsingPresentWindows()) {
                WindowAnimationManager& manager = m_animateManagers[(paintingDesktop-1)*(effects->numScreens())+screen ];
                if (manager.isManaging(w)) {
                    Motions* motions = manager.motions(w);
                    transformedGeo = QRectF(motions->getCurPos(), QSizeF(w->width() * motions->getCurScale().x(), w->height() * motions->getCurScale().y()));
                }
            }
        }
    }
}

void DesktopGridEffect::postPaintWindow(EffectWindow *w)
{
    effects->postPaintWindow(w);
}

void DesktopGridEffect::onHideTaskManager()
{
    if (activated) {
        toggle();
    }
}

void DesktopGridEffect::onBottomGestureToggled()
{
    toggle();
}

//-----------------------------------------------------------------------------
// User interaction

void DesktopGridEffect::slotWindowAdded(EffectWindow* w)
{
    if (!activated)
        return;
    if (!w->isNormalWindow() || w->windowClass().contains("org.kde.plasmashell")) {
        return;
    }
    if (isUsingPresentWindows()) {
        if (!isRelevantWithPresentWindows(w))
            return; // don't add
        foreach (const int i, desktopList(w)) {
            WindowAnimationManager& manager = m_animateManagers[ i*effects->numScreens()+w->screen()];
            manager.unmanageAll();
            getWindowLayout(false, w->screen(), manager);
        }
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::slotWindowClosed(EffectWindow* w)
{
    if (!activated && timeline.currentValue() == 0)
        return;
    if (!w->isNormalWindow() || w->windowClass().contains("org.kde.plasmashell")) {
        return;
    }
    if (w == windowMove) {
        effects->setElevatedWindow(windowMove, false);
        windowMove = nullptr;
    }
    if (isUsingPresentWindows()) {
        windowLayouts.remove(w);
        foreach (const int i, desktopList(w)) {
            WindowAnimationManager& manager = m_animateManagers[i*effects->numScreens()+w->screen()];
            manager.unmanage(w);
//            m_proxy->calculateWindowTransformations(manager.managedWindows(), w->screen(), windowLayouts);
            getWindowLayout(false, w->screen(), manager);
        }
    }
    effects->addRepaintFull();
    deleteCloseBtn(w);
}

void DesktopGridEffect::slotWindowDeleted(EffectWindow* w)
{
    if (!w->isNormalWindow() || w->windowClass().contains("org.kde.plasmashell")) {
        return;
    }
    if (w == windowMove)
        windowMove = nullptr;
    if (isUsingPresentWindows()) {
        windowLayouts.remove(w);
        for (QList<WindowAnimationManager>::iterator it = m_animateManagers.begin(),
             end = m_animateManagers.end(); it != end; ++it) {
            it->unmanage(w);
        }
    }
    deleteCloseBtn(w);
}

void DesktopGridEffect::slotWindowFrameGeometryChanged(EffectWindow* w, const QRect& old)
{
    Q_UNUSED(old)
    if (!activated)
        return;
    if (w == windowMove && wasWindowMove)
        return;
    if (isUsingPresentWindows()) {
        foreach (const int i, desktopList(w)) {
            WindowAnimationManager& manager = m_animateManagers[i*effects->numScreens()+w->screen()];
            manager.unmanage(w);
            getWindowLayout(false, w->screen(), manager);
        }
    }
}


bool DesktopGridEffect::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    if (!activated) {
        return false;
    }
    lastPos = pos.toPoint();
    sourceDesktop = posToDesktop(pos.toPoint());
    EffectWindow* w =  windowAt(pos.toPoint());
    if (isOnClearButton(pos.toPoint())) {
        isClearWindows = true;
        return true;
    } else  if (w == nullptr || w->isDesktop()) {
        showDesktop = true;
        return true;
    } else if (w != nullptr && !w->isDesktop() && (w->isMovable() || w->isMovableAcrossScreens() || isUsingPresentWindows())) {
        // Prepare it for moving
        if (isOnCloseButton(w, pos.toPoint())) {
            closeWindow = w;
            return true;
        } else {
            activeWindow = w;
            return true;
        }
    }

    return false;
}

bool DesktopGridEffect::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    if (lastPos != QPointF(0, 0)) {
        lastPos = pos.toPoint();
        return true;
    }

    return false;
}

bool DesktopGridEffect::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
    bool ret = false;
    if (activated) {
        sourceDesktop = posToDesktop(lastPos);
        EffectWindow* w =  windowAt(lastPos);
        if (isOnClearButton(lastPos) && isClearWindows) {
            clearWindows();
            m_originalMovingDesktop = posToDesktop(lastPos);
            ret = true;
        } else  if ((w == nullptr || w->isDesktop()) && showDesktop) {
            const int desk = posToDesktop(lastPos);
            if (desk > effects->numberOfDesktops())
                return false; // don't quit when missing desktop
            setCurrentDesktop(desk);
            setActive(false);
            ret = true;
        } else if (w != nullptr && !w->isDesktop() && (w->isMovable() || w->isMovableAcrossScreens() || isUsingPresentWindows())) {
            if (isOnCloseButton(w, lastPos)) {
                if (closeWindow == w) {
                    killWindow(w);
                    ret = true;
                }
            } else if (activeWindow == w){
                effects->activateWindow(w);
                setActive(false);
                ret = true;
            }
        }
    }

    isClearWindows = false;
    showDesktop = false;
    closeWindow = nullptr;
    activeWindow = nullptr;
    lastPos = QPoint(0,0);
    return false;
}

void DesktopGridEffect::windowInputMouseEvent(QEvent* e)
{
    if (!activated) {
        return;
    }

    if ((e->type() != QEvent::MouseMove
         && e->type() != QEvent::MouseButtonPress
         && e->type() != QEvent::MouseButtonRelease)
            || timeline.currentValue() != 1)  // Block user input during animations
        return;
    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    if (!(wasWindowMove || wasDesktopMove)) {
        for (EffectQuickScene *view : m_desktopButtons) {
            view->forwardMouseEvent(me);
            if (e->isAccepted()) {
                return;
            }
        }
    }

    effects->defineCursor(Qt::ClosedHandCursor);

    if (e->type() == QEvent::MouseButtonPress && me->buttons() == Qt::LeftButton) {
        sourceDesktop = posToDesktop(me->pos());
        EffectWindow* w =  windowAt(me->pos());
        if (isOnClearButton(me->pos())) {
            isClearWindows = true;
        } else  if (w == nullptr || w->isDesktop()) {
            showDesktop = true;
        } else if (w != nullptr && !w->isDesktop() && (w->isMovable() || w->isMovableAcrossScreens() || isUsingPresentWindows())) {
            // Prepare it for moving
            if (isOnCloseButton(w, me->pos())) {
                closeWindow = w;
            } else {
                activeWindow = w;
            }
        }
    } else if (e->type() == QEvent::MouseButtonRelease && me->button() == Qt::LeftButton) {
        sourceDesktop = posToDesktop(me->pos());
        EffectWindow* w =  windowAt(me->pos());
        if (isOnClearButton(me->pos()) && isClearWindows) {
            clearWindows();
            m_originalMovingDesktop = posToDesktop(me->pos());
        } else  if ((w == nullptr || w->isDesktop()) && showDesktop) {
            const int desk = posToDesktop(me->pos());
            if (desk > effects->numberOfDesktops())
                return ; // don't quit when missing desktop
            setCurrentDesktop(desk);
            setActive(false);
        } else if (w != nullptr && !w->isDesktop() && (w->isMovable() || w->isMovableAcrossScreens() || isUsingPresentWindows())) {
            if (isOnCloseButton(w, me->pos())) {
                if (closeWindow == w) {
                    killWindow(w);
                }
            } else if (activeWindow == w){
                effects->activateWindow(w);
                setActive(false);
            }
        }

        isClearWindows = false;
        showDesktop = false;
        closeWindow = nullptr;
        activeWindow = nullptr;
    }

    return;
}

void DesktopGridEffect::grabbedKeyboardEvent(QKeyEvent* e)
{
    if (timeline.currentValue() != 1)   // Block user input during animations
        return;
    if (windowMove != nullptr)
        return;
    if (e->type() == QEvent::KeyPress) {
        // check for global shortcuts
        // HACK: keyboard grab disables the global shortcuts so we have to check for global shortcut (bug 156155)
        if (shortcut.contains(e->key() + e->modifiers())) {
            toggle();
            return;
        }

        int desktop = -1;
        // switch by F<number> or just <number>
        if (e->key() >= Qt::Key_F1 && e->key() <= Qt::Key_F35)
            desktop = e->key() - Qt::Key_F1 + 1;
        else if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9)
            desktop = e->key() == Qt::Key_0 ? 10 : e->key() - Qt::Key_0;
        if (desktop != -1) {
            if (desktop <= effects->numberOfDesktops()) {
                setHighlightedDesktop(desktop);
                setCurrentDesktop(desktop);
                setActive(false);
            }
            return;
        }
        switch(e->key()) {
        // Wrap only on autorepeat
        case Qt::Key_Left:
            setHighlightedDesktop(desktopToLeft(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Right:
            setHighlightedDesktop(desktopToRight(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Up:
            setHighlightedDesktop(desktopUp(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Down:
            setHighlightedDesktop(desktopDown(highlightedDesktop, !e->isAutoRepeat()));
            break;
        case Qt::Key_Escape:
            setActive(false);
            return;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Space:
            setCurrentDesktop(highlightedDesktop);
            setActive(false);
            return;
        case Qt::Key_Plus:
            slotAddDesktop();
            break;
        case Qt::Key_Minus:
            slotRemoveDesktop();
            break;
        default:
            break;
        }
    }
}

bool DesktopGridEffect::borderActivated(ElectricBorder border)
{
    if (!borderActivate.contains(border))
        return false;
    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return true;
    toggle();
    return true;
}

//-----------------------------------------------------------------------------
// Helper functions

// Transform a point to its position on the scaled grid
QPointF DesktopGridEffect::scalePos(const QPoint& pos, int desktop, int screen) const
{
    if (screen == -1)
        screen = effects->screenNumber(pos);
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);
    QPoint desktopCell;
    if (orientation == Qt::Horizontal) {
        desktopCell.setX((desktop - 1) % gridSize.width() + 1);
        desktopCell.setY((desktop - 1) / gridSize.width() + 1);
    } else {
        desktopCell.setX((desktop - 1) / gridSize.height() + 1);
        desktopCell.setY((desktop - 1) % gridSize.height() + 1);
    }

    double progress = 1.0;//timeline.currentValue();
    QPointF point(
                interpolate(
                    (
                        (screenGeom.width() + unscaledBorder[screen]) *(desktopCell.x() - 1)
                        - (screenGeom.width() + unscaledBorder[screen]) *(activeCell.x() - 1)
                        ) + pos.x(),
                    (
                        (scaledSize[screen].width() + border) *(desktopCell.x() - 1)
                        + scaledOffset[screen].x()
                        + (pos.x() - screenGeom.x()) * scale[screen]
                        ),
                    progress),
                interpolate(
                    (
                        (screenGeom.height() + unscaledBorder[screen]) *(desktopCell.y() - 1)
                        - (screenGeom.height() + unscaledBorder[screen]) *(activeCell.y() - 1)
                        ) + pos.y(),
                    (
                        (scaledSize[screen].height() + border) *(desktopCell.y() - 1)
                        + scaledOffset[screen].y()
                        + (pos.y() - screenGeom.y()) * scale[screen]
                        ),
                    progress)
                );

    return point;
}

// Detransform a point to its position on the full grid
// TODO: Doesn't correctly interpolate (Final position is correct though), don't forget to copy to posToDesktop()
QPoint DesktopGridEffect::unscalePos(const QPoint& pos, int* desktop) const
{
    int screen = effects->screenNumber(pos);
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

    //double progress = timeline.currentValue();
    double scaledX = /*interpolate(
                        ( pos.x() - screenGeom.x() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.width() + unscaledBorder[screen] ) + activeCell.x() - 1,*/
            (pos.x() - scaledOffset[screen].x() + double(border) / 2.0) / (scaledSize[screen].width() + border)/*,
                        progress )*/;
    double scaledY = /*interpolate(
                        ( pos.y() - screenGeom.y() + unscaledBorder[screen] / 2.0 ) / ( screenGeom.height() + unscaledBorder[screen] ) + activeCell.y() - 1,*/
            (pos.y() - scaledOffset[screen].y() + double(border) / 2.0) / (scaledSize[screen].height() + border)/*,
                        progress )*/;
    int gx = qBound(0, int(scaledX), gridSize.width() - 1);     // Zero-based
    int gy = qBound(0, int(scaledY), gridSize.height() - 1);
    scaledX -= gx;
    scaledY -= gy;
    if (desktop != nullptr) {
        if (orientation == Qt::Horizontal)
            *desktop = gy * gridSize.width() + gx + 1;
        else
            *desktop = gx * gridSize.height() + gy + 1;
    }

    return QPoint(
                qBound(
                    screenGeom.x(),
                    qRound(
                        scaledX * (screenGeom.width() + unscaledBorder[screen])
                        - unscaledBorder[screen] / 2.0
                        + screenGeom.x()
                        ),
                    screenGeom.right()
                    ),
                qBound(
                    screenGeom.y(),
                    qRound(
                        scaledY * (screenGeom.height() + unscaledBorder[screen])
                        - unscaledBorder[screen] / 2.0
                        + screenGeom.y()
                        ),
                    screenGeom.bottom()
                    )
                );
}

int DesktopGridEffect::posToDesktop(const QPoint& pos) const
{
    // Copied from unscalePos()
    int screen = effects->screenNumber(pos);

    double scaledX = (pos.x() - scaledOffset[screen].x() + double(border) / 2.0) / (scaledSize[screen].width() + border);
    double scaledY = (pos.y() - scaledOffset[screen].y() + double(border) / 2.0) / (scaledSize[screen].height() + border);
    int gx = qBound(0, int(scaledX), gridSize.width() - 1);     // Zero-based
    int gy = qBound(0, int(scaledY), gridSize.height() - 1);
    if (orientation == Qt::Horizontal)
        return gy * gridSize.width() + gx + 1;
    return gx * gridSize.height() + gy + 1;
}

EffectWindow* DesktopGridEffect::windowAt(QPoint pos) const
{
    // Get stacking order top first
    EffectWindowList windows = effects->stackingOrder();
    EffectWindowList::Iterator begin = windows.begin();
    EffectWindowList::Iterator end = windows.end();
    --end;
    while (begin < end)
        qSwap(*begin++, *end--);

    int desktop;
    pos = unscalePos(pos, &desktop);
    if (desktop > effects->numberOfDesktops())
        return nullptr;
    for (auto it = windowLayouts.begin(); it != windowLayouts.end(); it++) {
        if (it.value().curGeometry.contains(pos)) {
            return it.key();
        }
    }
    if (isUsingPresentWindows()) {
        foreach (EffectWindow * w, windows) {
            if (w->isOnDesktop(desktop) && w->isDesktop() && w->geometry().contains(pos))
                return w;
        }
    } else {
        foreach (EffectWindow * w, windows) {
            if (w->isOnDesktop(desktop) && w->isOnCurrentActivity() && !w->isMinimized() && w->geometry().contains(pos))
                return w;
        }
    }
    return nullptr;
}

void DesktopGridEffect::setCurrentDesktop(int desktop)
{
    if (orientation == Qt::Horizontal) {
        activeCell.setX((desktop - 1) % gridSize.width() + 1);
        activeCell.setY((desktop - 1) / gridSize.width() + 1);
    } else {
        activeCell.setX((desktop - 1) / gridSize.height() + 1);
        activeCell.setY((desktop - 1) % gridSize.height() + 1);
    }
    if (effects->currentDesktop() != desktop)
        effects->setCurrentDesktop(desktop);
}

void DesktopGridEffect::setHighlightedDesktop(int d)
{
    if (d == highlightedDesktop || d <= 0 || d > effects->numberOfDesktops())
        return;
    if (highlightedDesktop > 0 && highlightedDesktop <= hoverTimeline.count())
        hoverTimeline[highlightedDesktop-1]->setCurrentTime(qMin(hoverTimeline[highlightedDesktop-1]->currentTime(),
                                                            hoverTimeline[highlightedDesktop-1]->duration()));
    highlightedDesktop = d;
    if (highlightedDesktop <= hoverTimeline.count())
        hoverTimeline[highlightedDesktop-1]->setCurrentTime(qMax(hoverTimeline[highlightedDesktop-1]->currentTime(), 0));
    effects->addRepaintFull();
}

int DesktopGridEffect::desktopToRight(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopToRight()
    int dt = desktop - 1;
    if (orientation == Qt::Vertical) {
        dt += gridSize.height();
        if (dt >= effects->numberOfDesktops()) {
            if (wrap)
                dt -= effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.width()) + 1;
        if (d >= gridSize.width()) {
            if (wrap)
                d -= gridSize.width();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.width()) + d;
    }
    return dt + 1;
}

int DesktopGridEffect::desktopToLeft(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopToLeft()
    int dt = desktop - 1;
    if (orientation == Qt::Vertical) {
        dt -= gridSize.height();
        if (dt < 0) {
            if (wrap)
                dt += effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.width()) - 1;
        if (d < 0) {
            if (wrap)
                d += gridSize.width();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.width()) + d;
    }
    return dt + 1;
}

int DesktopGridEffect::desktopUp(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopUp()
    int dt = desktop - 1;
    if (orientation == Qt::Horizontal) {
        dt -= gridSize.width();
        if (dt < 0) {
            if (wrap)
                dt += effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.height()) - 1;
        if (d < 0) {
            if (wrap)
                d += gridSize.height();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.height()) + d;
    }
    return dt + 1;
}

int DesktopGridEffect::desktopDown(int desktop, bool wrap) const
{
    // Copied from Workspace::desktopDown()
    int dt = desktop - 1;
    if (orientation == Qt::Horizontal) {
        dt += gridSize.width();
        if (dt >= effects->numberOfDesktops()) {
            if (wrap)
                dt -= effects->numberOfDesktops();
            else
                return desktop;
        }
    } else {
        int d = (dt % gridSize.height()) + 1;
        if (d >= gridSize.height()) {
            if (wrap)
                d -= gridSize.height();
            else
                return desktop;
        }
        dt = dt - (dt % gridSize.height()) + d;
    }
    return dt + 1;
}

//-----------------------------------------------------------------------------
// Activation

void DesktopGridEffect::toggle()
{
    setActive(!activated);
}

void DesktopGridEffect::setActive(bool active)
{
    effects->setShowingTaskMgr(active);
    if (!active) {
        clearBtns();
        windowLayouts.clear();
    }

    if (effects->activeFullScreenEffect() && effects->activeFullScreenEffect() != this)
        return; // Only one fullscreen effect at a time thanks
    if (active && isMotionManagerMovingWindows())
        return; // Still moving windows from last usage - don't activate
    if (activated == active)
        return; // Already in that state

    activated = active;
    if (activated) {
        effects->setShowingDesktop(false);
        if (timeline.currentValue() == 0)
            setup();
    } else {
        if (isUsingPresentWindows()) {
//            QList<WindowMotionManager>::iterator it;
//            for (it = m_managers.begin(); it != m_managers.end(); ++it) {
//                foreach (EffectWindow * w, (*it).managedWindows()) {
//                    if (effects->activeWindow() == w) {
//                        (*it).moveWindow(w, w->geometry());
//                    }
//                }
//            }
        }
        QTimer::singleShot(zoomDuration + 1, this,
                           [this] {
            if (activated)
                return;
            for (EffectQuickScene *view : m_desktopButtons) {
                view->hide();
            }
            for (EffectQuickScene *view : m_clearButtons) {
                view->hide();
            }
        }
        );
        setHighlightedDesktop(effects->currentDesktop());   // Ensure selected desktop is highlighted
    }
    effects->addRepaintFull();
}

void DesktopGridEffect::setup()
{
    index = 0;
    if (!isActive())
        return;
    if (!keyboardGrab) {
        keyboardGrab = effects->grabKeyboard(this);
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        effects->setActiveFullScreenEffect(this);
    }
    setHighlightedDesktop(effects->currentDesktop());

    // Soft highlighting
    qDeleteAll(hoverTimeline);
    hoverTimeline.clear();
    for (int i = 0; i < effects->numberOfDesktops(); i++) {
        QTimeLine *newTimeline = new QTimeLine(zoomDuration, this);
        newTimeline->setEasingCurve(QEasingCurve::InOutSine);
        hoverTimeline.append(newTimeline);
    }
    hoverTimeline[effects->currentDesktop() - 1]->setCurrentTime(hoverTimeline[effects->currentDesktop() - 1]->duration());

    // Create desktop name textures if enabled
    if (desktopNameAlignment) {
        QFont font;
        font.setBold(true);
        font.setPointSize(12);
        for (int i = 0; i < effects->numberOfDesktops(); i++) {
            EffectFrame* frame = effects->effectFrame(EffectFrameUnstyled, false);
            frame->setFont(font);
            frame->setText(effects->desktopName(i + 1));
            frame->setAlignment(desktopNameAlignment);
            desktopNames.append(frame);
        }
    }
    setupGrid();
    setCurrentDesktop(effects->currentDesktop());

    clearBtns();
    windowLayouts.clear();
    // setup the motion managers
    if (clickBehavior == SwitchDesktopAndActivateWindow)
        m_proxy = static_cast<PresentWindowsEffectProxy*>(effects->getProxy(BuiltInEffects::nameForEffect(BuiltInEffect::PresentWindows)));
    if (isUsingPresentWindows()) {
        m_proxy->reCreateGrids(); // revalidation on multiscreen, bug #351724
        for (int i = 1; i <= effects->numberOfDesktops(); i++) {
            for (int j = 0; j < effects->numScreens(); j++) {
                WindowAnimationManager manager;
                getWindowLayout(true, j, manager);
                m_animateManagers.append(manager);
            }
        }
    }

    auto it = m_desktopButtons.begin();
    const int n = DesktopGridConfig::showAddRemove() ? effects->numScreens() : 0;
    for (int i = 0; i < n; ++i) {
        EffectQuickScene *view;
        QSize size;
        if (it == m_desktopButtons.end()) {
            view = new EffectQuickScene(this);

            connect(view, &EffectQuickView::repaintNeeded, this, []() {
                effects->addRepaintFull();
            });

            view->rootContext()->setContextProperty("effects", effects);
            view->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/main.qml"))));

            QQuickItem *rootItem = view->rootItem();
            if (!rootItem) {
                delete view;
                continue;
            }

            m_desktopButtons.append(view);
            it = m_desktopButtons.end(); // changed through insert!

            size = QSize(rootItem->implicitWidth(), rootItem->implicitHeight());
        } else {
            view = *it;
            ++it;
            size = view->size();
        }
        const QRect screenRect = effects->clientArea(FullScreenArea, i, 1);
        view->show(); // pseudo show must happen before geometry changes
        const QPoint position(screenRect.right() - border/3 - size.width(),
                              screenRect.bottom() - border/3 - size.height());
        view->setGeometry(QRect(position, size));
    }
    while (it != m_desktopButtons.end()) {
        (*it)->deleteLater();
        it = m_desktopButtons.erase(it);
    }

    addClearBtn();
    addMask();
}

void DesktopGridEffect::setupGrid()
{
    // We need these variables for every paint so lets cache them
    int x, y;
    int numDesktops = effects->numberOfDesktops();
    switch(layoutMode) {
    default:
    case LayoutPager:
        orientation = Qt::Horizontal;
        gridSize = effects->desktopGridSize();
        // sanity check: pager may report incorrect size in case of one desktop
        if (numDesktops == 1) {
            gridSize = QSize(1, 1);
        }
        break;
    case LayoutAutomatic:
        y = sqrt(float(numDesktops)) + 0.5;
        x = float(numDesktops) / float(y) + 0.5;
        if (x * y < numDesktops)
            x++;
        orientation = Qt::Horizontal;
        gridSize.setWidth(x);
        gridSize.setHeight(y);
        break;
    case LayoutCustom:
        orientation = Qt::Horizontal;
        gridSize.setWidth(ceil(effects->numberOfDesktops() / double(customLayoutRows)));
        gridSize.setHeight(customLayoutRows);
        break;
    }
    scale.clear();
    unscaledBorder.clear();
    scaledSize.clear();
    scaledOffset.clear();
    for (int i = 0; i < effects->numScreens(); i++) {
        QRect geom = effects->clientArea(ScreenArea, i, 0);
        double sScale = 1.0f;
        //        if (gridSize.width() > gridSize.height())
        //            sScale = (geom.width() - border * (gridSize.width() + 1)) / double(geom.width() * gridSize.width());
        //        else
        //            sScale = (geom.height() - border * (gridSize.height() + 1)) / double(geom.height() * gridSize.height());
        double sBorder = border / sScale;
        QSizeF size(
                    double(geom.width()) * sScale,
                    double(geom.height()) * sScale
                    );
        QPointF offset(
                    geom.x() + (geom.width() - size.width() * gridSize.width() - border *(gridSize.width() - 1)) / 2.0,
                    geom.y() + (geom.height() - size.height() * gridSize.height() - border *(gridSize.height() - 1)) / 2.0
                    );
        scale.append(sScale);
        unscaledBorder.append(sBorder);
        scaledSize.append(size);
        scaledOffset.append(offset);
    }
}

void DesktopGridEffect::finish()
{
    if (desktopNameAlignment) {
        qDeleteAll(desktopNames);
        desktopNames.clear();
    }

    windowMoveElevateTimer->stop();

    if (keyboardGrab)
        effects->ungrabKeyboard();
    keyboardGrab = false;
    lastPresentTime = std::chrono::milliseconds::zero();
    effects->stopMouseInterception(this);
    effects->setActiveFullScreenEffect(nullptr);
    if (isUsingPresentWindows()) {
        while (!m_animateManagers.isEmpty()) {
            m_animateManagers.first().unmanageAll();
            m_animateManagers.removeFirst();
        }
        m_proxy = nullptr;
    }
}

void DesktopGridEffect::globalShortcutChanged(QAction *action, const QKeySequence& seq)
{
    if (action->objectName() != QStringLiteral("ShowDesktopGrid")) {
        return;
    }
    shortcut.clear();
    shortcut.append(seq);
}

bool DesktopGridEffect::isMotionManagerMovingWindows() const
{
    if (isUsingPresentWindows()) {
//        QList<WindowMotionManager>::const_iterator it;
//        for (it = m_managers.begin(); it != m_managers.end(); ++it) {
//            if ((*it).areWindowsMoving())
//                return true;
//        }
    }
    return false;
}

bool DesktopGridEffect::isUsingPresentWindows() const
{
    return (m_proxy != nullptr);
}

// transforms the geometry of the moved window to a geometry on the desktop
// internal method only used when a window is dropped onto a desktop
QRectF DesktopGridEffect::moveGeometryToDesktop(int desktop) const
{
    QPointF point = unscalePos(m_windowMoveGeometry.topLeft() + cursorPos() - m_windowMoveStartPoint);
    const double scaleFactor = scale[ windowMove->screen()];
    if (posToDesktop(m_windowMoveGeometry.topLeft() + cursorPos() - m_windowMoveStartPoint) != desktop) {
        // topLeft is not on the desktop - check other corners
        // if all corners are not on the desktop the window is bigger than the desktop - no matter what it will look strange
        if (posToDesktop(m_windowMoveGeometry.topRight() + cursorPos() - m_windowMoveStartPoint) == desktop) {
            point = unscalePos(m_windowMoveGeometry.topRight() + cursorPos() - m_windowMoveStartPoint) -
                    QPointF(m_windowMoveGeometry.width(), 0) / scaleFactor;
        } else if (posToDesktop(m_windowMoveGeometry.bottomLeft() + cursorPos() - m_windowMoveStartPoint) == desktop) {
            point = unscalePos(m_windowMoveGeometry.bottomLeft() + cursorPos() - m_windowMoveStartPoint) -
                    QPointF(0, m_windowMoveGeometry.height()) / scaleFactor;
        } else if (posToDesktop(m_windowMoveGeometry.bottomRight() + cursorPos() - m_windowMoveStartPoint) == desktop) {
            point = unscalePos(m_windowMoveGeometry.bottomRight() + cursorPos() - m_windowMoveStartPoint) -
                    QPointF(m_windowMoveGeometry.width(), m_windowMoveGeometry.height()) / scaleFactor;
        }
    }
    return QRectF(point, m_windowMoveGeometry.size() / scaleFactor);
}

void DesktopGridEffect::slotAddDesktop()
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() + 1);
}

void DesktopGridEffect::slotRemoveDesktop()
{
    effects->setNumberOfDesktops(effects->numberOfDesktops() - 1);
}

void DesktopGridEffect::slotNumberDesktopsChanged(uint old)
{
    if (!activated)
        return;
    const uint desktop = effects->numberOfDesktops();
    if (old < desktop)
        desktopsAdded(old);
    else
        desktopsRemoved(old);
}

void DesktopGridEffect::desktopsAdded(int old)
{
    const int desktop = effects->numberOfDesktops();
    for (int i = old; i <= effects->numberOfDesktops(); i++) {
        // add a timeline for the new desktop
        QTimeLine *newTimeline = new QTimeLine(zoomDuration, this);
        newTimeline->setEasingCurve(QEasingCurve::InOutSine);
        hoverTimeline.append(newTimeline);
    }

    // Create desktop name textures if enabled
    if (desktopNameAlignment) {
        QFont font;
        font.setBold(true);
        font.setPointSize(12);
        for (int i = old; i < desktop; i++) {
            EffectFrame* frame = effects->effectFrame(EffectFrameUnstyled, false);
            frame->setFont(font);
            frame->setText(effects->desktopName(i + 1));
            frame->setAlignment(desktopNameAlignment);
            desktopNames.append(frame);
        }
    }

    if (isUsingPresentWindows()) {
        for (int i = old+1; i <= effects->numberOfDesktops(); ++i) {
            for (int j = 0; j < effects->numScreens(); ++j) {
                WindowAnimationManager manager;
                manager.unmanageAll();
                //m_proxy->calculateWindowTransformations(managedWindows(), j, windowLayouts);
                getWindowLayout(false, j, manager);
                m_animateManagers.append(manager);
            }
        }
    }

    setupGrid();

    // and repaint
    effects->addRepaintFull();
}

void DesktopGridEffect::desktopsRemoved(int old)
{
    const int desktop = effects->numberOfDesktops();
    for (int i = desktop; i < old; i++) {
        delete hoverTimeline.takeLast();
        if (desktopNameAlignment) {
            delete desktopNames.last();
            desktopNames.removeLast();
        }
        if (isUsingPresentWindows()) {
            for (int j = 0; j < effects->numScreens(); ++j) {
                WindowAnimationManager& manager = m_animateManagers.last();
                manager.unmanageAll();
                m_animateManagers.removeLast();
            }
        }
    }
    // add removed windows to the last desktop
    if (isUsingPresentWindows()) {
        windowLayouts.clear();
        for (int j = 0; j < effects->numScreens(); ++j) {
            WindowAnimationManager& manager = m_animateManagers[(desktop-1)*(effects->numScreens())+j ];
            manager.unmanageAll();
            //m_proxy->calculateWindowTransformations(managedWindows(), j, windowLayouts);
            getWindowLayout(false, j, manager);
        }
    }

    setupGrid();

    // and repaint
    effects->addRepaintFull();
}
//TODO: kill this function? or at least keep a consistent numeration with desktops starting from 1
QVector<int> DesktopGridEffect::desktopList(const EffectWindow *w) const
{
    if (w->isOnAllDesktops()) {
        static QVector<int> allDesktops;
        if (allDesktops.count() != effects->numberOfDesktops()) {
            allDesktops.resize(effects->numberOfDesktops());
            for (int i = 0; i < effects->numberOfDesktops(); ++i)
                allDesktops[i] = i;
        }
        return allDesktops;
    }

    QVector<int> desks;
    desks.resize(w->desktops().count());
    int i = 0;
    for (const int desk : w->desktops()) {
        desks[i++] = desk-1;
    }
    return desks;
}

void DesktopGridEffect::clearBtns()
{
    auto it = m_windowCloseButtons.begin();
    while (it != m_windowCloseButtons.end()) {
        it.value()->deleteLater();
        it = m_windowCloseButtons.erase(it);
    }

    auto itor =  m_clearButtons.begin();
    while (itor != m_clearButtons.end()) {
        (*itor)->deleteLater();
        itor = m_clearButtons.erase(itor);
    }
}

void DesktopGridEffect::deleteCloseBtn(EffectWindow *w)
{
    if (m_windowCloseButtons.contains(w)) {
        m_windowCloseButtons[w]->deleteLater();
        m_windowCloseButtons.remove(w);

        if (m_windowCloseButtons.size() == 0) {
            setActive(false);
        }
    }
}

bool DesktopGridEffect::isOnCloseButton(EffectWindow *w, const QPoint &pos)
{
    if (!m_windowCloseButtons.contains(w)) {
        return false;
    }
    QPointF potg = m_windowCloseButtons[w]->rootItem()->mapFromGlobal(pos);
    return m_windowCloseButtons[w]->rootItem()->contains(potg);
}

void DesktopGridEffect::addMask()
{
    auto it = maskView.begin();
    const int n = DesktopGridConfig::showAddRemove() ? effects->numScreens() : 0;
    for (int i = 0; i < n; ++i) {
        EffectQuickScene *view;
        QSize size;
        if (it == maskView.end()) {
            view = new EffectQuickScene(this);

            connect(view, &EffectQuickView::repaintNeeded, this, []() {
                effects->addRepaintFull();
            });

            view->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/mask.qml"))));

            QQuickItem *rootItem = view->rootItem();
            if (!rootItem) {
                delete view;
                continue;
            }

            maskView.append(view);
            it = maskView.end(); // changed through insert!

            size = QSize(rootItem->implicitWidth(), rootItem->implicitHeight());
        } else {
            view = *it;
            ++it;
            size = view->size();
        }
        view->show(); // pseudo show must happen before geometry changes

        const QRect screenRect = effects->clientArea(FullScreenArea, i, 1);
        view->setGeometry(screenRect);
    }
    while (it != maskView.end()) {
        (*it)->deleteLater();
        it = maskView.erase(it);
    }
}

void DesktopGridEffect::addCloseBtn(EffectWindow* w, const QRect &rect)
{
    EffectQuickScene *view;
    QSize size;
    view = new EffectQuickScene(this);

    connect(view, &EffectQuickView::repaintNeeded, this, []() {
        effects->addRepaintFull();
    });

    view->rootContext()->setContextProperty("effects", effects);
    view->rootContext()->setContextProperty("window", w);
    view->rootContext()->setContextProperty("btn_img", CLOSE_BTN_BG.arg("normal"));
    view->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/close.qml"))));

    QQuickItem *rootItem = view->rootItem();
    if (!rootItem) {
        delete view;
        return;
    }

    m_windowCloseButtons.insert(w, view);

    view->show();
    view->setGeometry(rect);
}

void DesktopGridEffect::addClearBtn()
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

            view->rootContext()->setContextProperty("effects", this);
            view->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/desktopgrid/clear.qml"))));

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
        const QPoint position(screenRect.right() - screenRect.width()/2 - border/3 - size.width()/2,
                              screenRect.bottom() - border/3 - size.height() - 36);
        view->setGeometry(QRect(position, size));
    }
    while (it != m_clearButtons.end()) {
        (*it)->deleteLater();
        it = m_clearButtons.erase(it);
    }
}


bool DesktopGridEffect::isOnClearButton(const QPoint &pos)
{
    foreach(EffectQuickScene* view, m_clearButtons) {
        QPointF potg = view->rootItem()->mapFromGlobal(pos);
        if (view->rootItem()->contains(potg)) {
            return true;
        }
    }

    return false;
}

void DesktopGridEffect::killWindow(const EffectWindow *w)
{
    QString cmd = QString("kill -9 %1").arg(w->pid());
    system(cmd.toLocal8Bit().data());
}

void DesktopGridEffect::getWindowLayout(bool init, int screen, WindowAnimationManager &windowAnimationManager)
{
    foreach (EffectWindow * w, effects->stackingOrder()) {
        if ( w->screen() == screen &&isRelevantWithPresentWindows(w) && w->isNormalWindow() && !w->windowClass().contains("org.kde.plasmashell")) {
            if (!windowLayouts.contains(w)) {
                QRect rect = QRect(w->pos(), w->size());
                addCloseBtn(w, rect);
                PresentLayout layout;
                layout.curGeometry = w->geometry();
                layout.targetGeometry =  w->geometry();
                layout.curScale = QPointF(1., 1.);
                layout.targetScale = QPointF(1., 1.);
                windowLayouts.insert(w, layout);
            }
        }
    }
    int top = 0;
    QHash<EffectWindow*, QRectF> layouts = m_proxy->calculateWindowTransformations(windowLayouts.keys(), screen, top);
    for (auto it = layouts.begin(); it != layouts.end(); it++) {
        windowLayouts[it.key()].targetGeometry = it.value();
        QPointF targetPos;
        QPointF curPos;
        if (init) {
            targetPos = it.value().topLeft();
            curPos = targetPos + QPoint(0, effects->screenSize(screen).height() - top);
        } else {
            curPos = windowLayouts[it.key()].curGeometry.topLeft();
            targetPos = it.value().topLeft();
        }

        Motions *motions = new ParallelMotions;

        Animation2F *translateAni = new Animation2F;
        translateAni->initAnimate(init ? QEasingCurve::Linear : QEasingCurve::OutQuad, toStdMs(init ? 300 : 150));
        translateAni->initValue(curPos, targetPos);
        motions->addTranslateAnimate(translateAni);

        Animation2F *scaleAni = new Animation2F;
        scaleAni->initAnimate(init ? QEasingCurve::Linear : QEasingCurve::OutQuad, toStdMs(init ? 1 : 150));
        windowLayouts[it.key()].targetScale = QPointF(
                    float(it.value().width()) / it.key()->width(),
                    float(it.value().height()) / it.key()->height());

        scaleAni->initValue(windowLayouts[it.key()].curScale, windowLayouts[it.key()].targetScale);
        motions->addScaleAnimate(scaleAni);
        windowAnimationManager.manage(it.key(), motions);
    }
}



std::chrono::milliseconds DesktopGridEffect::toStdMs(int ms)
{
    return std::chrono::milliseconds(static_cast<int>(animationTime(ms)));
}

bool DesktopGridEffect::isActive() const
{
    return (timeline.currentValue() != 0 || activated || (isUsingPresentWindows() && isMotionManagerMovingWindows())) && !effects->isScreenLocked();
}

void DesktopGridEffect::clearWindows()
{
    auto it = m_windowCloseButtons.begin();
    while (it != m_windowCloseButtons.end()) {
        killWindow(it.key());
        it.value()->deleteLater();
        it = m_windowCloseButtons.erase(it);
    }
    setActive(false);
}

bool DesktopGridEffect::isRelevantWithPresentWindows(EffectWindow *w) const
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
} // namespace

