/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 yongrui li <liyongrui@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GLOBALPINCHGESTUREMGRS_H
#define GLOBALPINCHGESTUREMGRS_H

#include "gestures.h"
#include <QObject>

namespace KWin {
class PinchGesture;
class GestureRecognizer;

class GlobalPinchGestures : public QObject
{
    Q_OBJECT
public:
    explicit GlobalPinchGestures(QObject *parent = nullptr);

    void registerGesture(GestureRecognizer *recongnizer);
    void unregisterGesture(GestureRecognizer *recongnizer);

protected:
    void init5FingersPinchGesture();

private slots:
    void on5FingersPinchTriggered(quint32 time, const qreal &lastSpeed);
    void on5FingersPinchCancelled(quint32 time, const qreal &lastSpeed);

private:
    void showTaskPanel();
    PinchGesture *m_5FingersPinchGesture = nullptr;
    

}; // GlobalPinchGesture
} // namespace 

#endif // GLOBALPINCHGESTUREMGR_H