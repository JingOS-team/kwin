/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_JAPPMANAGERCLIENT_H
#define KWIN_JAPPMANAGERCLIENT_H

#include <QObject>
#include <QList>
#include <QString>

class JAppManager;
namespace KWin
{
class Toplevel;

class JappManagerClient : public QObject
{
    Q_OBJECT
public:
    explicit JappManagerClient(QObject *parent = nullptr);
    virtual ~JappManagerClient();

    void pauseByWUuid(const QString& uuid);
    void resumeByWUuid(const QString& uuid);

    void quitAppByWUuID(const QString &uuid);
    void quitAppsByWUuID(const QList<QString> &uuids);

private:
    QList<QString> m_closeTasks;
    JAppManager *m_jingAppManager;
};
}

#endif
