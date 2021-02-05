/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SHOWSYSINFO_H
#define SHOWSYSINFO_H

#include <kwineffects.h>
#include "kwineffectquickview.h"
namespace KWin
{

class ShowSysInfo : public Effect
{
    Q_OBJECT
public:
    ShowSysInfo();
    void postPaintScreen() override;
    bool isActive() const override;

    static bool supported();

private:
    EffectQuickScene *_noticeView;
};

}

#endif // SHOWSYSINFO_H
