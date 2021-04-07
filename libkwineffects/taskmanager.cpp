/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "taskmanager.h"

namespace KWin
{
TaskManager* taskManager = nullptr;
TaskManager::TaskManager(QObject *parent)
{
    KWin::taskManager = this;
}

TaskManager::~TaskManager()
{
    KWin::taskManager = nullptr;
}

void TaskManager::onGestueEnd(const QSizeF &delta, const QSizeF &spead)
{
    emit gestueEnd(delta, spead);
}

void TaskManager::onTaskSwipe(bool toRight)
{
    emit switchWidnow(toRight);
}

void TaskManager::setTaskState(TaskState ts, bool broadCast)
{
    if (_ts != ts) {
        if (ts == TS_Prepare && _ts != TS_None) {
            return;
        }
        TaskState oldState = _ts;
        _ts = ts;
        if (broadCast) {
            emit taskStateChanged(ts, oldState);
        }
    }
}
}
