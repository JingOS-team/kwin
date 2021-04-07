/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "switchwindows.h"
#include "workspace.h"
#include <QTimer>
#include <QDebug>
#include <math.h>

namespace KWin
{
EffectWindow * OPACITY_WIN_ID = (EffectWindow *)0x00001;
SwitchWindows::SwitchWindows()
{
    _curItor = _windows.begin();
    _resetTimer = new QTimer();
    _resetTimer->setInterval(10 * 1000);
    connect(_resetTimer, &QTimer::timeout, this, &SwitchWindows::init);
    connect(effects, &EffectsHandler::showingDesktopChanged, this, &SwitchWindows::slotShowingDesktopChanged);
    connect(effects, &EffectsHandler::windowAdded, this, &SwitchWindows::slotWindowAdded);
    connect(effects, &EffectsHandler::windowClosed, this, &SwitchWindows::slotWindowDeleted);
    connect(effects, &EffectsHandler::windowDeleted, this, &SwitchWindows::slotWindowDeleted);
    connect(effects, &EffectsHandler::switchWindows, this, &SwitchWindows::slotSwitchWindow);

    auto config = KSharedConfig::openConfig(QStringLiteral("plasmashellrc"));
    _topDockHeight = config->group("PlasmaViews").group("Panel 3").group("Defaults").readEntry("thickness", 36);
}

void SwitchWindows::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    _manager.updateTime(presentTime);

    // We need to mark the screen windows as transformed. Otherwise the
    // whole screen won't be repainted, resulting in artefacts.
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;

    effects->prePaintScreen(data, presentTime);
}

void SwitchWindows::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (isActive()) {
        if (w->isMinimized() && _manager.isManaging(w))
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
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
        }          w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
    }
    effects->prePaintWindow(w, data, presentTime);
}

void SwitchWindows::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    for (int screen = 0; screen < effects->numScreens(); screen++) {
        QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

        if ((!w->isDesktop() && _manager.isManaging(w))) {
            QRectF transformedGeo = w->geometry();
            // Display all quads on the same screen on the same pass
            WindowQuadList screenQuads;
            foreach (const WindowQuad & quad, data.quads)
                screenQuads.append(quad);

            if (screenQuads.isEmpty())
                continue;

            WindowPaintData d = data;
            d.quads = screenQuads;

            if (w->isDock()) {
                if (_manager.isManaging(OPACITY_WIN_ID)) {
                    Motions *opacityMotions = _manager.motions(OPACITY_WIN_ID);
                    d.multiplyOpacity(opacityMotions->getCurOpacity());
                }
            } else {
                Motions *motions = _manager.motions(w);

                QPointF transformedPos = motions->getCurPos();
                QPointF scale = motions->getCurScale();
                QPointF newPos = scalePos(transformedPos.toPoint(), w->screen(), scale);
                d.setXScale(scale.x());
                d.setYScale(scale.y());
                d += QPoint(qRound(newPos.x() - w->x()), qRound(newPos.y() - w->y()));
            }
            effects->paintWindow(w, mask, effects->clientArea(ScreenArea, screen, 0), d);
        } else if (w->isDesktop() || w == effects->activeWindow() || w->isDock()) {
            effects->paintWindow(w, mask, region, data);
        }
    }
}

QPointF SwitchWindows::scalePos(const QPoint& pos, int screen, const QPointF &scale) const
{
    if (screen == -1)
        screen = effects->screenNumber(pos);
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

    QPointF point(pos.x() + screenGeom.width() * (1 - scale.x()) / 2,
                  pos.y() + (screenGeom.height()) * (1 -scale.y()) / 2);

    return point;
}

void SwitchWindows::postPaintWindow(EffectWindow *w)
{
    if (_manager.isFinished(w)) {
        _manager.unmanage(w);
        if (!w->isOnCurrentActivity()) {
            w->disablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);
        }
        effects->addRepaintFull();
    }
    effects->postPaintWindow(w);
}

