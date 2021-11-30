/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2016 Michael Serpieri <mickybart@pygoscelis.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_SURFACEFLINGER_BACKEND_H
#define KWIN_SURFACEFLINGER_BACKEND_H
#include "platform.h"
#include "input.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>

// hybris/android
#include <android-config.h>
#include <hybris/surface_flinger/surface_flinger_compatibility_layer.h>
#include <hardware/hwcomposer_defs.h>
// needed as hwcomposer_window.h includes EGL which on non-arm includes Xlib
#include <fixx11h.h>

#include "abstract_wayland_output.h"
struct light_device_t;

// Evaluate "x", if it doesn't return zero, print a warning
#define SF_PLUGIN_EXPECT_ZERO(x) \
    { int res; if ((res = (x)) != 0) \
        qWarning("QPA-SF: %s in %s returned %i", (#x), __func__, res); }

// Evaluate "x", if it isn't NULL, print a warning
#define SF_PLUGIN_EXPECT_NULL(x) \
    { void *res; if ((res = (x)) != NULL) \
        qWarning("QPA-SF: %s in %s returned %x", (#x), __func__, (unsigned int)res); }

// Evaluate "x", if it is NULL, exit with a fatal error
#define SF_PLUGIN_FATAL(x) \
    qFatal("QPA-SF: %s in %s", x, __func__)

// Evaluate "x", if it is NULL, exit with a fatal error
#define SF_PLUGIN_ASSERT_NOT_NULL(x) \
    { void *res; if ((res = (x)) == NULL) \
        qFatal("QPA-SF: %s in %s returned %lX", (#x), __func__, (unsigned long)res); }

// Evaluate "x", if it doesn't return zero, exit with a fatal error
#define SF_PLUGIN_ASSERT_ZERO(x) \
    { int res; if ((res = (x)) != 0) \
        qFatal("QPA-SF: %s in %s returned %i", (#x), __func__, res); }

namespace KWin
{

class BacklightInputEventFilter;
class SurfaceFlingerOutput;

class SurfaceFlingerBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "surfaceflinger.json")
public:
    explicit SurfaceFlingerBackend(QObject *parent = nullptr);
    virtual ~SurfaceFlingerBackend();

    void init() override;
    Screens *createScreens(QObject *parent = nullptr) override;
    OpenGLBackend *createOpenGLBackend() override;

    QSize screenSize() const override ;
    virtual QVector<CompositingType> supportedCompositors() const override;
    Outputs outputs() const override;
    Outputs enabledOutputs() const override;

    EGLDisplay display();
    EGLSurface createEglSurface();
    EGLSurface eglSurface();
    void makeCurrent();
    EGLConfig config();
    EGLContext context();
    QSize size() const ;
    bool  isBacklightOff() const ;

Q_SIGNALS:
    void outputBlankChanged();

private Q_SLOTS:
    void toggleBlankOutput();
    void screenBrightnessChanged(int brightness) {
        m_oldScreenBrightness = brightness;
    }

private:
    void initLights();
    void toggleScreenBrightness();
    QSize m_displaySize;
    struct SfClient *m_sf_client = nullptr;
    struct SfSurface *m_sf_surface = nullptr;
    light_device_t *m_lights = nullptr;
    bool m_outputBlank = true;
    int m_oldScreenBrightness = 0x7f;
    QScopedPointer<BacklightInputEventFilter> m_filter;
    QScopedPointer<SurfaceFlingerOutput> m_output;
};

class BacklightInputEventFilter : public InputEventFilter
{
public:
    BacklightInputEventFilter(SurfaceFlingerBackend *backend);
    virtual ~BacklightInputEventFilter();

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;

private:
    void toggleBacklight();
    SurfaceFlingerBackend *m_backend;
    QElapsedTimer m_doubleTapTimer;
    QVector<qint32> m_touchPoints;
    bool m_secondTap = false;
};


class SurfaceFlingerOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:
    SurfaceFlingerOutput(QObject *parent);
    ~SurfaceFlingerOutput() override;
    bool isValid() const;

    void updateDpms(KWaylandServer::OutputInterface::DpmsMode mode) override;

    // QSize pixelSize() const override;
    // qreal scale() const override;
    // QRect geometry() const override;
    // QSize physicalSize() const override;
    // int refreshRate() const override;
Q_SIGNALS:
    void dpmsModeRequested(KWaylandServer::OutputInterface::DpmsMode mode);
private:
    QSize m_pixelSize;
};

}

#endif
