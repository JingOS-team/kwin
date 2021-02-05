/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2007 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2008 Lucas Murray <lmurray@undefinedfire.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_DESKTOPGRID_H
#define KWIN_DESKTOPGRID_H

#include <kwineffects.h>
#include <QObject>
#include <QTimeLine>
#include <QHash>
class QTimer;

#include "kwineffectquickview.h"
#include "windowanimationmanager.h"

namespace KWin
{

class PresentWindowsEffectProxy;
class EffectQuickScene;


class PresentLayout
{
public:
    QPointF curScale;
    QPointF targetScale;
    QRectF curGeometry;
    QRectF targetGeometry;
};

class DesktopGridEffect
    : public Effect
{
    Q_OBJECT
    Q_PROPERTY(int zoomDuration READ configuredZoomDuration)
    Q_PROPERTY(int border READ configuredBorder)
    Q_PROPERTY(Qt::Alignment desktopNameAlignment READ configuredDesktopNameAlignment)
    Q_PROPERTY(int layoutMode READ configuredLayoutMode)
    Q_PROPERTY(int customLayoutRows READ configuredCustomLayoutRows)
    Q_PROPERTY(bool usePresentWindows READ isUsePresentWindows)
    // TODO: electric borders

public:
    DesktopGridEffect();
    ~DesktopGridEffect() override;
    void reconfigure(ReconfigureFlags) override;
    void prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void postPaintScreen() override;
    void prePaintWindow(EffectWindow* w, WindowPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void windowInputMouseEvent(QEvent* e) override;
    void grabbedKeyboardEvent(QKeyEvent* e) override;
    bool borderActivated(ElectricBorder border) override;
    bool isActive() const override;
    void drawWindow(EffectWindow *w, int mask, const QRegion &region, WindowPaintData &data) override;
    void postPaintWindow(EffectWindow* w) override;


    void onHideTaskManager() override;
    void onBottomGestureToggled() override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    int requestedEffectChainPosition() const override {
        return 50;
    }

    enum { LayoutPager, LayoutAutomatic, LayoutCustom }; // Layout modes
    enum { SwitchDesktopAndActivateWindow, SwitchDesktopOnly }; // Click behavior

    // for properties
    int configuredZoomDuration() const {
        return zoomDuration;
    }
    int configuredBorder() const {
        return border;
    }
    Qt::Alignment configuredDesktopNameAlignment() const {
        return desktopNameAlignment;
    }
    int configuredLayoutMode() const {
        return layoutMode;
    }
    int configuredCustomLayoutRows() const {
        return customLayoutRows;
    }
    bool isUsePresentWindows() const {
        return clickBehavior == SwitchDesktopAndActivateWindow;
    }

public:
    Q_SCRIPTABLE void clearWindows();

private Q_SLOTS:
    void toggle();
    // slots for global shortcut changed
    // needed to toggle the effect
    void globalShortcutChanged(QAction *action, const QKeySequence& seq);
    void slotAddDesktop();
    void slotRemoveDesktop();
    void slotWindowAdded(KWin::EffectWindow* w);
    void slotWindowClosed(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotNumberDesktopsChanged(uint old);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w, const QRect &old);

private:
    QPointF scalePos(const QPoint& pos, int desktop, int screen = -1) const;
    QPoint unscalePos(const QPoint& pos, int* desktop = nullptr) const;
    int posToDesktop(const QPoint& pos) const;
    EffectWindow* windowAt(QPoint pos) const;
    void setCurrentDesktop(int desktop);
    void setHighlightedDesktop(int desktop);
    int desktopToRight(int desktop, bool wrap = true) const;
    int desktopToLeft(int desktop, bool wrap = true) const;
    int desktopUp(int desktop, bool wrap = true) const;
    int desktopDown(int desktop, bool wrap = true) const;
    void setActive(bool active);
    void setup();
    void setupGrid();
    void finish();
    bool isMotionManagerMovingWindows() const;
    bool isRelevantWithPresentWindows(EffectWindow *w) const;
    bool isUsingPresentWindows() const;
    QRectF moveGeometryToDesktop(int desktop) const;
    void desktopsAdded(int old);
    void desktopsRemoved(int old);
    QVector<int> desktopList(const EffectWindow *w) const;
    void addCloseBtn(EffectWindow *w, const QRect &rect);
    void clearBtns();
    void deleteCloseBtn(EffectWindow *w);
    bool isOnCloseButton(EffectWindow *w, const QPoint& pos);

    void addMask();
    void addClearBtn();
    bool isOnClearButton(const QPoint& pos);
    void killWindow(const EffectWindow *w);

    std::chrono::milliseconds toStdMs(int ms);
    void getWindowLayout(bool init, int screen, WindowAnimationManager &windowAnimationManager);

    QList<ElectricBorder> borderActivate;
    int zoomDuration;
    int border;
    Qt::Alignment desktopNameAlignment;
    int layoutMode;
    int customLayoutRows;
    int clickBehavior;

    bool activated;
    QTimeLine timeline;
    int paintingDesktop;
    int highlightedDesktop;
    int sourceDesktop = 1;
    int m_originalMovingDesktop;
    bool keyboardGrab;
    bool wasWindowMove, wasWindowCopy, wasDesktopMove, isValidMove;
    EffectWindow* windowMove;
    QPoint windowMoveDiff;
    QPoint dragStartPos;
    QTimer *windowMoveElevateTimer;
    std::chrono::milliseconds lastPresentTime;

    // Soft highlighting
    QList<QTimeLine*> hoverTimeline;

    QList< EffectFrame* > desktopNames;

    QSize gridSize;
    Qt::Orientation orientation;
    QPoint activeCell;
    // Per screen variables
    QList<double> scale; // Because the border isn't a ratio each screen is different
    QList<double> unscaledBorder;
    QList<QSizeF> scaledSize;
    QList<QPointF> scaledOffset;

    // Shortcut - needed to toggle the effect
    QList<QKeySequence> shortcut;

    PresentWindowsEffectProxy* m_proxy;
    QList<WindowAnimationManager> m_animateManagers;
    QRect m_windowMoveGeometry;
    QPoint m_windowMoveStartPoint;

    QVector<EffectQuickScene*> maskView;
    QVector<EffectQuickScene*> m_desktopButtons;
    QVector<EffectQuickScene*> m_clearButtons;
    bool clickCloseWindow = false;
    QHash<EffectWindow *, EffectQuickScene*> m_windowCloseButtons;
    QAction *m_activateAction;

    EffectWindow* closeWindow = nullptr;
    EffectWindow* activeWindow = nullptr;
    bool showDesktop = false;
    bool isClearWindows = false;
    QPoint lastPos;
    QHash<EffectWindow*, PresentLayout> windowLayouts;
};

} // namespace

#endif