void SwitchWindows::postPaintScreen()
{
    if (!_manager.isEmpty()) {
        effects->addRepaintFull();
    } else if (--_pending > 0) {
        witchWindow(_revert ? !(_switchState == S_FORWARD || _switchState == S_FORWARD_BOUND) :
                              (_switchState == S_FORWARD || _switchState == S_FORWARD_BOUND));
        _revert = false;
    } else if ( _switchState != S_IDLE){
        _switchState = S_IDLE;
        effects->addRepaintFull();
    }

    if (_manager.isFinished(OPACITY_WIN_ID)) {
        _manager.unmanage(OPACITY_WIN_ID);
    }
    effects->postPaintScreen();
}

bool SwitchWindows::isActive() const
{
    return !_manager.isEmpty() || (_switchState != S_NONE && _switchState != S_IDLE);
}

bool SwitchWindows::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void SwitchWindows::slotWindowDeleted(EffectWindow *w)
{
    if (_windows.contains(w)) {
        if (*_curItor == w) {
            _curItor = _windows.begin();
        }
        _windows.removeOne(w);
    }
    _manager.unmanage(w);
}

void SwitchWindows::slotSwitchWindow(bool toRight)
{
    bool isOnDesktop = false;
    if (isShowingDesktop()) {
        isOnDesktop = true;
        init();
    }

    if (_switchState != S_NONE) {
        if (_switchState == S_IDLE) {
            _pending = 1;
            witchWindow(toRight);
        } else {
            if ((toRight && (_switchState == S_FORWARD)) ||
                    (!toRight && (_switchState == S_BACKWARD))) {
                if (_pending < 10) {
                    _pending++;
                }
            } else if ((toRight && (_switchState == S_FORWARD_BOUND)) ||
                       (!toRight && (_switchState == S_BACKWARD_BOUND))) {
                _pending = 2;
            } else {
                _pending = 2;
                _revert = true;
            }
        }
    } else {
        fillList();
        _pending = 1;
        witchWindow(toRight, isOnDesktop);
    }
}

void SwitchWindows::witchWindow(bool isForward, bool isOnDesktop)
{
    if (_windows.size() <= 0) {
        effects->addRepaintFull();
        return;
    }

    auto old = _curItor;
    if (isForward) {
        if ((_windows.end() == _curItor+1 || _windows.end() == _curItor) && !isOnDesktop) {
            _switchState = S_FORWARD_BOUND;
            doBoundWindow(*_curItor, isForward);
            _pending = 1;
        } else {
            _switchState = S_FORWARD;
            if (!isOnDesktop) {
                _curItor++;
                doSwitchWindow(*old, *_curItor, isForward);
            } else {
                doSwitchWindow(nullptr, *_curItor, isForward);
            }
        }
    } else {
        if (_curItor == _windows.begin()) {
            _switchState = S_BACKWARD_BOUND;
            doBoundWindow(*_curItor, isForward);
            _pending = 1;
        } else {
            _switchState = S_BACKWARD;
            _curItor--;
            doSwitchWindow(*old, *_curItor, isForward);
        }
    }
    _resetTimer->start();

    effects->addRepaintFull();
}

void SwitchWindows::doSwitchWindow(EffectWindow *old, EffectWindow *cur, bool isForward)
{
    if (old != nullptr) {
        old->enablePainting(EffectWindow::PAINT_DISABLED_BY_DESKTOP);

        QPointF windowScale = getWindowScale(old, _scale);

        Motions *motions = new ParallelMotions;
        Animation2F *oldAni = new Animation2F;
        oldAni->initAnimate(QEasingCurve::InOutQuart, toStdMs(getAniDuration()));
        oldAni->initValue(old->pos(), getWindowSwitchPos(old, !isForward));
        motions->addTranslateAnimate(oldAni);

        BoundAnimation2F *oldScaleAni = new BoundAnimation2F;
        oldScaleAni->initAnimate(QEasingCurve::InOutQuart, toStdMs(getAniDuration()));
        oldScaleAni->initValue(QPointF(1., 1.), windowScale);
        motions->addScaleAnimate(oldScaleAni);
        _manager.manage(old, motions);
    }


    if (cur != nullptr) {
        effects->activateWindowWhithoutAnimation(cur);
        QPointF windowScale = getWindowScale(cur, _scale);
        Motions *curMotions = new ParallelMotions;
        Animation2F *curAni = new Animation2F;
        curAni->initAnimate(QEasingCurve::InOutQuart, toStdMs(getAniDuration()));
        curAni->initValue(getWindowSwitchPos(cur, isForward), cur->pos());
        curMotions->addTranslateAnimate(curAni);

        BoundAnimation2F *curScaleAni = new BoundAnimation2F;
        curScaleAni->initAnimate(QEasingCurve::InOutQuart, toStdMs(getAniDuration()));
        curScaleAni->initValue(QPointF(1., 1.), windowScale);
        curMotions->addScaleAnimate(curScaleAni);
        _manager.manage(cur, curMotions);


        Motions *opacityMotions = new ParallelMotions;
        BoundAnimation1F *opacity = new BoundAnimation1F;
        opacity->initAnimate(QEasingCurve::InOutQuart, toStdMs(getAniDuration()));
        opacity->initValue(0, 1);
        opacityMotions->addOpacityAnimate(opacity);
        _manager.manage(OPACITY_WIN_ID, opacityMotions);
    }
}

