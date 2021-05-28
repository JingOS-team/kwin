/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tasklist.h"
#include "presentwindowsconfig.h"

#include <QRect>
#include <cmath>

namespace KWin {

const int MARGIN = 100;
const int ITEMS_PER_PAGE = 9;
const int SLIDE_TIME = 1500;
const int ROW_COUNT = 2;
TaskList::TaskList(QObject *parent)
    : QObject(parent)
{
    _timeLine.setEasingCurve(QEasingCurve::Linear);
    _showPanel = PresentWindowsConfig::showPanel();

    _speadLine.setEasingCurve(QEasingCurve::InExpo);
    _speadLine.setDirection(TimeLine::Backward);
}

void TaskList::windowToOriginal(EffectWindow *window, bool animate)
{
    if (_items.contains(window)) {
        auto item = _items.find(window).value();
        item->_destPos = item->_oriPos;
        if (animate) {
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        } else {
            item->_srcPos = item->_curPos = item->_destPos;
        }

        item->_destScale = item->_oriScale;
        if (animate) {
            item->_srcScale = item->_curScale;
            item->_isScaleAnimating = true;
        } else {
            item->_srcScale = item->_curScale = item->_destScale;
        }

        item->_destOpacity = 1.;
        if (animate) {
            item->_srcOpacity = item->_curOpacity;
            item->_isOpacityAnimating = true;
        } else {
            item->_srcOpacity = item->_curOpacity = item->_destOpacity;
        }
    }
}

void TaskList::initCurValue()
{
    for (WindowItem *item : _items.values()) {
        item->_srcPos = item->_curPos = item->_destPos = item->_oriPos;
        item->_isPosAnimating = false;

        item->_curScale = item->_srcScale = item->_destScale = QSizeF(item->w->appScale(), item->w->appScale());
        item->_isScaleAnimating = false;

        item->_curOpacity = item->_srcOpacity = item->_destOpacity = 1.;
        item->_isOpacityAnimating = false;
    }
}

void TaskList::remove(EffectWindow *window)
{
    _resetingGride = true;
    auto it = _items.find(window);
    WindowItem *item = it.value();

    item->_isToClose = true;
    item->_srcPos = item->_curPos;
    item->_destPos = QPointF(item->_curPos.x(), window->height() * item->_curScale.height() * -1);
    item->_isPosAnimating = true;

    resetAnimate(_itemsIndex.indexOf(it) % 2 == 0 ? 90 : 180, QEasingCurve::Linear);
}

void TaskList::stopSlide()
{
    _speadLine.stop();
}

void TaskList::toOriginal(bool toNormal)
{
    for (WindowItem *item : _items.values()) {
        item->_destPos = item->_oriPos;
        item->_srcPos = item->_curPos;
        item->_isPosAnimating = true;
        if (toNormal) {
            item->_destScale = QSizeF(item->w->appScale(), item->w->appScale());
            item->_srcScale = item->_curScale;
            item->_isScaleAnimating = true;

            item->_destOpacity = 1.;
            item->_srcOpacity = item->_curOpacity;
            item->_isOpacityAnimating = true;
        }
    }
    resetAnimate(150);
}

void TaskList::toNormalWindow(EffectWindow *window)
{
    for (WindowItem* item : _items.values()) {
        if (item->w == window) {
            QRect geometry = window->geometry();
            item->_destPos = geometry.topLeft();
            if (item->_curPos.x() < (geometry.width() * item->_curScale).width() * -1){
                item->_srcPos = item->_curPos = QPointF((geometry.width() * item->_curScale).width() * -1, item->_curPos.y());
            } else if (item->_curPos.x() > _pageSize.width()) {
                item->_srcPos = item->_curPos = QPointF( _pageSize.width(), item->_curPos.y());
            } else {
                item->_srcPos = item->_curPos;
            }
            item->_isPosAnimating = true;

            item->_destScale = QSizeF(window->appScale(), window->appScale());
            item->_srcScale = item->_curScale;
            item->_isScaleAnimating = true;

            item->_destOpacity = 1.;
            item->_srcOpacity = item->_curOpacity;
            item->_isOpacityAnimating = true;
        } else {
            item->_destOpacity = 0;
            item->_srcOpacity = item->_curOpacity;
            item->_isOpacityAnimating = true;
        }
    }

    resetAnimate(250);
}

bool TaskList::nextPage(bool toNormal)
{
    if (_curPageIndex  < _pageCount - 1) {

        ++_curPageIndex;
        auto w = _itemsIndex[_curPageIndex].key();

        effects->showDockBg(w->isJingApp(), false);
        effects->activateWindowWhithoutAnimation(w);

        resetAnimate(toNormal ? 400 : 300);
        for (WindowItem *item : _items.values()) {
            if (_orientation == Horizontal) {
                if (_layoutDirection == LeftToRight) {
                    item->_destPos = item->_oriPos - QPointF((pageSize().width() + MARGIN), 0.);
                } else {
                    item->_destPos = item->_oriPos + QPointF((pageSize().width() + MARGIN), 0.);
                }
            } else {
                if (_verticalLayoutDirection == TopToBottom) {
                    item->_destPos = item->_oriPos - QPointF(0, _pageSize.height() + MARGIN);
                } else {
                    item->_destPos = item->_oriPos + QPointF(0, _pageSize.height() + MARGIN);
                }
            }

            if (toNormal) {
                item->_destScale = QSizeF(item->w->appScale(), item->w->appScale());
                item->_srcScale = item->_curScale;
                item->_isScaleAnimating = true;

                item->_srcOpacity = item->_curOpacity;
            }

            item->_oriPos = item->_destPos;
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        }

        return true;
    }

    toOriginal(toNormal);

    return false;
}

bool TaskList::prePage(bool toNormal)
{
    if (_curPageIndex >= 1 && _pageCount > 0) {
        --_curPageIndex;

        auto w = _itemsIndex[_curPageIndex].key();

        effects->showDockBg(w->isJingApp(), false);
        effects->activateWindowWhithoutAnimation(w);

        resetAnimate(toNormal ? 400 : 300);
        for (WindowItem *item : _items.values()) {
            if (_orientation == Horizontal) {
                if (_layoutDirection == LeftToRight) {
                    item->_destPos = item->_oriPos + QPointF((pageSize().width() + MARGIN), 0.);
                } else {
                    item->_destPos = item->_oriPos - QPointF((pageSize().width() + MARGIN), 0.);
                }
            } else {
                if (_verticalLayoutDirection == TopToBottom) {
                    item->_destPos = item->_oriPos + QPointF(0, _pageSize.height() + MARGIN);
                } else {
                    item->_destPos = item->_oriPos - QPointF(0, _pageSize.height() + MARGIN);
                }
            }

            if (toNormal) {
                item->_destScale = QSizeF(item->w->appScale(), item->w->appScale());
                item->_srcScale = item->_curScale;
                item->_isScaleAnimating = true;

                item->_srcOpacity = item->_curOpacity;
            }

            item->_oriPos = item->_destPos;
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        }

        return true;
    }

    toOriginal(toNormal);

    return false;
}

void TaskList::hideItem(EffectWindow *curWindow)
{
    for (WindowItem* item : _items.values()) {
        if (item->w == curWindow) {
            item->_destScale = QSizeF(0., 0.);
            item->_srcScale = item->_curScale;
            item->_isScaleAnimating = true;

            item->_destPos = curWindow->geometry().center();
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        } else {
            item->_destOpacity = 0;
            item->_srcOpacity = item->_curOpacity;
            item->_isOpacityAnimating = true;
        }
    }

    resetAnimate(250);
}

void TaskList::addWindowToGrid(EffectWindow *w)
{
    _resetingGride = true;
    WindowItem *item = new WindowItem;
    item->w = w;
    item->_curPos = item->_srcPos = item->_oriPos = item->_destPos = QPointF(0, 0);
    _itemsIndex.prepend(_items.insert(w, item));
    int size =_items.size();
    if (size > 1) {
        for (int i = 0; i < _itemsIndex.size(); i++) {
            auto item = _itemsIndex[i].value();
            if (i + 1 < _itemsIndex.size()) {
                auto nextItem = _itemsIndex[i + 1].value();
                item->_oriPos = item->_destPos = nextItem->_curPos;
                item->_srcPos = item->_curPos;
                item->_isPosAnimating = true;
                if (i == 0) {
                    item->_curScale = nextItem->_curScale;
                }
            }
        }

        auto item = _itemsIndex.last().value();
        if (size % ROW_COUNT == 0) {
            item->_oriPos = item->_destPos = _itemsIndex[size - 2].value()->_destPos + QPointF(0,  _elementSize.height());
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        } else {
            item->_oriPos = item->_destPos = _itemsIndex[size - 3].value()->_destPos - QPointF(_elementSize.width(),  0);
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        }

        _bigThenOnPage = ceil(_items.size() / double(ROW_COUNT)) * _elementSize.width() > _pageSize.width();
        resetAnimate(300);
    } else {
        toGrideModel(nullptr);
    }

}

void TaskList::removeWindowFromGrid(EffectWindow *w)
{
    if (_items.contains(w)) {
        QRect area = effects->clientArea(ScreenArea, 0, effects->currentDesktop());
        if (_showPanel)   // reserve space for the panel
            area = effects->clientArea(MaximizeArea, 0, effects->currentDesktop());

        auto it = _items.find(w);
        int index = _itemsIndex.indexOf(it);
        int moveDistance = 0;
        if (_itemsIndex.size() > 6 && _itemsIndex.size() % ROW_COUNT == 1) {
            int lastDistance = _itemsIndex[_items.size() - 2].value()->_oriPos.x();
            if (lastDistance > 0) {
                moveDistance = 0 - lastDistance;
            }
        } else if (_itemsIndex.size() == 5) {
            int firstDistance = _itemsIndex.first().value()->_oriPos.x() - _headPos.x();
            if (firstDistance > 0) {
                moveDistance = 0 - firstDistance;
            }
        }
        QPointF movePos(moveDistance, 0);
        for (int i = _itemsIndex.size() - 1; i >= 0 ; i--) {
            auto item = _itemsIndex[i].value();
            if (i > index && i > 0) {
                if (i % ROW_COUNT == 0) {
                    item->_destPos = QPointF(item->_oriPos.rx() + _elementSize.width(), item->_oriPos.ry() + (ROW_COUNT - 1) * _elementSize.height()) + movePos;
                } else {
                    item->_destPos = QPointF(item->_oriPos.rx(), item->_oriPos.ry() - _elementSize.height()) + movePos;
                }
            } else {
                item->_destPos = item->_oriPos + movePos;
            }


            bool shownNow = QRectF(item->_curPos, QSizeF(item->w->size().width() * item->_curScale.width(), item->w->size().height() * item->_curScale.height())).intersects(area);
            bool toShow = QRectF(item->_destPos, QSizeF(item->w->size().width() * item->_curScale.width(), item->w->size().height() * item->_curScale.height())).intersects(area);
            if (shownNow || toShow) {
                item->_oriPos = item->_destPos;
                item->_srcPos = item->_curPos;
                item->_isPosAnimating = true;
            } else {
                item->_srcPos = item->_curPos = item->_oriPos = item->_destPos;
            }
        }

        delete it.value();
        _itemsIndex.removeOne(it);
        _items.remove(w);

        _bigThenOnPage = ceil(_items.size() / double(ROW_COUNT)) * _elementSize.width() > _pageSize.width();
        resetAnimate(200, QEasingCurve::Linear);
        _resetingGride = true;
        _waitForNextAnimate = false;
    }
}

void TaskList::hideList(EffectWindow* curWindow)
{
    if (_itemsIndex.size() < 1) {
        return;
    }

    auto it = _items.find(curWindow);
    if (it == _items.end()) {
        return;
    }

    resetAnimate(150);
    auto lIndex = _itemsIndex.indexOf(it);

    if (_orientation == Horizontal) {
        if (lIndex > 0) {
            auto pIt = _itemsIndex[lIndex - 1];
            (pIt).value()->_destPos = QPointF((_layoutDirection == RightToLeft) ? pageSize().width() : 0 - pageSize().width() *  pIt.value()->_curScale.width(), pIt.value()->_curPos.y());
            pIt.value()->_isPosAnimating = true;
            pIt.value()->_srcPos = pIt.value()->_curPos;
        }

        if (lIndex < _itemsIndex.size() - 1) {
            auto nIt = _itemsIndex[lIndex + 1];
            nIt.value()->_destPos = QPointF((_layoutDirection == RightToLeft) ? 0 - pageSize().width() *  nIt.value()->_curScale.width() : pageSize().width(), nIt.value()->_curPos.y());
            nIt.value()->_isPosAnimating = true;
            nIt.value()->_srcPos = nIt.value()->_curPos;
        }

    } else {
        if (lIndex > 0) {
            auto pIt = _itemsIndex[lIndex - 1];
            (pIt).value()->_destPos = QPointF(pIt.value()->_curPos.x() ,(_verticalLayoutDirection == BottomToTop) ? 0 : pageHeight(pIt.key()));
            pIt.value()->_isPosAnimating = true;
            pIt.value()->_srcPos = pIt.value()->_curPos;
        }

        if (lIndex < _itemsIndex.size() - 1) {
            auto nIt = _itemsIndex[lIndex + 1];
            nIt.value()->_isPosAnimating = true;
            nIt.value()->_destPos = QPointF(nIt.value()->_curPos.x() ,(_verticalLayoutDirection == BottomToTop) ? pageHeight(nIt.key()) : 0);
            nIt.value()->_srcPos = nIt.value()->_curPos;
        }
    }
}

void TaskList::showList(qreal x, qreal y, EffectWindow *curWindow)
{
    if (_itemsIndex.size() < 1) {
        return;
    }
    auto it = _items.find(curWindow);
    if (it == _items.end()) {
        return;
    }
    resetAnimate(150);
    WindowItem *pCurItem = it.value();
    pCurItem->_curPos =  QPointF(x, y);

    auto lIndex = _itemsIndex.indexOf(it);
    if (_orientation == Horizontal) {
        int direction =  (_layoutDirection == RightToLeft) ? 1 : -1;
        if (lIndex > 0) {
            auto pIt = _itemsIndex[lIndex - 1];
            pIt.value()->_destPos = pCurItem->_curPos - (QPointF((pageSize().width() *  pCurItem->_curScale.width() + MARGIN) * -1, 0)) * direction;
            pIt.value()->_isPosAnimating = true;
            pIt.value()->_srcPos = pIt.value()->_curPos;
        }
        if (lIndex < _itemsIndex.size() - 1) {
            auto nIt = _itemsIndex[lIndex + 1];
            nIt.value()->_destPos = pCurItem->_curPos - (QPointF((pageSize().width() *  pCurItem->_curScale.width() + MARGIN), 0)) * direction;
            nIt.value()->_isPosAnimating = true;
            nIt.value()->_srcPos = nIt.value()->_curPos;
        }

    } else {
        int direction =  (_verticalLayoutDirection == BottomToTop) ? 1 : -1;
        if (lIndex > 0) {
            auto pIt = _itemsIndex[lIndex - 1];
            pIt.value()->_destPos = pCurItem->_curPos - (QPointF(0, (pageSize().width() *  pCurItem->_curScale.width() + MARGIN) * -1)) * direction;
            pIt.value()->_isPosAnimating = true;
            pIt.value()->_srcPos = pIt.value()->_curPos;
        }
        if (lIndex < _itemsIndex.size() - 1) {
            auto nIt = _itemsIndex[lIndex + 1];
            nIt.value()->_destPos = pCurItem->_curPos - (QPointF(0, (pageSize().width() *  pCurItem->_curScale.width() + MARGIN))) * direction;
            nIt.value()->_isPosAnimating = true;
            nIt.value()->_srcPos = nIt.value()->_curPos;
        }
    }

    qDebug()<<Q_FUNC_INFO<<pCurItem->_curPos;
    if (!pCurItem->w->isJingApp()) {
        pCurItem->_curPos += QPointF(0., effects->panelGeometry().height() * pCurItem->_curScale.height());
    }
    qDebug()<<Q_FUNC_INFO<<pCurItem->_curPos;
}

void TaskList::moveItem(QSizeF size, EffectWindow *window)
{
    auto it = _items.find(window);
    if (it != _items.end()) {
        it.value()->_curPos += QPointF(0, size.height());
    }
}

void TaskList::scaleItems(QSizeF scale)
{
    for (WindowItem *item : _items.values()) {
        item->_curScale = QSizeF(item->w->appScale() * scale.width(), item->w->appScale() * scale.height());
    }
}

void TaskList::translateItemsOnDirection(qreal x, qreal y, bool animating)
{
    if (x * _spead < 0.001) {
        _speadLine.stop();
    }
    for (int i = 0; i < _itemsIndex.size(); i++) {
        auto item = _itemsIndex[i].value();
        if (_orientation == Horizontal) {
            if (!animating) {
                item->_oriPos = item->_curPos = item->_curPos + QPointF(x, 0);
            } else {
                item->_oriPos = item->_destPos = item->_curPos + QPointF(x, 0);
                item->_srcPos = item->_curPos;
                item->_isPosAnimating = true;
            }
        } else {
            if (!animating) {
                item->_oriPos = item->_curPos = item->_curPos + QPointF(y, 0);
            } else {
                item->_oriPos = item->_destPos = item->_curPos + QPointF(y, 0);
                item->_srcPos = item->_curPos;
                item->_isPosAnimating = true;
            }
        }
    }
}

void TaskList::translateItems(qreal x, qreal y, EffectWindow* w, qreal scale)
{
    auto lIndex =  -1;
    QPointF curPos = QPointF(x, y);

    if (w != nullptr) {
        auto it = _items.find(w);
        if (it != _items.end()) {
            WindowItem *pCurItem = it.value();
            pCurItem->_curPos = curPos;
            lIndex =  _itemsIndex.indexOf(it);
        }
    }

    for (int i = 0; i < _itemsIndex.size(); i++) {
        int num = i - lIndex;
        auto item = _itemsIndex[i].value();
        if (_orientation == Horizontal) {
            int direction =  (_layoutDirection == RightToLeft) ? 1 : -1;
            if (item->_isPosAnimating) {
                item->_destPos = curPos - (QPointF((pageSize().width() *  scale + MARGIN) * num, 0)) * direction;
                qDebug()<<Q_FUNC_INFO<<__LINE__<<item->_destPos<<" "<<effects->panelGeometry()<<" "<<item->_curScale;
                if (!item->w->isJingApp()) {
                    item->_destPos += QPointF(0., effects->panelGeometry().height() * item->_curScale.height());
                }
                qDebug()<<Q_FUNC_INFO<<__LINE__<<item->_destPos;
            } else {
                item->_curPos = curPos - (QPointF((pageSize().width() *  scale + MARGIN) * num, 0)) * direction;
                qDebug()<<Q_FUNC_INFO<<__LINE__<<item->_curPos<<" "<<effects->panelGeometry()<<" "<<item->_curScale;
                if (!item->w->isJingApp()) {
                    item->_curPos += QPointF(0., effects->panelGeometry().height() * item->_curScale.height());
                }
                qDebug()<<Q_FUNC_INFO<<__LINE__<<item->_curPos;
            }
        } else {
            int direction =  (_verticalLayoutDirection == BottomToTop) ? 1 : -1;
            if (item->_isPosAnimating) {
                item->_destPos = curPos - (QPointF(0, (pageHeight(item->w) *  scale + MARGIN) * num)) * direction;
            } else {
                item->_curPos = curPos - (QPointF(0, (pageHeight(item->w) *  scale + MARGIN) * num)) * direction;
            }
        }
    }

}

void TaskList::translateItem(qreal x, qreal y, EffectWindow *w)
{
    auto it = _items.find(w);
    if (it == _items.end()) {
        return;
    }
    WindowItem *pCurItem = it.value();
    pCurItem->_curPos =  QPointF(x, y);
    qDebug()<<Q_FUNC_INFO<<pCurItem->_curPos;
    if (!w->isJingApp()) {
        pCurItem->_curPos += QPointF(0., effects->panelGeometry().height() * pCurItem->_curScale.height());
    }
    qDebug()<<Q_FUNC_INFO<<pCurItem->_curPos;
}

void TaskList::setItemOpacity(qreal opacity)
{
    for (WindowItem* item : _items.values()) {
        item->_curOpacity = opacity;
    }
}

void TaskList::moveStop(const QSizeF &spead)
{
    if (_orientation == Horizontal) {
        int direction =  (_layoutDirection == RightToLeft) ? 1 : -1;
        _spead = spead.width() * direction;
    } else {
        int direction =  (_verticalLayoutDirection == BottomToTop) ? 1 : -1;
        _spead = spead.height() * direction;
    }

    if (std::abs(_spead) > 10) {
        _spead = (std::sqrt(std::abs(_spead)) + 10) * std::abs(_spead) / _spead;
    }
    if (boundCheck()) {
        _acceleration = _spead / SLIDE_TIME;
        resetSpeadLine(SLIDE_TIME);
    }
}

WindowItem *TaskList::getWindowItem(EffectWindow *w)
{
    if (_items.contains(w)) {
        return _items.value(w);
    }

    return nullptr;
}

WindowItem *TaskList::getRemoveWindowItem(EffectWindow *w)
{
    if (_removeList.contains(w)) {
        return _removeList.value(w);
    }

    return nullptr;
}

void TaskList::updateTime(std::chrono::milliseconds presentTime)
{
    bool isAnimating = false;
    std::chrono::milliseconds delta = std::chrono::milliseconds::zero();
    if (_lastPresentTime.count()) {
        delta = presentTime - _lastPresentTime;
    }
    _lastPresentTime = presentTime;

    _timeLine.update(delta);
    _speadLine.update(delta);

    const qreal progress = _timeLine.value();

    QPointF distance = QPoint(0, 0);
    if (!_speadLine.done()) {
        qreal spead = _spead * _speadLine.value();
        distance = QPointF(spead * delta.count() - _acceleration * sqrt(delta.count()) / 2, 0);
        if (std::abs(distance.x()) < 0.1) {
            _speadLine.stop();
        }
    }

    QList<WindowItem*> items = _items.values();
    items.append(_removeList.values());
    for (WindowItem* item : items) {
        if (item->_isPosAnimating) {
            item->_curPos = item->_srcPos + (item->_destPos - item->_srcPos) * progress;
        }

        if (!_speadLine.done()) {
            item->_curPos = item->_curPos += distance;
            item->_oriPos = item->_curPos;
        }

        if (_timeLine.done() && _speadLine.done()) {
            item->_isPosAnimating = false;
            if (item->_isToClose) {
                _waitForNextAnimate = true;
                item->_srcScale = item->_curScale = item->_destScale = item->_oriScale;
                effects->setElevatedWindow(item->w, false);
                item->w->kill();
            }
        } else {
            isAnimating = true;
        }

        if (item->_isScaleAnimating) {
            item->_curScale = item->_srcScale + (item->_destScale - item->_srcScale) * progress;
            if (_timeLine.done()) {
                item->_isScaleAnimating = false;
            } else {
                isAnimating = true;
            }
        }

        if (item->_isOpacityAnimating) {
            item->_curOpacity = item->_srcOpacity + (item->_destOpacity - item->_srcOpacity) * progress;
            if (_timeLine.done()) {
                item->_isOpacityAnimating = false;
            } else {
                isAnimating = true;
            }
        }
    }

    if (!_speadLine.done()) {
        if (!boundCheck()) {
            _speadLine.stop();
            _isSliding = false;
        }
    } else if (_isSliding) {
        _isSliding = false;
    }
    _isAnimating = isAnimating;
    if (_resetingGride && !_isAnimating && !_waitForNextAnimate) {
        _resetingGride = false;
    }
    emit isAnimatingChanged(isAnimating, progress);
}

qreal TaskList::pageHeight(EffectWindow* w)
{
    return pageSize().height() -  w->isJingApp() ? 0 : effects->panelGeometry().height();
}

void TaskList::resetAnimate(int time, QEasingCurve::Type type)
{
    _lastPresentTime =  std::chrono::milliseconds::zero();
    _timeLine.setEasingCurve(type);
    _timeLine.setDuration(std::chrono::milliseconds(static_cast<int>(Effect::animationTime(time))));
    _timeLine.reset();
}

void TaskList::makeupGride(QList<QHash<EffectWindow*, WindowItem*>::iterator> items, int screen, int curIndex)
{
    // This layout mode requires at least one window visible
    if (items.count() == 0)
        return;

    QRect area = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (_showPanel)   // reserve space for the panel
        area = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());

