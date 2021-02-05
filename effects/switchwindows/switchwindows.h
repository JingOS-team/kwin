/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SWITCHWINDOWS_H
#define SWITCHWINDOWS_H

#include <kwineffects.h>
#include "windowanimationmanager.h"
#include <QList>

namespace KWin
{

class SwitchWindows : public Effect
{
    Q_OBJECT
public:
    enum SwitchState {
        S_NONE,
        S_IDLE,
        S_FORWARD,
        S_FORWARD_BOUND,
        S_BACKWARD,
        S_BACKWARD_BOUND,
        S_END
    };

    SwitchWindows();

    //void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintScreen() override;
    void postPaintWindow(EffectWindow* w) override;
    bool isActive() const override;

    static bool supported();

public Q_SLOTS:
    void init();
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotSwitchWindow(bool toRight);
    void slotShowingDesktopChanged(bool show);
    void slotWindowAdded(KWin::EffectWindow *w);

private:
    bool isShowingDesktop();
    QPointF scalePos(const QPoint &pos, int screen, const QPointF &scale) const;
    void fillList();
    void witchWindow(bool isForward, bool isOnDesktop = false);
    void doSwitchWindow(KWin::EffectWindow *old, KWin::EffectWindow *cur, bool isForward);
    void doBoundWindow(KWin::EffectWindow *w, bool isForward);
    QPointF getWindowSwitchPos(EffectWindow *w, bool isForward);
    QPointF getWindowBoundToPos(EffectWindow *w, bool isForward);
    std::chrono::milliseconds toStdMs(int ms);
    QPointF getWindowScale(EffectWindow *w, qreal scale);

    int getAniDuration();
private:
    bool _revert = false;
    int _pending = 0;
    qreal _scale = 0.9;
    qreal _boundScale = 0.9;
    int _aniDuration = 500;
    int _topDockHeight = 36;
    int _boundDuration = 500;

    bool _started = false;
    SwitchState _switchState = S_NONE;
    QTimer *_resetTimer;
    QList<KWin::EffectWindow*> _windows;
    QList<KWin::EffectWindow*>::iterator _curItor;
    WindowAnimationManager  _manager;
};

}

#endif // SWITCHWINDOWS_H
