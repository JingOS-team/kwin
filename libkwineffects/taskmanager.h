/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef TASKMANAGER_H
#define TASKMANAGER_H
#include <QObject>
#include <kwineffects_export.h>

namespace KWin {

class KWINEFFECTS_EXPORT TaskManager : public QObject
{
    Q_OBJECT
public:
    explicit TaskManager(QObject *parent = nullptr);
    virtual ~TaskManager();

    enum TaskState {
        TS_None,
        TS_Prepare,
        TS_Move,
        TS_MoveEnd,
        TS_ToDesktop,
        TS_ToWindow,
        TS_Swip,
        TS_TaskAnimating,
        TS_Task
    };

    void onGestueEnd(const QSizeF &delta, const QSizeF &spead);
    void onTaskSwipe(bool toRight);

    void setTaskState(TaskState ts, bool broadCast = true);

    TaskState getTaskState() {
        return _ts;
    }

    void updateMove(const QSizeF& delta, qreal progress) {
        emit move(delta, progress);
    }

    bool isIsNoneState() {
        return getTaskState() == TS_None;
    }

    bool isInitState() {
        return getTaskState() == TS_None || getTaskState() ==  TS_Prepare;
    }
Q_SIGNALS:
    void taskStateChanged(TaskState ts, TaskState oldState);
    void move(const QSizeF& delta, qreal progress);
    void switchWidnow(bool toRight);
    void gestueEnd(const QSizeF &delta, const QSizeF &spaead);

private:
    TaskState _ts = TS_None;
};

extern KWINEFFECTS_EXPORT TaskManager* taskManager;
}
#endif // TASKMANAGER_H
