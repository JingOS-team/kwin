/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Thomas Perl <thomas.perl@jolla.com>
**
** Based on hwcomposer_screeninfo.cpp
**
** Copyright (C) 2016 Michael Serpieri <mickybart@pygoscelis.org>
**
** This file is part of the surfaceflinger plugin.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "surfaceflinger_screeninfo.h"

#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>

#include <hybris/surface_flinger/surface_flinger_compatibility_layer.h>

//TODO:define below should be replaced by: #include <private/qmath_p.h>
#define Q_MM_PER_INCH 25.4f

namespace KWin
{

const char *ENV_VAR_EGLFS_PHYSICAL_WIDTH = "QT_QPA_EGLFS_PHYSICAL_WIDTH";
const char *ENV_VAR_EGLFS_PHYSICAL_HEIGHT = "QT_QPA_EGLFS_PHYSICAL_HEIGHT";
const char *ENV_VAR_EGLFS_WIDTH = "QT_QPA_EGLFS_WIDTH";
const char *ENV_VAR_EGLFS_HEIGHT = "QT_QPA_EGLFS_HEIGHT";
const char *ENV_VAR_EGLFS_DEPTH = "QT_QPA_EGLFS_DEPTH";
const char *ENV_VAR_EGLFS_REFRESH_RATE = "QT_QPA_EGLFS_REFRESH_RATE";

// Fallback source: Use default values and print a warning

class SurfaceFlingerScreenInfoFallbackSource {
public:
    QSizeF physicalScreenSize(QSize screenSize)
    {
        const int DEFAULT_PHYSICAL_DPI = 100;

        qWarning("EGLFS: Cannot determine physical screen size, assuming %d DPI",
                DEFAULT_PHYSICAL_DPI);
        qWarning("EGLFS: To override, set %s and %s (in mm)",
                ENV_VAR_EGLFS_PHYSICAL_WIDTH, ENV_VAR_EGLFS_PHYSICAL_HEIGHT);

        return QSizeF(screenSize.width() * Q_MM_PER_INCH / DEFAULT_PHYSICAL_DPI,
                      screenSize.height() * Q_MM_PER_INCH / DEFAULT_PHYSICAL_DPI);
    }

    QSize screenSize()
    {
        const int DEFAULT_WIDTH = 800;
        const int DEFAULT_HEIGHT = 600;

        qWarning("EGLFS: Cannot determine screen size, falling back to %dx%d",
                DEFAULT_WIDTH, DEFAULT_HEIGHT);
        qWarning("EGLFS: To override, set %s and %s (in pixels)",
                ENV_VAR_EGLFS_WIDTH, ENV_VAR_EGLFS_HEIGHT);

        return QSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    }

    int screenDepth()
    {
        const int DEFAULT_DEPTH = 32;

        qWarning("EGLFS: Cannot determine screen depth, falling back to %d",
                DEFAULT_DEPTH);
        qWarning("EGLFS: To override, set %s",
                ENV_VAR_EGLFS_DEPTH);

        return DEFAULT_DEPTH;
    }
    
    float refreshRate()
    {
        const float DEFAULT_REFRESH_RATE = 60.0;
        
        qWarning("EGLFS: Cannot determine screen refresh rate, falling back to %f",
                DEFAULT_REFRESH_RATE);
        qWarning("EGLFS: To override, set %s",
                ENV_VAR_EGLFS_REFRESH_RATE);
        
        return DEFAULT_REFRESH_RATE;
    }
};

// Environment variable source: Use override values from env vars

class SurfaceFlingerScreenInfoAndroidSource {
public:
    SurfaceFlingerScreenInfoAndroidSource(size_t display_id)
    {
        m_width = 0;
        m_height = 0;
        m_physicalWidth = 0;
        m_physicalHeight = 0;
        m_refresh_rate = 0.0f;
        
        struct SfDisplayInfo display_info;
        if (sf_get_display_info(display_id, &display_info) == 0) {
            m_width = display_info.w;
            m_height = display_info.h;
            m_physicalWidth = m_width * Q_MM_PER_INCH / display_info.xdpi / display_info.density;
            m_physicalHeight = m_height * Q_MM_PER_INCH / display_info.ydpi / display_info.density;
            m_refresh_rate = display_info.fps;
        }
    }

    bool hasPhysicalScreenSize()
    {
        return ((m_physicalWidth != 0) && (m_physicalHeight != 0));
    }

    QSizeF physicalScreenSize()
    {
        return QSizeF(m_physicalWidth, m_physicalHeight);
    }

