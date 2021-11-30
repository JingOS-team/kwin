/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef KWIN_HWCOMPOSER_BACKEND_H
#define KWIN_HWCOMPOSER_BACKEND_H
#include "platform.h"
#include "abstract_wayland_output.h"
#include "input.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QSemaphore>
#include <QFile>
#include <android-config.h>
// libhybris
#include <hardware/hwcomposer.h>
#include <hwcomposer_window.h>
#include <hybris/hwc2/hwc2_compatibility_layer.h>
#include <hybris/hwcomposerwindow/hwcomposer.h>
#include <QBasicTimer>

// needed as hwcomposer_window.h includes EGL which on non-arm includes Xlib
#include <fixx11h.h>
#include "renderloop.h"

typedef struct hwc_display_contents_1 hwc_display_contents_1_t;
typedef struct hwc_layer_1 hwc_layer_1_t;
typedef struct hwc_composer_device_1 hwc_composer_device_1_t;
struct light_device_t;

class HWComposerNativeWindowBuffer;

namespace KWin
{

class HwcomposerWindow;
class BacklightInputEventFilter;

// lijing android compatible
class AndroidAssistance;

class HwcomposerOutput : public AbstractWaylandOutput
{
    Q_OBJECT
public:
#if defined(HWC_DEVICE_API_VERSION_2_0)
    HwcomposerOutput(uint32_t hwcVersion, hwc_composer_device_1_t *device, hwc2_compat_display_t* hwc2_primary_display);
#else
    HwcomposerOutput(uint32_t hwcVersion, hwc_composer_device_1_t *device, void* hwc2_primary_display);
#endif
    ~HwcomposerOutput() override;

    void init();

    RenderLoop *renderLoop() const override;
    bool isValid() const;
    bool hardwareTransforms() const;
    void updateDpms(KWaylandServer::OutputInterface::DpmsMode mode) override;
    QSize pixelSize() const override;
    QRect geometry() const override;
    QSize physicalSize() const override;
Q_SIGNALS:
    void dpmsModeRequested(KWaylandServer::OutputInterface::DpmsMode mode);
private:
    QSize m_pixelSize;
    uint32_t m_hwcVersion;
    hwc_composer_device_1_t *m_device;
#if defined(HWC_DEVICE_API_VERSION_2_0)
    hwc2_compat_display_t *m_hwc2_primary_display;
#else
    void *m_hwc2_primary_display;
#endif
    friend class HwcomposerBackend;
    RenderLoop *m_renderLoop;
};

class HwcomposerBackend : public Platform
{
    Q_OBJECT
    Q_INTERFACES(KWin::Platform)
    Q_PLUGIN_METADATA(IID "org.kde.kwin.Platform" FILE "hwcomposer.json")
public:
    explicit HwcomposerBackend(QObject *parent = nullptr);
    virtual ~HwcomposerBackend();

    void init() override;
    OpenGLBackend *createOpenGLBackend() override;

    Outputs outputs() const override;
    Outputs enabledOutputs() const override;

    QSize size() const;
    QSize screenSize() const override;

    int scale() const;

    HwcomposerWindow *createSurface();

    hwc_composer_device_1_t *device() const {
        return m_device;
    }

    int deviceVersion() const {
        return m_hwcVersion;
    }

    void enableVSync(bool enable);
    void waitVSync();
    void wakeVSync();
    QVector<CompositingType> supportedCompositors() const override {
        return QVector<CompositingType>{OpenGLCompositing};
    }

#if defined(HWC_DEVICE_API_VERSION_2_0)
    hwc2_compat_device_t *hwc2_device() const {
        return m_hwc2device;
    }

    hwc2_compat_display_t *hwc2_display() const {
        return m_hwc2_primary_display;
    }
#endif

Q_SIGNALS:
    void outputBlankChanged();

public Q_SLOTS:
    void displayPowerStateChange(int status, int reason);
    void screenTurnOn(QString str);
    void screenTurnOff(QString str);

private Q_SLOTS:
    void compositing(int flags);
private:
    friend HwcomposerWindow;
    bool updateOutputs();
    void updateOutputsEnabled();
    void setPowerMode(bool enable);

protected:/*! ---cursor ----*/
    void doHideCursor() override;
    void doShowCursor() override;
    void doSetSoftwareCursor() override;

private:
    void updateCursor();
    void moveCursor();
    void initCursor();

private:
    hwc_composer_device_1_t *m_device = nullptr;
    int m_vsyncInterval = 16;
    uint32_t m_hwcVersion;
    bool m_hasVsync = false;
    QMutex m_vsyncMutex;
    QWaitCondition m_vsyncWaitCondition;
    QSemaphore     m_compositingSemaphore;
    QScopedPointer<BacklightInputEventFilter> m_filter;
    QScopedPointer<HwcomposerOutput> m_output;
    QScopedPointer<AndroidAssistance> m_android_assis;
    void RegisterCallbacks();

#if defined(HWC_DEVICE_API_VERSION_2_0)
    hwc2_compat_device_t *m_hwc2device = nullptr;
    hwc2_compat_display_t* m_hwc2_primary_display = nullptr;
#else
    void *m_hwc2_primary_display = nullptr;
#endif

public: //! debug
    void showFps();
    int  m_fps;
    bool m_bShowFps;
};

class HwcomposerWindow : public HWComposerNativeWindow
{
public:
    virtual ~HwcomposerWindow();
    void present(HWComposerNativeWindowBuffer *buffer) override;

private:
    friend HwcomposerBackend;
    HwcomposerWindow(HwcomposerBackend *backend);
    HwcomposerBackend *m_backend;
    hwc_display_contents_1_t **m_list;
    int lastPresentFence = -1;

#if defined(HWC_DEVICE_API_VERSION_2_0)
    hwc2_compat_display_t *m_hwc2_primary_display = nullptr;
#endif
};

class BacklightInputEventFilter : public QObject,  public InputEventFilter
{
public:
    BacklightInputEventFilter(HwcomposerBackend *backend);
    virtual ~BacklightInputEventFilter();

    bool pointerEvent(QMouseEvent *event, quint32 nativeButton) override;
    bool wheelEvent(QWheelEvent *event) override;
    bool keyEvent(QKeyEvent *event) override;
    bool touchDown(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchMotion(qint32 id, const QPointF &pos, quint32 time) override;
    bool touchUp(qint32 id, quint32 time) override;
    void timerEvent(QTimerEvent *) Q_DECL_OVERRIDE;
private:
    HwcomposerBackend *m_backend;
    QElapsedTimer m_doubleTapTimer;
    QVector<qint32> m_touchPoints;
    QBasicTimer m_cpuTimeout;
    QFile * cmd0File;
    QFile * cmd4File;
    bool m_secondTap = false;
};

}

#endif
