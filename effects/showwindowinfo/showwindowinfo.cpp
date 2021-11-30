/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 yongrui li <liyongrui@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "showwindowinfo.h"
#include <sys/time.h>

namespace KWin
{

ShowWindowInfo::ShowWindowInfo()
{
    connect(effects, &EffectsHandler::downloadWindowModeChange, this, &ShowWindowInfo::slotChangeState);
    m_filename = getenv("HOME");
    m_filename += "/showwindowinfo.log";
}

void ShowWindowInfo::postPaintScreen()
{
    m_layer = 1;
    m_nums++;
    if(m_status!=END && m_f) 
    {
        // fprintf(m_f, "\n");
        struct timeval time;
        gettimeofday(&time, NULL);
        fprintf(m_f, "--------------------%d frame---%ld time-----------------------", m_nums, time.tv_sec*1000 + time.tv_usec/1000);
        fprintf(m_f, "\n\n");
    }

    if(m_status == FRAME)
    {
        m_start_frame++;
        if(m_start_frame >= m_time_or_nums)
            slotChangeState(0, 0);
    }
    else if(m_status == TIME)
    {
        struct timeval time;
        gettimeofday(&time, NULL);
        if(time.tv_sec*1000 + time.tv_usec/1000 - m_start_time > m_time_or_nums)
            slotChangeState(0, 0);
    }
    effects->postPaintScreen();
}

void ShowWindowInfo::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    w->setShowIgnoreDisible(true);
    effects->prePaintWindow(w, data, presentTime);
}

void ShowWindowInfo::postPaintWindow(EffectWindow *w)
{
    w->setShowIgnoreDisible(false);
    effects->postPaintWindow(w);
}

void ShowWindowInfo::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if(w && m_status != END && m_f) 
    {
        int pid = w->pid();
        int x= w->x();
        int y = w->y();
        int width = w->width();
        int height = w->height();
        QString windowClass =  w->windowClass();
        QString windowRole = w->windowRole();
        int windowType = w->windowType();
        bool visible = w->isVisible();
        fprintf(m_f, "layer:%d, isvisible:%d, wtype:", m_layer, visible);
        m_layer++;
        switch (windowType)
        {
            case NET::Desktop:
                fprintf(m_f,"Desktop");
                break;
            case NET::Dock:
                fprintf(m_f,"Dock");
                break;
            case NET::Menu:
                fprintf(m_f,"Menu");
                break;
            case NET::Toolbar:
                fprintf(m_f,"Toolbar");
                break;
            case NET::Splash:
                fprintf(m_f,"Splash");
                break;
            case NET::Utility:
                fprintf(m_f,"Utility");
                break;
            case NET::Dialog:
                fprintf(m_f,"Dialog");
                break;
            case NET::Normal:
                fprintf(m_f,"Normal");
                break;
            case NET::DropdownMenu:
                fprintf(m_f,"DropdownMenu");
                break;
            case NET::PopupMenu:
                fprintf(m_f,"PopupMenu");
                break;
            case NET::Tooltip:
                fprintf(m_f,"Tooltip");
                break;
            case NET::Notification:
                fprintf(m_f,"Notification");
                break;
            case NET::CriticalNotification:
                fprintf(m_f,"CriticalNotification");
                break;
            case NET::OnScreenDisplay:
                fprintf(m_f,"OnScreenDisplay");
                break;
            case NET::ComboBox:
                fprintf(m_f,"ComboBox");
                break;
            case NET::DNDIcon:
                fprintf(m_f,"DNDIcon");
                break;
            default:
                fprintf(m_f,"unknown");
                break;
        }
        fprintf(m_f, ", pid:%d, wclass:%s, x:%d, y:%d, w:%d, h:%d jingType:%d jingLayer:%d\n",
                pid, windowClass.toStdString().data(), 
                x, y, width, height, w->jingWindowType(), w->jingLayer());
    }
    effects->paintWindow(w, mask, region, data);
}

void ShowWindowInfo::slotChangeState(int mode, int n){
    switch (Status(mode))
    {
        case END:
            if(m_f)
            {
                fprintf(m_f, "------------------------end recording--------------------");
                fprintf(m_f, "\n\n\n\n");
                fclose(m_f);
                m_f = nullptr;
            }
            m_time_or_nums = 0;
            m_start_time = 0;
            m_start_frame = 0;
            m_nums=0;
            m_status = END;
            break;
        case START:
            if(m_status==END)
            {
                m_f = fopen(m_filename.c_str(), "a+");
                if(m_f)
                {
        
                    fprintf(m_f, "------------------------star recording--------------------\n");
                    fprintf(m_f, "-------------------continue recording mode----------------\n");
                    // fprintf(m_f, "pid,windowClass,windowRole,x,y,width,height,time\n");
                }
            }
            else if(m_status==FRAME || m_status==TIME)
            {
                fprintf(m_f, "---------------turnto continue recording mode-------------\n");
            }
            m_status = START;
            break;
        case FRAME:
            if(m_status==END)
            {
                m_f = fopen(m_filename.c_str(), "a+");
                if(m_f)
                {  
                    fprintf(m_f, "------------------------star recording--------------------\n");
                    fprintf(m_f, "--------------------frame recording mode------------------\n");
                }
                m_time_or_nums = n;
                m_status = FRAME;
                m_start_frame= 0;
            }
            else if(m_status==FRAME)
            {
                m_time_or_nums += n;
                if(m_f) 
                    fprintf(m_f, "\n\nAdd more frame command\n\n");
            }
            else
            {
                if(m_f) 
                    fprintf(m_f, "\n\nError frame command\n\n");
            }
            break;
        case TIME:
            if(m_status==END)
            {
                m_f = fopen(m_filename.c_str(), "a+");
                if(m_f)
                {  
                    fprintf(m_f, "------------------------star recording--------------------\n");
                    fprintf(m_f, "--------------------time recording mode------------------\n");
                }
                m_status = TIME;
                m_time_or_nums = n;

                struct timeval time;
                gettimeofday(&time, NULL);
                m_start_time = time.tv_sec*1000 + time.tv_usec/1000;
            }
            else if(m_status==TIME)
            {
                m_time_or_nums += n;
                if(m_f) 
                    fprintf(m_f, "\n\nAdd more time command\n\n");
            }
            else
            {
                if(m_f) 
                    fprintf(m_f, "\n\nError time command\n\n");
            }
            break;
    }
}

bool ShowWindowInfo::isActive() const{
    return m_status != END; 
}

}