    bool hasScreenSize()
    {
        return ((m_width != 0 && m_height != 0));
    }

    QSize screenSize()
    {
        return QSize(m_width, m_height);
    }

    bool hasScreenDepth()
    {
        return (m_depth != 0);
    }

    int screenDepth()
    {
        return m_depth;
    }
    
    bool hasRefreshRate()
    {
        return (m_refresh_rate != 0.0f);
    }
    
    float refreshRate()
    {
        return m_refresh_rate;
    }

private:
    int m_physicalWidth;
    int m_physicalHeight;
    int m_width;
    int m_height;
    float m_refresh_rate;
    int m_depth;
};


// Environment variable source: Use override values from env vars

class SurfaceFlingerScreenInfoEnvironmentSource {
public:
    SurfaceFlingerScreenInfoEnvironmentSource()
        : m_physicalWidth(qgetenv(ENV_VAR_EGLFS_PHYSICAL_WIDTH).toInt())
        , m_physicalHeight(qgetenv(ENV_VAR_EGLFS_PHYSICAL_HEIGHT).toInt())
        , m_width(qgetenv(ENV_VAR_EGLFS_WIDTH).toInt())
        , m_height(qgetenv(ENV_VAR_EGLFS_HEIGHT).toInt())
        , m_depth(qgetenv(ENV_VAR_EGLFS_DEPTH).toInt())
        , m_refresh_rate(qgetenv(ENV_VAR_EGLFS_REFRESH_RATE).toFloat())
    {
    }

    bool hasPhysicalScreenSize()
    {
        return ((m_physicalWidth != 0) && (m_physicalHeight != 0));
    }

    QSizeF physicalScreenSize()
    {
        return QSizeF(m_physicalWidth, m_physicalHeight);
    }

    bool hasScreenSize()
    {
        return ((m_width != 0 && m_height != 0));
    }

    QSize screenSize()
    {
        return QSize(m_width, m_height);
    }

    bool hasScreenDepth()
    {
        return (m_depth != 0);
    }

    int screenDepth()
    {
        return m_depth;
    }
    
    bool hasRefreshRate()
    {
        return (m_refresh_rate != 0.0f);
    }
    
    float refreshRate()
    {
        return m_refresh_rate;
    }

private:
    int m_physicalWidth;
    int m_physicalHeight;
    int m_width;
    int m_height;
    int m_depth;
    float m_refresh_rate;
};

SurfaceFlingerScreenInfo::SurfaceFlingerScreenInfo(size_t display_id)
{
    /**
     * Look up the values in the following order of preference:
     *
     *  1. Environment variables can override everything
     *  2. Android DisplayInfo
     *  3. Fallback values (with warnings) if 1. and 2. fail
     **/
    SurfaceFlingerScreenInfoEnvironmentSource envSource;
    SurfaceFlingerScreenInfoAndroidSource androidSource(display_id);
    SurfaceFlingerScreenInfoFallbackSource fallbackSource;

    if (envSource.hasScreenSize()) {
        m_screenSize = envSource.screenSize();
    } else if (androidSource.hasScreenSize()) {
        m_screenSize = androidSource.screenSize();
    } else {
        m_screenSize = fallbackSource.screenSize();
    }

    if (envSource.hasPhysicalScreenSize()) {
        m_physicalScreenSize = envSource.physicalScreenSize();
    } else if (androidSource.hasPhysicalScreenSize()) {
        m_physicalScreenSize = androidSource.physicalScreenSize();
    } else {
        m_physicalScreenSize = fallbackSource.physicalScreenSize(m_screenSize);
    }

    if (envSource.hasScreenDepth()) {
        m_screenDepth = envSource.screenDepth();
    } else if (androidSource.hasScreenDepth()) {
        m_screenDepth = androidSource.screenDepth();
    } else {
        m_screenDepth = fallbackSource.screenDepth();
    }
    
    if (envSource.hasRefreshRate()) {
        m_refreshRate = envSource.refreshRate();
    } else if (androidSource.hasRefreshRate()) {
        m_refreshRate = androidSource.refreshRate();
    } else {
        m_refreshRate = fallbackSource.refreshRate();
    }

    qDebug() << "EGLFS: Screen Info";
    qDebug() << " - Physical size:" << m_physicalScreenSize;
    qDebug() << " - Screen size:" << m_screenSize;
    qDebug() << " - Screen depth:" << m_screenDepth;
    qDebug() << " - Refresh rate:" << m_refreshRate;
}

}
