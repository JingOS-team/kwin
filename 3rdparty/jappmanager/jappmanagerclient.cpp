/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "jappmanagerclient.h"

#include <condition_variable>
#include <thread>
#include <jappmanager.h>
#include <QDebug>

QList<QString> g_tasks;
std::condition_variable g_conditionVariable;
std::mutex g_quitTaskMutex;

namespace KWin
{
JappManagerClient::JappManagerClient(QObject *parent)
    : QObject(parent)
    , m_jingAppManager(new JAppManager)
{
    auto quitThread = std::thread([this]() {
        while (true) {
            std::unique_lock<std::mutex> lg(g_quitTaskMutex);
            g_conditionVariable.wait(lg, [=] ()->bool {return g_tasks.size() > 0;});
            foreach(QString uuid, g_tasks) {
                qDebug()<<"JappManagerClient <quitByWUuid "<<uuid;
                m_jingAppManager->quitByWUuid(uuid);
            }
            g_tasks.clear();
        }
    });
    quitThread.detach();
}

JappManagerClient::~JappManagerClient()
{

}

void JappManagerClient::pauseByWUuid(const QString &uuid)
{
    m_jingAppManager->pauseByWUuid(uuid);
}

void JappManagerClient::resumeByWUuid(const QString &uuid)
{
    m_jingAppManager->resumeByWUuid(uuid);
}

void JappManagerClient::quitAppByWUuID(const QString &uuid)
{
    qDebug()<<"JappManagerClient:"<<Q_FUNC_INFO;
    std::lock_guard<std::mutex> lg(g_quitTaskMutex);
    g_tasks.append(uuid);
    g_conditionVariable.notify_all();
}

void JappManagerClient::quitAppsByWUuID(const QList<QString> &uuids)
{
    qDebug()<<"JappManagerClient:"<<Q_FUNC_INFO;
    std::lock_guard<std::mutex> lg(g_quitTaskMutex);
    g_tasks.append(uuids);
    g_conditionVariable.notify_all();
}

}
