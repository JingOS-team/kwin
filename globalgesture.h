/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GLOBALGESTURE_H
#define GLOBALGESTURE_H
#include <QThread>
#include <QDataStream>
#include <QLocalSocket>
#include <QSizeF>
class QTimer;

namespace KWin {
class GlobalGesture : public QObject
{
    Q_OBJECT
public:
    explicit GlobalGesture(QObject *parent = nullptr);
    static GlobalGesture* self() {
        return _self;
    }

    void pinchGestureBegin(int fingerCount, quint32 time);
    void pinchGestureUpdate(qreal scale, qreal angleDelta, const QSizeF &delta, quint32 time);
    void pinchGestureEnd(quint32 time);
    void pinchGestureCancelled(quint32 time);

    void swipeGestureBegin(int fingerCount, quint32 time, bool isTouch = false);
    void swipeGestureUpdate(const QSizeF &delta, quint32 time, bool isTouch = false);
    void swipeGestureEnd(quint32 time, bool isTouch = false);
    void swipeGestureCancelled(quint32 time, bool isTouch = false);

    void init(int pid);
private:
    void minimumWindow();

private:
    QSizeF _lastSpead = QSizeF(0., 0.);
    static GlobalGesture* _self;
    QSizeF _lastSwipDelta;
    bool _swipeStarted = false;
    quint32 _swipeTime = 0;
    QSizeF _swipeDelta;
};
}

#endif // GLOBALGESTURE_H
