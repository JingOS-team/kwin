/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TASKPANEL_H
#define TASKPANEL_H

#include <kwineffects.h>

#include <QRectF>
#include <QTimer>
#include <QVector>

#include "kwineffectquickview.h"
#include "tasklist.h"

namespace KWin
{

class TaskPanel : public Effect
{
    Q_OBJECT
public:
    TaskPanel();
    void prePaintScreen(ScreenPrePaintData& data, std::chrono::milliseconds presentTime) override;
    void paintScreen(int mask, const QRegion &region, ScreenPaintData& data) override;
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintWindow(EffectWindow* w) override;
    void postPaintScreen() override;
    bool isActive() const override;

    void windowInputMouseEvent(QEvent* e) override;
    void grabbedKeyboardEvent(QKeyEvent* e) override;

    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

    static bool supported();

    Q_SCRIPTABLE void clearWindows();
private slots:
    void slotSwitchWindow(bool toRight);
    void onMove(const QSizeF &delta, qreal progress);
    void onTaskStateChanged(TaskManager::TaskState taskState, TaskManager::TaskState oldState);
    void animatingChanged(bool isAnimating, qreal animateProgress);
    void resetData();

    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotShowingDesktopChanged(bool show);
    void slotWindowAdded(KWin::EffectWindow *w);

    void slotGestueEnd(const QSizeF& delta, const QSizeF &spead);

    void toggle();
private:
    bool isCloseState();
    bool isTaskPanelState();

    void closeTask(EffectWindow *window);

    void addClearBtn();
    bool isOnClearButton(const QPoint& pos);
    void clearBtns();

    void toTaskGride(EffectWindow * window);
    void setupWindows(bool animate);
    void showDesktop(bool direct);
    bool isRelevantWithPresentWindows(EffectWindow *w) const;
    QPointF scalePos(const QPointF &pos, int screen, qreal progress) const;
    QSizeF scaleSize(int screen, qreal scale) const;
    bool isShowingDesktop();
    EffectWindow* windowAt(QPoint pos);

    EffectWindow* currentWindow();
    bool isManageWindowType(EffectWindow *w);
    bool isSystemUI(EffectWindow *w);

    QScopedPointer<GLTexture> m_clearTexture;
private:
    qreal _screenScale = 1.;
    bool _delayReset = false;
    bool _isActive = false;
    qreal _curBrightness = 1;
    qreal _lastSpead = 0;
    ulong _lastTime = 0;
    bool _hasTouchDown = false;
    qint32 _lastTouchId = -1;
    QPoint _startPressPos;
    QPoint _lastPressPos;
    bool _motionProcessed = false;
    QList<QKeySequence> _shortcut;
    QList<QKeySequence> _shortcut1;
    QAction *m_activateAction;
    QAction *m_activateAction2;
    bool keyboardGrab = false;;
    bool _startMoveFromDesktop = false;
    qreal _moveProgress = 0.;
    qreal _animateProgress = 0.;
    qreal _lastScale = 1.;
    QPointF _lastPos = QPointF(0., 0.);
    TaskList _taskList;
    bool _hideList = false;
    QTimer _resetTimer;
    bool _isSetupAnimate = false;
    EffectWindow *_activeWindow = nullptr;
    bool _isMoveing = false;
    bool _isDragging = false;
    QVector<EffectQuickScene*> m_clearButtons;
};

}

#endif // TASKPANEL_H
