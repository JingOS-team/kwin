/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 yongrui li <liyongrui@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SHOWWINDOWINFO_H
#define SHOWWINDOWINFO_H

#include <kwineffects.h>
#include "kwineffectquickview.h"
#include <string>
#include "stdio.h"
namespace KWin
{

class ShowWindowInfo : public Effect
{
    Q_OBJECT
public:
    enum Status {END = 0, START, FRAME, TIME};
    ShowWindowInfo();
    ~ShowWindowInfo(){};
    bool isActive() const override;
    void paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data) override;
    void postPaintScreen() override; 
    void prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void postPaintWindow(EffectWindow* w) override;
private slots:
    void slotChangeState(int mode, int n);
    

private:
    Status m_status = END;
    std::string m_filename; 
    FILE* m_f = nullptr;
    int m_nums = 0;    // recorde frames number start from 0
    int m_time_or_nums = 0;   // record time length or frames num
    int m_layer = 1;
    long m_start_time = 0;     // record start time
    int m_start_frame = 0;     // recorded frame numbers
};

}

#endif // SHOWWINDOWINFO_H