    int rows = ROW_COUNT;
    int columns = int(ceil(items.count() / double(rows)));

    _hM = 28 / 640.0 * area.width();
    _vM = 48 / 472.0 * area.height();
    area.adjust(26 / 640.0 * area.width(), 30 / 472.0 * area.height(), -26 / 640.0 * area.width(), -68 / 472.0 * area.height());

    // Assign slots
    int slotWidth = (area.width() + _hM) / 2.5;
    int slotHeight = area.height() / 2;

    int curColumnIndex = curIndex / rows;
    QPointF start;
    if (curColumnIndex <  2) {
        start = QPointF(0., 0.);
    } else if (curColumnIndex >= columns - 2) {
        start = QPointF(slotWidth * columns - pageSize().width(), 0.);
    } else {
        start = QPointF(slotWidth * curColumnIndex, 0.);
    }

    QVector<WindowItem*> takenSlots;
    takenSlots.resize(rows*columns);
    takenSlots.fill(0);

    int i = 0;
    for (int x = 0 ; x < columns; ++x) {
        for (int y = 0; y < rows; ++y)  {
            takenSlots[y + x*rows] = items[i].value();
            if (++i > items.size() - 1)  {
                goto breakLoop;
            }
        }
    }

breakLoop:
    for (int slot = 0; slot < columns*rows; ++slot) {
        WindowItem *item = takenSlots[slot];
        if (!item) // some slots might be empty
            continue;
        EffectWindow *w = item->w;

        // Work out where the slot is
        QRect target(_pageSize.width() - (slot / rows + 1) * slotWidth + start.x(),
                    area.y() + (slot % rows) * slotHeight + start.y(),
                    slotWidth, slotHeight);
        target.adjust(_hM/4, _vM/4, -_hM/4, -_vM/4);   // Borders

        double scale;
        if (target.width() / double(w->width()) < target.height() / double(w->height())) {
            // Center vertically
            scale = target.width() / double(w->width());
            target.moveTop(target.top() + (target.height() - int(w->height() * scale)) / 2);
        } else {
            // Center horizontally
            scale = target.height() / double(w->height());
            target.moveLeft(target.left() + (target.width() - int(w->width() * scale)) / 2);
        }
//        // Don't scale the windows too much
//        if (scale > 2.0 || (scale > 1.0 && (w->width() > 300 || w->height() > 300))) {
//            scale = (w->width() > 300 || w->height() > 300) ? 1.0 : 2.0;
//            target = QRect(
//                        target.center().x() - int(w->width() * scale) / 2,
//                        target.center().y() - int(w->height() * scale) / 2,
//                        scale * w->width(), scale * w->height());
//        }

        bool shownNow = QRectF(item->_curPos, QSizeF(item->w->size().width() * item->_curScale.width(), item->w->size().height() * item->_curScale.height())).intersects(area);
        item->_destScale = QSizeF(scale, scale);
        if (shownNow) {
            item->_srcScale = item->_curScale;
            item->_isScaleAnimating = true;
        } else {
            item->_srcScale = item->_curScale = item->_destScale;
        }

        item->_destPos = target.topLeft();
        bool toShow = QRectF(item->_destPos, QSizeF(item->w->size().width() * item->_curScale.width(), item->w->size().height() * item->_curScale.height())).intersects(area);
        if (shownNow || toShow) {
            item->_oriPos = item->_destPos = target.topLeft();
            item->_srcPos = item->_curPos;
            item->_isPosAnimating = true;
        } else {
            item->_srcPos = item->_curPos = item->_oriPos = item->_destPos;
        }
    }


