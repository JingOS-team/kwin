/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SHOWSYSINFO_H
#define SHOWSYSINFO_H

#include <kwineffects.h>
#include "kwineffectquickview.h"
namespace KWin
{

class ShowSysInfo : public Effect
{
    Q_OBJECT
public:
    ShowSysInfo();
    void prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void postPaintScreen() override;
    bool isActive() const override;
    virtual bool pointerEvent(QMouseEvent* e) override;

    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintWindow(EffectWindow* w) override;

    static bool supported();

    void hideDockBg(bool animate = true);
    void showDockBg(bool animate = true);

    Q_SCRIPTABLE void showDesktop();
    Q_SCRIPTABLE void enterControlBar();
    Q_SCRIPTABLE void leaveControlBar();

    Q_SCRIPTABLE void switchWindow(bool toRight);

    static bool enabledByDefault();
private:
    bool isShowNextWindow();
    bool isShowingDesktop() const;

private slots:
    void slotShowingDesktopChanged(bool show);
    void slotAboutToQuit();

private:
    bool _isShowingDockBg = true;
    TimeLine _timeLine;
    qreal _dockBgAlphaRate = 1.0;
    TimeLine _bottomAnimationTimeLine;
    bool _hasTouchDown = false;
    QPoint _lastPressPos;
    QPoint _startPressPos;
    bool _pressed = false;
    qreal _barScale = 1.0;
    QRect _bottomRect;
    EffectQuickScene *_noticeView;
    EffectQuickScene *_bottomControlview;
    std::chrono::milliseconds _lastPresentTime = std::chrono::milliseconds::zero();
    QScopedPointer<GLTexture> _quitTexture;
    bool _isShowingQuitBg = false;
};

}

#endif // SHOWSYSINFO_H
