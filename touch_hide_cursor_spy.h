/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2018 Martin Flöser <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once
#include "input_event_spy.h"
#include <QObject>
class QTimer;
namespace KWin
{

class TouchHideCursorSpy : public QObject,  public InputEventSpy
{
    Q_OBJECT
public:
    TouchHideCursorSpy();
    void pointerEvent(KWin::MouseEvent *event) override;
    void wheelEvent(KWin::WheelEvent *event) override;
    void touchDown(qint32 id, const QPointF &pos, quint32 time) override;

private:
    void showCursor();
    void hideCursor();

    QTimer *m_hideCursorTimer = nullptr;
    bool m_cursorHidden = false;
};

}