    _headPos = QPointF(pageSize().width() - slotWidth, items.first().value()->_destPos.y());
    _tailPos = items.last().value()->_destPos;
    _elementSize = QSizeF(slotWidth, slotHeight);
    _bigThenOnPage = ceil(_items.size() / double(ROW_COUNT)) * _elementSize.width() > _pageSize.width();
}

bool TaskList::boundCheck()
{
    bool result = true;
    if (_itemsIndex.size() > 0) {
        auto headItem = _itemsIndex.first().value();
        auto endItem = _itemsIndex.last().value();
        if (_orientation == Horizontal) {
            if (_layoutDirection == LeftToRight) {
                int distance = headItem->_curPos.x() - _headPos.x();
                if (distance > 0) {
                    rebound(QSize(distance, 0));
                    result = false;
                } else if (endItem->_curPos.x() +  _elementSize.width() < pageSize().width()) {
                    if (_bigThenOnPage) {
                        rebound(QSize(pageSize().width() - (endItem->_curPos.x() +  _elementSize.width()), 0));
                    } else {
                        rebound(QSize(distance, 0));
                    }
                    result = false;
                }
            } else {
                int distance = _headPos.x() - headItem->_curPos.x();
                if (distance > 0) {
                    rebound(QSize(distance, 0));
                    result = false;
                } else if (endItem->_curPos.x() > _hM / 2) {
                    if (_bigThenOnPage) {
                        rebound(QSize( _hM / 2 - endItem->_curPos.x(), 0));
                    } else {
                        rebound(QSize(distance,  _hM / 2));
                    }
                    result = false;
                }
            }
        } else {
            if (_verticalLayoutDirection == TopToBottom) {
                int distance = headItem->_curPos.y() - _headPos.y();
                if (distance > 0) {
                    rebound(QSize(distance, 0));
                    result = false;
                } else if (endItem->_curPos.y() +  _elementSize.height() <  pageSize().height()) {
                    if (_bigThenOnPage) {
                        rebound(QSize(0,  pageSize().height() - (endItem->_curPos.y() +  _elementSize.height())));
                    } else {
                        rebound(QSize(0, distance));
                    }
                    result = false;
                }
            } else {
                int distance = _headPos.y() - headItem->_curPos.y();
                if (distance > 0) {
                    rebound(QSize(distance, 0));
                    result = false;
                } else if (endItem->_curPos.y() > 0) {
                    if (_bigThenOnPage) {
                        rebound(QSize(0, 0 - endItem->_curPos.y()));
                    } else {
                        rebound(QSize(0, distance));
                    }
                    result = false;
                }
            }
        }
    }

    return result;
}

