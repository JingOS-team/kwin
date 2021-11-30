/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef FLOATBALL_H
#define FLOATBALL_H

#include <QRect>
#include <QPointF>
#include <kwineffects.h>
#include "kwineffectquickview.h"

class QTimer;
class QDBusInterface;
namespace KWin
{
class TabletEvent;
class FloatBall : public Effect
{
    Q_OBJECT
public:
    FloatBall();
    virtual ~FloatBall();

    void postPaintScreen() override;
    bool isActive() const override;

    static bool supported();
    static bool enabledByDefault();

    bool pointerEvent(QMouseEvent *e) override;
    bool tabletToolEvent(TabletEvent *e) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

private:
    void moveBall(const QPointF &delta);
    void showInput(bool show);
private:
    QTimer *_timer;
    bool _waitTrigger = false;
    QPointF _lastTouchPos;
    bool _isTouchDown = false;
    QRectF _ballGeometry;
    QDBusInterface *_sogoServiceInterface;
    QScopedPointer<GLTexture> _closeTexture;
    bool _hasInputFocus = false;
};

}

#endif // FLOATBALL_H