void SwitchWindows::doBoundWindow(EffectWindow *w, bool isForward)
{

    Motions *motions = new ParallelMotions;

    BoundAnimation2F *transAni = new BoundAnimation2F;
    transAni->initAnimate(QEasingCurve::OutInCubic, toStdMs(_boundDuration));
    transAni->initValue(w->pos(), getWindowBoundToPos(w, !isForward));
    motions->addTranslateAnimate(transAni);

    QPointF windowScale = getWindowScale(w, _boundScale);

    BoundAnimation2F *scaleAni = new BoundAnimation2F;
    scaleAni->initAnimate(QEasingCurve::OutInCubic, toStdMs(_boundDuration));
    scaleAni->initValue(QPointF(1., 1.), windowScale);
    motions->addScaleAnimate(scaleAni);
    _manager.manage(w, motions);
}

QPointF SwitchWindows::getWindowSwitchPos(EffectWindow *w, bool isForward)
{
    qreal srWidht = effects->screenSize(w->screen()).width();
    QPointF pos = w->pos();
    pos = isForward ? (pos - QPointF(srWidht, 0)) : (pos + QPointF(srWidht, 0));

    return pos;
}

QPointF SwitchWindows::getWindowBoundToPos(EffectWindow *w, bool isForward)
{
    qreal srWidht = effects->screenSize(w->screen()).width();
    QPointF pos = w->pos();
    pos = isForward ? (pos - QPointF(srWidht / 5, 0) ) : (pos + QPointF(srWidht / 5, 0));

    return pos;
}

std::chrono::milliseconds SwitchWindows::toStdMs(int ms)
{
    return std::chrono::milliseconds(static_cast<int>(animationTime(ms)));
}

QPointF SwitchWindows::getWindowScale(EffectWindow *w, qreal scale)
{
    int screen = w->screen();
    if (screen == -1)
        screen = effects->screenNumber(w->geometry().topLeft());
    QRect screenGeom = effects->clientArea(ScreenArea, screen, 0);

    return QPointF(screenGeom.width() * scale / w->geometry().width(), screenGeom.height() * scale / w->geometry().height());
}

int SwitchWindows::getAniDuration()
{
    return _aniDuration / std::pow(2, _pending - 1);
}

void SwitchWindows::slotShowingDesktopChanged(bool show)
{
    if (show) {
        init();
    }
}

void SwitchWindows::slotWindowAdded(EffectWindow *w)
{
    init();
}

bool SwitchWindows::isShowingDesktop()
{
    EffectWindowList windows = effects->stackingOrder();
    for (auto it = windows.rbegin(); it != windows.rend(); it++) {
        if ((*it)->isNormalWindow() && !(*it)->windowClass().contains("org.kde.plasmashell") && (*it)->isPaintingEnabled()) {
            return false;
        }
        if ((*it)->isDesktop()) {
            return true;
        }
    }

    return true;
}

void SwitchWindows::fillList()
{
    foreach (EffectWindow * w, effects->stackingOrder()) {
        if (!w->isNormalWindow() || w->windowClass().contains("org.kde.plasmashell")) {
            continue;
        }

        _windows.push_front(w);
    }
    _curItor = _windows.begin();
}

void SwitchWindows::init()
{
    _switchState = S_NONE;
    _started = false;
    _windows.clear();
}
}