void TaskList::rebound(QSize size)
{
    _isSliding = false;
    _speadLine.stop();
    translateItemsOnDirection(size.width(), size.height(), true);
    resetAnimate(350);
}

void TaskList::resetSpeadLine(int time)
{
    _isSliding = true;
    _speadLine.setDuration(std::chrono::milliseconds(static_cast<int>(Effect::animationTime(time))));
    _speadLine.reset();
}

void TaskList::setupWindowItems(EffectWindowList windowlist, int screen, EffectWindow *curWindow, bool animation)
{
    _pageSize = effects->screenSize(screen);

    int lIndex = windowlist.indexOf(curWindow);

    int start =  0;
    _curPageIndex = 0;
    if (curWindow == nullptr || curWindow->isDesktop()) {
        _curPageIndex = -1;
        start = 0;
    }

    QRect area = effects->clientArea(ScreenArea, screen, effects->currentDesktop());
    if (_showPanel)   // reserve space for the panel
        area = effects->clientArea(MaximizeArea, screen, effects->currentDesktop());

    for (int i = 0; i < windowlist.size() ; i++) {
        WindowItem *item = new WindowItem;
        item->w = windowlist[i];
        if (_orientation == Horizontal) {
            if (_layoutDirection == LeftToRight) {
                item->_destPos = QPointF((pageSize().width()  + MARGIN) * (i + start - lIndex), item->w->y());
            } else {
                item->_destPos = QPointF(0 - (pageSize().width()  + MARGIN)* (i + start - lIndex), item->w->y());
            }
        } else {
            if (_verticalLayoutDirection == TopToBottom) {
                item->_destPos = QPointF(0,  (pageHeight(item->w) + MARGIN) * (i + start - lIndex));
            } else {
                item->_destPos =  QPointF(0, 0 - (pageHeight(item->w) + MARGIN) * (i + start - lIndex));
            }
        }

        if (animation) {
            item->_srcPos = item->_curPos;
            item->_oriPos = item->_destPos;
            item->_isScaleAnimating = true;
        } else {
            item->_curPos = item->_srcPos = item->_oriPos = item->_destPos;
        }

        item->_oriScale = item->_srcScale = item->_curScale = QSizeF(item->w->appScale(), item->w->appScale());

        _itemsIndex.append(_items.insert(windowlist[i], item));
    }

    if (animation) {
        resetAnimate(120);
    }

    _pageCount = _itemsIndex.size();
    _hasSetup = true;
}

QHash<EffectWindow *, WindowItem *> TaskList::toGrideModel(EffectWindow* curWindow)
{
    _resetingGride = true;
    int index = 0;
    if (curWindow != nullptr && _items.contains(curWindow)) {
        index = _itemsIndex.indexOf(_items.find(curWindow));
    }

    makeupGride(_itemsIndex, 0, index);

    resetAnimate(550);

    return _items;
}

QHash<EffectWindow *, WindowItem *> TaskList::getItems()
{
    return _items;
}

void TaskList::clear()
{
    _itemsIndex.clear();
    for (auto it = _items.begin(); it != _items.end(); it++) {
        delete it.value();
    }
    _items.clear();
    _curPageIndex = 0;
    _pageCount = 0;
    _hasSetup = false;
    _isAnimating = false;
}

bool TaskList::isManageWindow(EffectWindow *w)
{
    return _items.contains(w);
}

bool TaskList::isRemoving(EffectWindow *w)
{
    return _removeList.contains(w);
}

}
