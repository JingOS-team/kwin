/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 yongrui li <liyongrui@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "globalpinchgestures.h"
#include "effects.h"
#include "gestures.h"

namespace KWin {

GlobalPinchGestures::GlobalPinchGestures(QObject *parent)
    : QObject(parent)
    , m_5FingersPinchGesture(new PinchGesture(this))
{
}

void GlobalPinchGestures::registerGesture(GestureRecognizer *recongnizer)
{
    if (nullptr == recongnizer) {
        return;
    }
    recongnizer->registerPinchGesture(m_5FingersPinchGesture);
    init5FingersPinchGesture();
}

void GlobalPinchGestures::unregisterGesture(GestureRecognizer *recongnizer)    
{
    if (nullptr == recongnizer) {
        return;
    }
    recongnizer->unregisterPinchGesture(m_5FingersPinchGesture);
}

void GlobalPinchGestures::init5FingersPinchGesture()
{
    m_5FingersPinchGesture->setMinimumFingerCount(5);
    m_5FingersPinchGesture->setMaximumFingerCount(5);
    m_5FingersPinchGesture->setFingerDistanceIncrement(800);

    connect(m_5FingersPinchGesture, &Gesture::cancelled, this, &GlobalPinchGestures::on5FingersPinchCancelled, Qt::QueuedConnection);
    connect(m_5FingersPinchGesture, &Gesture::triggered, this, &GlobalPinchGestures::on5FingersPinchTriggered, Qt::QueuedConnection);
}

void GlobalPinchGestures::on5FingersPinchTriggered(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time);
    Q_UNUSED(lastSpeed);
    showTaskPanel();
}

void GlobalPinchGestures::on5FingersPinchCancelled(quint32 time, const qreal &lastSpeed)
{
    Q_UNUSED(time);
    Q_UNUSED(lastSpeed);
    m_5FingersPinchGesture->resetStartDistance();
}

void GlobalPinchGestures::showTaskPanel() 
{
    effects->toTriggerTask();
}

} // namespace