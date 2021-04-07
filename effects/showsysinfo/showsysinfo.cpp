/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "showsysinfo.h"

#include <QQuickItem>
#include <QQmlContext>
#include <QMouseEvent>

namespace KWin
{

const int BOTTOM_CONTROL_HEIGHT = 18;
const int BOTTOM_CONTROL_WIDTH = 420;

const qreal BOTTOM_CONTROL_SCALE_Y = 0.2;
const qreal BOTTOM_CONTROL_SCALE_X = 0.2;

ShowSysInfo::ShowSysInfo()
{
    connect(effects, &EffectsHandler::showingDesktopChanged, this, &ShowSysInfo::slotShowingDesktopChanged);
    _noticeView = new EffectQuickScene(this);

    connect(_noticeView, &EffectQuickView::repaintNeeded, this, []() {
        effects->addRepaintFull();
    });

    _noticeView->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/showsysinfo/notice.qml"))));

    QQuickItem *rootItem = _noticeView->rootItem();
    if (!rootItem) {
        delete _noticeView;
        return;
    }

    _noticeView->show();
    _noticeView->rootItem()->setOpacity(0.8);
    _noticeView->setGeometry(QRect(0, 0, 300, 300));

    _bottomControlview = new EffectQuickScene(this);

    connect(_bottomControlview, &EffectQuickView::repaintNeeded, this, []() {
        effects->addRepaintFull();
    });

    _bottomControlview->rootContext()->setContextProperty("showSysInfo", this);
    _bottomControlview->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/showsysinfo/main.qml"))));

    rootItem = _bottomControlview->rootItem();
    if (!rootItem) {
        delete _bottomControlview;
        _bottomControlview = nullptr;
        return;
    }

    _bottomControlview->setGeometry(QRect(0, 0, 300, 300));
    _bottomControlview->show();

    _bottomAnimationTimeLine.setEasingCurve(QEasingCurve::OutQuart);
    _bottomAnimationTimeLine.setDuration(std::chrono::milliseconds(static_cast<int>(Effect::animationTime(150))));
}

void ShowSysInfo::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
    if (_lastPresentTime.count()) {
        delta = presentTime - _lastPresentTime;
    }
    _lastPresentTime = presentTime;

    _bottomAnimationTimeLine.update(delta);

    effects->prePaintScreen(data, presentTime);
}

void ShowSysInfo::postPaintScreen()
{
    if (effects->showCloseNotice() && effects->activeWindow()) {
        QPoint sCenter = effects->clientArea(WorkArea, effects->activeWindow()).center();
        QRect rect(0, 0, 460, 50);
        rect.moveCenter(sCenter);
        _noticeView->setGeometry(rect);
        _noticeView->rootItem()->setOpacity(0.8);
        effects->renderEffectQuickView(_noticeView);
    }

    if (!effects->showingDesktop() && effects->activeWindow() && _bottomControlview && !effects->isScreenLocked()) {
        QSize screenSize =  effects->screenSize(0);

        int height = (1 - BOTTOM_CONTROL_SCALE_Y * _bottomAnimationTimeLine.value()) * BOTTOM_CONTROL_HEIGHT;
        int width = (1 - BOTTOM_CONTROL_SCALE_X * _bottomAnimationTimeLine.value()) * BOTTOM_CONTROL_WIDTH;

        _bottomRect = QRect((screenSize.width()- width) / 2, screenSize.height() - height - 5, width, height);
        _bottomControlview->setGeometry(_bottomRect);

        _bottomControlview->rootContext()->setContextProperty("barScale", _barScale);
        effects->renderEffectQuickView(_bottomControlview);
    }

    effects->postPaintScreen();

    if (!_bottomAnimationTimeLine.done()) {
        effects->addRepaintFull();
    } else {
        _lastPresentTime = std::chrono::milliseconds::zero();
    }
}

bool ShowSysInfo::isActive() const
{
    bool isActive = taskManager->getTaskState() == TaskManager::TS_None && effects->activeWindow();
    return isActive;
    //  && !effects->isScreenLocked() && (effects->showCloseNotice() || effects->activeWindow()->isNormalWindow())
}

bool ShowSysInfo::pointerEvent(QMouseEvent *e)
{
    if ((e->type() != QEvent::MouseMove
         && e->type() != QEvent::MouseButtonPress
         && e->type() != QEvent::MouseButtonRelease))  // Block user input during animations
        return false;

    QMouseEvent* me = static_cast< QMouseEvent* >(e);
    _bottomControlview->forwardMouseEvent(me);
    if (e->isAccepted()) {
        return true;
    }

    return false;
}

void ShowSysInfo::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (w->isBackApp()) {
        return;
    }

    if (isActive() && w->isScaleApp() && !w->isBackApp()) {
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

void ShowSysInfo::paintWindow(EffectWindow *w, int mask, QRegion region, WindowPaintData &data)
{
    if (w->isBackApp()) {
        return;
    }
    if (isActive() && w->isScaleApp() && !w->isBackApp()) {
        WindowQuadList screenQuads;
        foreach (const WindowQuad & quad, data.quads)
            screenQuads.append(quad);

        if (screenQuads.isEmpty())
            return;

        WindowPaintData d = data;
        d.quads = screenQuads;
        d.setXScale(w->getAppScale());
        d.setYScale(w->getAppScale());

        QRect rect = region.boundingRect();
        rect.setSize(rect.size() * w->getAppScale());
        rect.intersected(w->geometry());
        effects->paintWindow(w, mask, rect, d);
    } else {
        effects->paintWindow(w, mask, region, data);
    }
}

bool ShowSysInfo::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

void ShowSysInfo::showDesktop()
{
    effects->setShowingDesktop(true);
}

void ShowSysInfo::enterControlBar()
{
    _bottomAnimationTimeLine.setDirection(TimeLine::Backward);
    _bottomAnimationTimeLine.reset();
    effects->addRepaintFull();
}

void ShowSysInfo::leaveControlBar()
{
    _bottomAnimationTimeLine.setDirection(TimeLine::Forward);
    _bottomAnimationTimeLine.reset();
    effects->addRepaintFull();
}

void ShowSysInfo::switchWindow(bool toRight)
{
    taskManager->onTaskSwipe(toRight);
}

bool ShowSysInfo::isShowNextWindow()
{
    auto curW = effects->activeWindow();
    return !curW->opaqueRegion().isEmpty() || curW->maximizeMode() != 3;
}

void ShowSysInfo::slotShowingDesktopChanged(bool show)
{
    _barScale = 1.0;
    if (!effects->showingDesktop() && effects->activeWindow() && _bottomControlview) {
        _bottomControlview->rootContext()->setContextProperty("barScale", _barScale);
    }
}

}
