/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef GLOBALGESTURETYPES_H
#define GLOBALGESTURETYPES_H
#include <QtGlobal>
#include <QString>

const QString GESTURE_SERVER_NAME = "%1";
const quint8 GESTURE_SWIPE_BEGIN = 1;
const quint8 GESTURE_SWIPE_UPDATE = 2;
const quint8 GESTURE_SWIPE_END = 3;
const quint8 GESTURE_SWIPE_CANCELL = 4;

const quint8 GESTURE_PINCH_BEGIN = 5;
const quint8 GESTURE_PINCH_UPDATE = 6;
const quint8 GESTURE_PINCH_END = 7;
const quint8 GESTURE_PINCH_CANCELL = 8;

#endif // GLOBALGESTURETYPES_H
