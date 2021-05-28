/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TASKLIST_H
#define TASKLIST_H
#include <QObject>
#include <QRectF>
#include <QHash>
#include <QDebug>
#include "kwineffects.h"
#include "kwineffectquickview.h"

namespace KWin {


class WindowItem {
public:
    WindowItem() {

    }

    WindowItem(const WindowItem& item){
        w = item.w;
        _oriPos = item._oriPos;
        _curPos = item._curPos;
        _srcPos = item._srcPos;
        _destPos = item._destPos;

        _oriScale = item._oriScale ;
        _curScale = item._curScale ;
        _srcScale = item._srcScale ;
        _destScale = item._destScale;

       _curOpacity = item._curOpacity ;
       _srcOpacity = item._srcOpacity ;
       _destOpacity = item._destOpacity;

        _isPosAnimating = item._isPosAnimating;
        _isScaleAnimating = item._isScaleAnimating;
        _isOpacityAnimating = item._isOpacityAnimating;
        _isToClose = item._isToClose;
    }

    EffectWindow *w = nullptr;
    QPointF _oriPos = QPointF(0., 0.);
    QPointF _curPos = QPointF(0., 0.);
    QPointF _srcPos = QPointF(0., 0.);
    QPointF _destPos = QPointF(0., 0.);

    QSizeF _oriScale = QSizeF(1., 1.);
    QSizeF _curScale = QSizeF(1., 1.);
    QSizeF _srcScale = QSizeF(1., 1.);
    QSizeF _destScale = QSizeF(1., 1.);

    qreal _curOpacity = 1.;
    qreal _srcOpacity = 1.;
    qreal _destOpacity = 1.;

    bool _isPosAnimating = false;
    bool _isScaleAnimating = false;
    bool _isOpacityAnimating = false;
    bool _isToClose = false;
};

class TaskList : public QObject
{
    Q_OBJECT
public:
    enum Orientation {
        Horizontal,
        Vertical
    };
    enum LayoutDirection {
        LeftToRight,
        RightToLeft
    };
    enum VerticalLayoutDirection {
        TopToBottom,
        BottomToTop
    };

    explicit TaskList(QObject *parent = nullptr);

    void windowToOriginal(EffectWindow* window, bool animate);
    void toOriginal(bool toNormal);
    void toNormalWindow(EffectWindow* window);

    bool nextPage(bool toNormal);
    bool prePage(bool toNormal);

    void initCurValue();

    void remove(EffectWindow* window);

    int curPageNum();
    void stopSlide();

    void hideItem(EffectWindow* curWindow);
    void addWindowToGrid(EffectWindow* w);
    void removeWindowFromGrid(EffectWindow* w);

    void hideList(EffectWindow* curWindow);
    void showList(qreal x, qreal y, EffectWindow* curWindow);

    void moveItem(QSizeF size, EffectWindow* window);
    void scaleItems(QSizeF scale);
    void translateItemsOnDirection(qreal x, qreal y, bool animating);
    void translateItems(qreal x, qreal y, EffectWindow* w, qreal scale);
    void translateItem(qreal x, qreal y, EffectWindow* w);
    void setItemOpacity(qreal opacity);

    void moveStop(const QSizeF &spead);

    void setupWindowItems(EffectWindowList windowlist, int screen, EffectWindow* curWindow, bool animation);

    QHash<EffectWindow*, WindowItem*> toGrideModel(EffectWindow* curWindow);
    QHash<EffectWindow*, WindowItem*> getItems();

    void clear();
    bool isManageWindow(EffectWindow* w);
    bool isRemoving(EffectWindow* w);
    WindowItem* getWindowItem(EffectWindow* w);
    WindowItem* getRemoveWindowItem(EffectWindow *w);

    void updateTime(std::chrono::milliseconds presentTime);

    bool isAnimating() {
        return _isAnimating;
    }

    bool hasSetup() {
        return _hasSetup;
    }

    QSizeF pageSize() {
        return _pageSize;
    }

    int itemsCount() {
        return _items.size();
    }

    bool isResetingGrid() {
        return _resetingGride;
    }

    bool isSliding() {
        return _isSliding;
    }
Q_SIGNALS:
    void isAnimatingChanged(bool, qreal);

private:
    qreal pageHeight(EffectWindow* w);
    void resetAnimate(int time, QEasingCurve::Type type = QEasingCurve::OutQuint);
    void makeupGride(QList<QHash<EffectWindow*, WindowItem*>::iterator> items, int screen, int curIndex);
    bool boundCheck();
    void rebound(QSize size);
    void resetSpeadLine(int time);

private:
    int _hM = 10;
    int _vM = 10;
    bool _waitForNextAnimate = false;
    bool _isSliding = false;
    bool _resetingGride = false;
    qreal _spead = 0.;
    qreal _acceleration = 0.;
    QSizeF _pageSize;
    bool _showPanel;
    bool _hasSetup = false;
    int _curPageIndex = 0;
    int _pageCount = 0;
    TimeLine _timeLine;
    TimeLine _speadLine;
    QPointF _headPos;
    QPointF _tailPos;
    bool  _bigThenOnPage;
    int _elementWidth;
    QSizeF _elementSize;
    bool _isAnimating = false;
    QHash<EffectWindow*, WindowItem*> _removeList;
    QList<QHash<EffectWindow*, WindowItem*>::iterator> _itemsIndex;
    QHash<EffectWindow*, WindowItem*> _items;
    Orientation _orientation = Horizontal;
    LayoutDirection _layoutDirection = RightToLeft;
    VerticalLayoutDirection _verticalLayoutDirection = BottomToTop;
    std::chrono::milliseconds _lastPresentTime = std::chrono::milliseconds::zero();
};

}
#endif // TASKLIST_H
