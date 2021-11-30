/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "hwcomposer_backend.h"

#include "composite.h"
#include "cursor.h"
#include "egl_hwcomposer_backend.h"
#include "logging.h"
#include "main.h"
//#include "screens_hwcomposer.h"
#include "wayland_server.h"
// KWayland
#include <KWaylandServer/output_interface.h>
#include <KWaylandServer/seat_interface.h>
// KDE
#include <KConfigGroup>
// Qt
#include <QDBusConnection>
#include <QKeyEvent>
// hybris/android
#include <android-config.h>
#include <hardware/hardware.h>
#include <hardware/lights.h>
#include <hybris/hwc2/hwc2_compatibility_layer.h>
#include <hybris/hwcomposerwindow/hwcomposer.h>
// linux
#include <linux/input.h>
#include <sync/sync.h>

#include <QDBusError>
#include <QtConcurrent>
#include <QDBusMessage>
#include "renderloop_p.h"
#include "composite.h"
// based on test_hwcomposer.c from libhybris project (Apache 2 licensed)

// lijing android compatible
#include "AndroidAssistance.h"

using namespace KWaylandServer;

namespace KWin {

BacklightInputEventFilter::BacklightInputEventFilter(HwcomposerBackend *backend)
    : InputEventFilter(), m_backend(backend)
{
//        cmd4File = new QFile("/sys/devices/system/cpu/cpufreq/policy4/scaling_governor");
//        bool ret = cmd4File->open(QIODevice::WriteOnly);
//	qDebug()<<"BacklightInputEventFilter ~~~~~~~~~~~~~~~~~ ret: "<<ret;
//        cmd0File = new QFile("/sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
//        ret = cmd0File->open(QIODevice::WriteOnly);
//	qDebug()<<"BacklightInputEventFilter ~~~~~~~~~~~~~~~~~ ret: "<<ret;

}

BacklightInputEventFilter::~BacklightInputEventFilter() = default;

bool BacklightInputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    return false;
}

bool BacklightInputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    return false;
}

void BacklightInputEventFilter::timerEvent(QTimerEvent *e)
{
  //  if (e->timerId() == m_cpuTimeout.timerId()) {
  //     cmd0File->write("ondemand", strlen("ondemand"));
  //     cmd0File->flush();
  //     cmd4File->write("ondemand", strlen("ondemand"));
  //     cmd4File->flush();
  //     m_cpuTimeout.stop();
  //  }
}

bool BacklightInputEventFilter::keyEvent(QKeyEvent *event)
{
    //! [dba debug: 2021-06-23]  因上层监听了power按键，影响系统亮灭屏，将其拦截掉。
    if (event->key() == Qt::Key_PowerOff && event->type() == QEvent::KeyRelease) {
        return true;
    }

  //  if ( event->key() != Qt::Key_PowerOff &&
  //       event->type() == QEvent::KeyPress) {
  //      if (!m_cpuTimeout.isActive()) {
  //          cmd0File->write("performance", strlen("performance"));
  //          cmd0File->flush();
  //          cmd4File->write("performance", strlen("performance"));
  //          cmd4File->flush();
  //          m_cpuTimeout.start(20000, this);
  //      } else {
  //          m_cpuTimeout.stop();
  //          m_cpuTimeout.start(20000, this);
  //      }
  //  }

    return false;
}

bool BacklightInputEventFilter::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
  // cmd0File->write("performance", strlen("performance"));
  // cmd0File->flush();
  // cmd4File->write("performance", strlen("performance"));
  // cmd4File->flush();


    return false;
}

bool BacklightInputEventFilter::touchUp(qint32 id, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(time)
  // cmd0File->write("ondemand", strlen("ondemand"));
  // cmd0File->flush();
  // cmd4File->write("ondemand", strlen("ondemand"));
  // cmd4File->flush();


    return false;
}

bool BacklightInputEventFilter::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    return false;
}


HwcomposerBackend::HwcomposerBackend(QObject *parent)
    : Platform(parent)
{
    /*! repowerd */
    QDBusConnection connection = QDBusConnection::systemBus();
    if (!connection.connect(QStringLiteral(""),
                            QStringLiteral("/com/jingos/repowerd/Display"),
                            QStringLiteral("com.jingos.repowerd.Display"),
                            "TurnOff",
                            this,
                            SLOT(screenTurnOff(QString)))) {
        qCWarning(KWIN_HWCOMPOSER) << "--err:" << connection.lastError().message();
    };
    if (!connection.connect(QStringLiteral(""),
                            QStringLiteral("/com/jingos/repowerd/Display"),
                            QStringLiteral("com.jingos.repowerd.Display"),
                            "TurnOn",
                            this,
                            SLOT(screenTurnOn(QString)))) {
        qCWarning(KWIN_HWCOMPOSER) << "--err:" << connection.lastError().message();
    };

    /* //! [dba debug: 2021-07-31] [BUG 2967] 存在repowerd背光调节结束，屏幕还没上电的情况.
    if (!connection.connect(QStringLiteral("com.jingos.repowerd.Screen"),
                            QStringLiteral("/com/jingos/repowerd/Screen"),
                            QStringLiteral("com.jingos.repowerd.Screen"),
                            "DisplayPowerStateChange",
                            this,
                            SLOT(displayPowerStateChange(int, int)))) {
        qCWarning(KWIN_HWCOMPOSER) << "--err:" << connection.lastError().message();
    };*/

    setPerScreenRenderingEnabled(true);
    supportsOutputChanges();

    m_bShowFps = false;
    QString debugShowFps = qgetenv("KWIN_DEBUG_SHOW_FPS");
    if (!debugShowFps.isEmpty()) {
        m_bShowFps = true;
        QTimer::singleShot(1000, this, &HwcomposerBackend::showFps);
    }
}

HwcomposerBackend::~HwcomposerBackend()
{
    if (sceneEglDisplay() != EGL_NO_DISPLAY) {
        eglTerminate(sceneEglDisplay());
    }
}

#if defined(HWC_DEVICE_API_VERSION_2_0)
typedef struct : public HWC2EventListener
{
    HwcomposerBackend *backend = nullptr;
} HwcProcs_v20;

void hwc2_callback_vsync(HWC2EventListener *listener, int32_t sequenceId,
                         hwc2_display_t display, int64_t timestamp)
{
#if HAVE_PERFETTO //! [dba debug: 2021-07-02] 此处在binder线程，没有perfetto时不要调用qdebug.
    qDebug() << "[PERFETTO]"<<"vsync"<<timestamp;
#endif
    static_cast<const HwcProcs_v20 *>(listener)->backend->wakeVSync();
}

void hwc2_callback_hotplug(HWC2EventListener *listener, int32_t sequenceId,
                           hwc2_display_t display, bool connected,
                           bool primaryDisplay)
{
    hwc2_compat_device_on_hotplug(static_cast<const HwcProcs_v20 *>(listener)->backend->hwc2_device(), display, connected);
}

void hwc2_callback_refresh(HWC2EventListener *listener, int32_t sequenceId,
                           hwc2_display_t display)
{

}
#endif

void HwcomposerBackend::RegisterCallbacks()
{
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (m_hwcVersion == HWC_DEVICE_API_VERSION_2_0) {
        static int composerSequenceId = 0;

        HwcProcs_v20 *procs = new HwcProcs_v20();
        procs->on_vsync_received = hwc2_callback_vsync;
        procs->on_hotplug_received = hwc2_callback_hotplug;
        procs->on_refresh_received = hwc2_callback_refresh;
        procs->backend = this;

        hwc2_compat_device_register_callback(m_hwc2device, procs, composerSequenceId++);
        return;
    }
#endif

    // register callbacks
    hwc_procs_t *procs = new hwc_procs_t;
    procs->invalidate = [](const struct hwc_procs *procs) {
        Q_UNUSED(procs)
    };
    procs->vsync = [](const struct hwc_procs *procs, int disp, int64_t timestamp) {
        Q_UNUSED(procs)
        if (disp != 0) {
            return;
        }
        dynamic_cast<HwcomposerBackend *>(kwinApp()->platform())->wakeVSync();
    };
    procs->hotplug = [](const struct hwc_procs *procs, int disp, int connected) {
        Q_UNUSED(procs)
        Q_UNUSED(disp)
        Q_UNUSED(connected)
    };
    m_device->registerProcs(m_device, procs);
}

void HwcomposerBackend::init()
{
    hw_module_t *hwcModule = nullptr;
    if (hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t **)&hwcModule) != 0) {
        qCWarning(KWIN_HWCOMPOSER) << "Failed to get hwcomposer module";
        emit initFailed();
        return;
    }

    hw_device_t *hwDevice = nullptr;
    hwc_composer_device_1_t *hwcDevice = nullptr;
    if (hwcModule->methods->open(hwcModule, HWC_HARDWARE_COMPOSER, &hwDevice) != 0) {
#if defined(HWC_DEVICE_API_VERSION_2_0)
        qCWarning(KWIN_HWCOMPOSER) << "Failed to open hwcomposer device, probably it's hwc2";
        m_hwcVersion = HWC_DEVICE_API_VERSION_2_0;
#else
        qCWarning(KWIN_HWCOMPOSER) << "Failed to open hwcomposer device";
        emit initFailed();
        return;
#endif
    } else {
        hwc_composer_device_1_t *hwcDevice = (hwc_composer_device_1_t *)hwDevice;
        m_hwcVersion = hwcDevice->common.version;
        if ((m_hwcVersion & 0xffff0000) == 0) {
            // Assume header version is always 1
            uint32_t header_version = 1;
            // Legacy version encoding
            m_hwcVersion = (m_hwcVersion << 16) | header_version;
        }
    }
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (m_hwcVersion == HWC_DEVICE_API_VERSION_2_0)
        m_hwc2device = hwc2_compat_device_new(false);
    else
#endif
        m_device = hwcDevice;

    RegisterCallbacks();
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (m_hwcVersion == HWC_DEVICE_API_VERSION_2_0) {
        for (int i = 0; i < 5 * 1000; ++i) {
            // Wait at most 5s for hotplug events
            if ((m_hwc2_primary_display =
                     hwc2_compat_device_get_display_by_id(m_hwc2device, 0)))
                break;
            usleep(1000);
        }
    }
#endif
    m_filter.reset(new BacklightInputEventFilter(this));
    input()->prependInputEventFilter(m_filter.data());

    // get display configuration
    m_output.reset(new HwcomposerOutput(m_hwcVersion, hwcDevice, m_hwc2_primary_display));
    m_output.data()->init();
    if (!m_output->isValid()) {
        emit initFailed();
        return;
    }

    if (m_output->refreshRate() != 0) {
        m_vsyncInterval = 1000000 / m_output->refreshRate();
    }

    emit outputAdded(m_output.data());
    emit outputEnabled(m_output.data());

    using namespace KWaylandServer;
    auto updateDpms = [this] {
        if (!m_output || !m_output->waylandOutput()) {
            m_output->waylandOutput()->setDpmsMode(OutputInterface::DpmsMode::On);
        }
    };
    connect(this, &HwcomposerBackend::outputBlankChanged, this, updateDpms);

    connect(m_output.data(), &HwcomposerOutput::dpmsModeRequested, this,
            [this](KWaylandServer::OutputInterface::DpmsMode mode) {
                updateOutputs();
            });

    updateOutputs();

    emit screensQueried();
    setReady(true);
    screenTurnOn("all");

    // lijing for android compatible env
    std::string lib_path = AndroidAssistance::AndroidCompatibleExist();
    if (!lib_path.empty()) {
        m_android_assis.reset(new AndroidAssistance(lib_path));
        m_android_assis->setCallback(AndroidAssistance::HardwareInfo_t {
                size().width(),
                size().height(),
                0,
                m_output->refreshRate()
        });
    }
}

QSize HwcomposerBackend::size() const
{
    if (m_output) {
        return m_output->pixelSize();
    }
    return QSize();
}

QSize HwcomposerBackend::screenSize() const
{
    if (m_output) {
        return m_output->pixelSize() / m_output->scale();
    }
    return QSize();
}

int HwcomposerBackend::scale() const
{
    if (m_output) {
        return m_output->scale();
    }
    return 1;
}


void HwcomposerBackend::displayPowerStateChange(int status, int reason)
{
    qDebug() << __FILE__ << __LINE__<<__FUNCTION__<<status << reason;
    if (status) {
        screenTurnOn("all");
    } else {
        screenTurnOff("all");
    }
}

/*!
 * \brief HwcomposerBackend::screenTurnOn
 * [in] all internal  external
 */
void HwcomposerBackend::screenTurnOn(QString str)
{
    qDebug() << __FILE__ << __LINE__<<__FUNCTION__<<str;
    setPowerMode(true);
    if (Compositor *compositor = Compositor::self()) {
    compositor->addRepaintFull();
    }
    emit outputBlankChanged();
}

/*!
 * \brief HwcomposerBackend::screenTurnOn
 * [in] all internal  external
 */
void HwcomposerBackend::screenTurnOff(QString str)
{
    qDebug() << __FILE__ << __LINE__<<__FUNCTION__<<str;
    setPowerMode(false);
}

void HwcomposerBackend::enableVSync(bool enable)
{
    if (m_hasVsync == enable) {
        return;
    }
    int result = 0;
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (m_hwcVersion == HWC_DEVICE_API_VERSION_2_0)
        hwc2_compat_display_set_vsync_enabled(m_hwc2_primary_display, enable ? HWC2_VSYNC_ENABLE : HWC2_VSYNC_DISABLE);
    else
#endif
    result = m_device->eventControl(m_device, 0, HWC_EVENT_VSYNC, enable ? 1 : 0);
    m_hasVsync = enable && (result == 0);
}

HwcomposerWindow *HwcomposerBackend::createSurface()
{
    return new HwcomposerWindow(this);
}

Outputs HwcomposerBackend::outputs() const
{
    if (!m_output.isNull()) {
        return QVector<HwcomposerOutput *>({m_output.data()});
    }
    return {};
}

Outputs HwcomposerBackend::enabledOutputs() const
{
    return outputs();
}

void HwcomposerBackend::updateOutputsEnabled()
{
    bool enabled = true;
    setOutputsEnabled(enabled);
}
void HwcomposerBackend::setPowerMode(bool enable)
{
    setOutputsEnabled(enable);
    if(enable){
        hwc2_compat_display_set_power_mode(m_hwc2_primary_display, HWC2_POWER_MODE_ON);
        hwc2_compat_display_set_vsync_enabled(m_hwc2_primary_display, HWC2_VSYNC_ENABLE);
    }else{
        hwc2_compat_display_set_vsync_enabled(m_hwc2_primary_display, HWC2_VSYNC_DISABLE);
        hwc2_compat_display_set_power_mode(m_hwc2_primary_display, HWC2_POWER_MODE_OFF);
    }

    //! 同步power model状态给已经改变且生效给repowerd。
    QDBusMessage message = QDBusMessage::createMethodCall(  "com.jingos.repowerd.PowerButton",
                                                            "/com/jingos/repowerd/PowerButton",
                                                            "com.jingos.repowerd.PowerButton",
                                                            "powerModeChange");
    message << enable;
    QDBusMessage response = QDBusConnection::systemBus().call(message);
    if (response.type() != QDBusMessage::ReplyMessage) {
        qDebug() << __FILE__ << __LINE__ << "method called failed!";
    }
}
bool HwcomposerBackend::updateOutputs()
{
    updateOutputsEnabled();
    emit screensQueried();

    return true;
}
OpenGLBackend *HwcomposerBackend::createOpenGLBackend()
{
    return new EglHwcomposerBackend(this);
}

void HwcomposerBackend::waitVSync()
{
    if (!m_hasVsync) {
        return;
    }
    m_vsyncMutex.lock();
    m_vsyncWaitCondition.wait(&m_vsyncMutex, m_vsyncInterval);
    m_vsyncMutex.unlock();
}

void HwcomposerBackend::compositing(int flags)
{
#if HAVE_PERFETTO
    qDebug() << "[PERFETTO]"
             << "compositing:"
             << "flags:" << flags;
#endif
    m_compositingSemaphore.release();
    // Compositor::self()->runPerformCompositing(flags, m_output->renderLoop());
    if(flags > 0){
        RenderLoopPrivate *renderLoopPrivate = RenderLoopPrivate::get(m_output->renderLoop());
        if(renderLoopPrivate->pendingFrameCount > 0){
            renderLoopPrivate->notifyFrameCompleted(std::chrono::steady_clock::now().time_since_epoch());
        }else{
            #if HAVE_PERFETTO
            qDebug() << "[PERFETTO]"<< "pending frame count = 0";
            #endif
        }
    }
    m_compositingSemaphore.acquire();
}

void HwcomposerBackend::wakeVSync()
{
    int flags = 1;
    if (m_compositingSemaphore.available() > 0) {
        flags = 0;
    }
    QMetaObject::invokeMethod(this, "compositing", Qt::QueuedConnection, Q_ARG(int, flags));

    qDebug() << "lijing wakeVSync";
    // lijing for android compatible env
    if (m_android_assis != nullptr) {
        m_android_assis->wakeupVsync();
    }
}

void HwcomposerBackend::doHideCursor()
{
}
void HwcomposerBackend::doShowCursor()
{
    if (usesSoftwareCursor()) {
        return;
    }
    updateCursor();
}
void HwcomposerBackend::doSetSoftwareCursor()
{
}

void HwcomposerBackend::updateCursor()
{
    if (isSoftwareCursorForced() || isCursorHidden()) {
        return;
    }

    auto cursor = Cursors::self()->currentCursor();
    if (cursor->image().isNull()) {
        doHideCursor();
        return;
    }

    bool success = false;

    setSoftwareCursor(!success);
}
void HwcomposerBackend::moveCursor() {
}
void HwcomposerBackend::initCursor() {
    setSoftwareCursorForced(true);
    if (waylandServer()->seat()->hasPointer()) {
        // The cursor is visible by default, do nothing.
    } else {
        hideCursor();
    }

    connect(waylandServer()->seat(), &KWaylandServer::SeatInterface::hasPointerChanged, this,
            [this] {
                if (waylandServer()->seat()->hasPointer()) {
                    showCursor();
                } else {
                    hideCursor();
                }
            });
    // now we have screens and can set cursors, so start tracking
    connect(Cursors::self(), &Cursors::currentCursorChanged, this, &HwcomposerBackend::updateCursor);
    connect(Cursors::self(), &Cursors::positionChanged, this, &HwcomposerBackend::moveCursor);
}

/**********************************************************************************/

static void initLayer(hwc_layer_1_t *layer, const hwc_rect_t &rect, int layerCompositionType) {
    memset(layer, 0, sizeof(hwc_layer_1_t));
    layer->compositionType = layerCompositionType;
    layer->hints = 0;
    layer->flags = 0;
    layer->handle = 0;
    layer->transform = 0;
    layer->blending = HWC_BLENDING_NONE;
#ifdef HWC_DEVICE_API_VERSION_1_3
    layer->sourceCropf.top = 0.0f;
    layer->sourceCropf.left = 0.0f;
    layer->sourceCropf.bottom = (float)rect.bottom;
    layer->sourceCropf.right = (float)rect.right;
#else
    layer->sourceCrop = rect;
#endif
    layer->displayFrame = rect;
    layer->visibleRegionScreen.numRects = 1;
    layer->visibleRegionScreen.rects = &layer->displayFrame;
    layer->acquireFenceFd = -1;
    layer->releaseFenceFd = -1;
    layer->planeAlpha = 0xFF;
#ifdef HWC_DEVICE_API_VERSION_1_5
    layer->surfaceDamage.numRects = 0;
#endif
}

HwcomposerWindow::HwcomposerWindow(HwcomposerBackend *backend) //! [dba debug: 2021-06-18]
    : HWComposerNativeWindow( backend->size().width(),  backend->size().height(), HAL_PIXEL_FORMAT_RGBA_8888), m_backend(backend)
{
    setBufferCount(3);
    uint32_t hwcVersion = m_backend->deviceVersion();
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (hwcVersion == HWC_DEVICE_API_VERSION_2_0) {
        m_hwc2_primary_display = m_backend->hwc2_display();
        hwc2_compat_layer_t *layer = hwc2_compat_display_create_layer(m_hwc2_primary_display);
        hwc2_compat_layer_set_composition_type(layer, HWC2_COMPOSITION_CLIENT);
        hwc2_compat_layer_set_blend_mode(layer, HWC2_BLEND_MODE_NONE);
        hwc2_compat_layer_set_transform(layer, HWC_TRANSFORM_ROT_90);

        hwc2_compat_layer_set_source_crop(layer, 0.0f, 0.0f, m_backend->size().width(), m_backend->size().height());
        hwc2_compat_layer_set_display_frame(layer, 0, 0, m_backend->size().width(), m_backend->size().height());
        hwc2_compat_layer_set_visible_region(layer, 0, 0, m_backend->size().width(), m_backend->size().height());
    } else {
#endif
        size_t size = sizeof(hwc_display_contents_1_t) + 2 * sizeof(hwc_layer_1_t);
        hwc_display_contents_1_t *list = (hwc_display_contents_1_t *)malloc(size);
        m_list = (hwc_display_contents_1_t **)malloc(HWC_NUM_DISPLAY_TYPES * sizeof(hwc_display_contents_1_t *));
        for (int i = 0; i < HWC_NUM_DISPLAY_TYPES; ++i) {
            m_list[i] = nullptr;
        }
        // Assign buffer only to the first item, otherwise you get tearing
        // if passed the same to multiple places
        // see https://github.com/mer-hybris/qt5-qpa-hwcomposer-plugin/commit/f1d802151e8a4f5d10d60eb8de8e07552b93a34a
        m_list[0] = list;
        const hwc_rect_t rect = {
            0,
            0,
            m_backend->size().width(),
            m_backend->size().height()};
        initLayer(&list->hwLayers[0], rect, HWC_FRAMEBUFFER);
        initLayer(&list->hwLayers[1], rect, HWC_FRAMEBUFFER_TARGET);

        list->retireFenceFd = -1;
        list->flags = HWC_GEOMETRY_CHANGED;
        list->numHwLayers = 2;
#if defined(HWC_DEVICE_API_VERSION_2_0)
    }
#endif
    backend->initCursor();
}

HwcomposerWindow::~HwcomposerWindow()
{
    if (lastPresentFence != -1) {
        close(lastPresentFence);
    }
}

void HwcomposerWindow::present(HWComposerNativeWindowBuffer *buffer)
{
    //qDebug()<<__FILE__<<__LINE__<<"buffer size:"<<buffer->width << buffer->height;
    if (m_backend->m_bShowFps) {
        m_backend->m_fps++;
    }
#if HAVE_PERFETTO
    qDebug() << "[PERFETTO]"<<"present start";
#endif

    uint32_t hwcVersion = m_backend->deviceVersion();
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (hwcVersion == HWC_DEVICE_API_VERSION_2_0) {
        uint32_t numTypes = 0;
        uint32_t numRequests = 0;
        int displayId = 0;
        hwc2_error_t error = HWC2_ERROR_NONE;

        int acquireFenceFd = HWCNativeBufferGetFence(buffer);
        int syncBeforeSet = 1;

        if (syncBeforeSet && acquireFenceFd >= 0) {
            sync_wait(acquireFenceFd, -1);
            close(acquireFenceFd);
            acquireFenceFd = -1;
        }
        /*
        {  //! 通过presentOrValidate接口尝试直接present
            int32_t presentFence = 0;
            uint32_t state;
            hwc2_error_t error = hwc2_compat_display_present_or_validate(m_hwc2_primary_display, &numTypes, &numRequests, &presentFence, &state);
            if (error != HWC2_ERROR_NONE && error != HWC2_ERROR_HAS_CHANGES) {
                qDebug("presentOrValidate : %d:  (%d)\n", displayId, error);
            }
            if (state == 1) {
                qDebug("Present Succeeded.\n");
                return;
            }
        }*/

        error = hwc2_compat_display_validate(m_hwc2_primary_display, &numTypes, &numRequests);
        if (error != HWC2_ERROR_NONE && error != HWC2_ERROR_HAS_CHANGES) {
            qDebug("prepare: validate failed for display %d: %d", displayId, error);
            return;
        }

        if (numTypes || numRequests) {
            qDebug("prepare: validate required changes for display %d: %d",displayId, error);
            return;
        }

        error = hwc2_compat_display_accept_changes(m_hwc2_primary_display);
        if (error != HWC2_ERROR_NONE) {
            qDebug("prepare: acceptChanges failed: %d", error);
            return;
        }

        hwc2_compat_display_set_client_target(m_hwc2_primary_display, /* slot */ 0, buffer,
                                              acquireFenceFd,
                                              HAL_DATASPACE_UNKNOWN);

        int presentFence = -1;
        hwc2_compat_display_present(m_hwc2_primary_display, &presentFence);


        if (lastPresentFence != -1) {
            sync_wait(lastPresentFence, -1);
            close(lastPresentFence);
        }

        lastPresentFence = presentFence != -1 ? dup(presentFence) : -1;

        HWCNativeBufferSetFence(buffer, presentFence);

    } else {
#endif
        hwc_composer_device_1_t *device = m_backend->device();

        auto fblayer = &m_list[0]->hwLayers[1];
        fblayer->handle = buffer->handle;
        fblayer->acquireFenceFd = getFenceBufferFd(buffer);
        fblayer->releaseFenceFd = -1;

        int err = device->prepare(device, 1, m_list);
        Q_ASSERT(err == 0);

        err = device->set(device, 1, m_list);
        Q_ASSERT(err == 0);
        m_backend->enableVSync(true);
        setFenceBufferFd(buffer, fblayer->releaseFenceFd);

        if (m_list[0]->retireFenceFd != -1) {
            close(m_list[0]->retireFenceFd);
            m_list[0]->retireFenceFd = -1;
        }
        m_list[0]->flags = 0;
#if defined(HWC_DEVICE_API_VERSION_2_0)
    }
#endif
#if HAVE_PERFETTO
    qDebug() << "[PERFETTO]"<<"present end";
#endif
}

bool HwcomposerOutput::hardwareTransforms() const
{
    return false;
}

HwcomposerOutput::HwcomposerOutput(uint32_t hwcVersion, hwc_composer_device_1_t *device, hwc2_compat_display_t *hwc2_primary_display)
    : AbstractWaylandOutput(), m_renderLoop(new RenderLoop(this)), m_hwcVersion(hwcVersion), m_device(device), m_hwc2_primary_display(hwc2_primary_display)
{
    int32_t attr_values[5];
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (hwcVersion == HWC_DEVICE_API_VERSION_2_0) {
        HWC2DisplayConfig *config = hwc2_compat_display_get_active_config(hwc2_primary_display);
        Q_ASSERT(config);
        //! [dba debug: 2021-10-16] 没有手机代码分支，用分辨率区分手机
        if((config->width == 720) && (config->height == 1600)){
            attr_values[0] = config->width;
            attr_values[1] = config->height;
            attr_values[2] = config->dpiX;
            attr_values[3] = config->dpiY;
        }else{ //! [dba debug: 2021-10-16]  jingpad在drm中对屏幕做了旋转，这里需要将长宽与其对应
            attr_values[1] = config->width;
            attr_values[0] = config->height;
            attr_values[3] = config->dpiX;
            attr_values[2] = config->dpiY;
        }

        //! 莱迪斯
        if (attr_values[0] == 2072){
            attr_values[4] = 20000000;
        }else{
            attr_values[4] = config->vsyncPeriod;
        }

        /*dba  debug */
        QString debugWidth = qgetenv("KWIN_DEBUG_WIDTH");
        if (!debugWidth.isEmpty()) {
            attr_values[0] = debugWidth.toInt();
        }
        QString debugHeight = qgetenv("KWIN_DEBUG_HEIGHT");
        if (!debugHeight.isEmpty()) {
            attr_values[1] = debugHeight.toInt();
        }
    } else {
#endif
        uint32_t configs[5];
        size_t numConfigs = 5;
        if (device->getDisplayConfigs(device, 0, configs, &numConfigs) != 0) {
            qCWarning(KWIN_HWCOMPOSER) << "Failed to get hwcomposer display configurations";
            return;
        }

        uint32_t attributes[] = {
            HWC_DISPLAY_WIDTH,
            HWC_DISPLAY_HEIGHT,
            HWC_DISPLAY_DPI_X,
            HWC_DISPLAY_DPI_Y,
            HWC_DISPLAY_VSYNC_PERIOD,
            HWC_DISPLAY_NO_ATTRIBUTE};
        device->getDisplayAttributes(device, 0, configs[0], attributes, attr_values);
#if defined(HWC_DEVICE_API_VERSION_2_0)
    }
#endif
    QSize pixelSize(attr_values[0], attr_values[1]);

    if (pixelSize.isEmpty()) {
        return;
    }

    QSizeF physicalSize = pixelSize / 3.8;
    if (attr_values[2] != 0 && attr_values[3] != 0) {
        static const qreal factor = 25.4;
        physicalSize = QSizeF(qreal(pixelSize.width() * 1000) / qreal(attr_values[2]) * factor,
                              qreal(pixelSize.height() * 1000) / qreal(attr_values[3]) * factor);
    }

    /*dba  debug */
    QString debugDpi = qgetenv("KWIN_DEBUG_DPI");
    if (!debugDpi.isEmpty()) {
        if (debugDpi.toFloat() != 0) {
            physicalSize = pixelSize / debugDpi.toFloat();
        }
    }

    // read in mode information

    QVector<KWaylandServer::OutputDeviceInterface::Mode> modes;
    {
        KWaylandServer::OutputDeviceInterface::ModeFlags deviceflags = 0;
        deviceflags |= KWaylandServer::OutputDeviceInterface::ModeFlag::Current;
        deviceflags |= KWaylandServer::OutputDeviceInterface::ModeFlag::Preferred;

        KWaylandServer::OutputDeviceInterface::Mode mode;
        mode.id = 0;
        mode.size = QSize(attr_values[0], attr_values[1]);
        mode.flags = deviceflags;
        mode.refreshRate = (attr_values[4] == 0) ? 60000 : 10E11 / attr_values[4];
        modes << mode;
    }
    initInterfaces(QString(), QString(), QByteArray(), physicalSize.toSize(), modes, {});
    setInternal(true);
    setDpmsSupported(true);

    const auto outputGroup = kwinApp()->config()->group("HWComposerOutputs").group("0");
    setWaylandMode(pixelSize, modes[0].refreshRate);
    //setTransform(HwcomposerOutput::Transform::Rotated270);

    /*! scale */
    const qreal dpi = modeSize().height() / (physicalSize.height() / 25.4);
    KConfig _cfgfonts(QStringLiteral("kcmfonts"));
    KConfigGroup cfgfonts(&_cfgfonts, "General");
    qDebug() << Q_FUNC_INFO << "set default xft dpi: modeSize:" << modeSize() << "physicalSize:" << physicalSize << "dpi:" << dpi;
//    cfgfonts.writeEntry("defaultXftDpi", dpi > 96 * 1.5 ? 192 : 96);
    cfgfonts.writeEntry("defaultXftDpi", 192);
    setScale(1.0);

    /*dba  debug */
    QString debugScale = qgetenv("KWIN_DEBUG_SCALE");
    if (!debugScale.isEmpty()) {
        setScale(outputGroup.readEntry("Scale", debugScale.toFloat()));
    }
}

HwcomposerOutput::~HwcomposerOutput()
{
#if defined(HWC_DEVICE_API_VERSION_2_0)
    if (m_hwcVersion == HWC_DEVICE_API_VERSION_2_0) {
        if (m_hwc2_primary_display != NULL) {
            free(m_hwc2_primary_display);
        }
    } else {
#endif
        hwc_close_1(m_device);
#if defined(HWC_DEVICE_API_VERSION_2_0)
    }
#endif
}

void HwcomposerOutput::init()
{
    const int refreshRate = 60000; // TODO: get refresh rate via randr
    m_renderLoop->setRefreshRate(refreshRate);
}

RenderLoop *HwcomposerOutput::renderLoop() const
{
    return m_renderLoop;
}

bool HwcomposerOutput::isValid() const
{
    return isEnabled();
}

void HwcomposerOutput::updateDpms(KWaylandServer::OutputInterface::DpmsMode mode)
{
     //qDebug()<<__FILE__<<__LINE__<<__FUNCTION__;
    emit dpmsModeRequested(mode);
}

QSize HwcomposerOutput::pixelSize() const
{
    //qDebug()<<__FUNCTION__<< AbstractWaylandOutput::pixelSize();
    //QSize size = AbstractWaylandOutput::pixelSize();
    //return  QSize(size.height(), size.width()) ;
    return  AbstractWaylandOutput::pixelSize() ;
}
QRect HwcomposerOutput::geometry() const
{
    //qDebug()<<__FUNCTION__<< AbstractWaylandOutput::geometry();
    //QSize size = AbstractWaylandOutput::geometry().size();
    // return QRect(0,0,size.height(), size.width() );
    return AbstractWaylandOutput::geometry();
}
QSize HwcomposerOutput::physicalSize() const
{
    //qDebug()<<__FUNCTION__<< AbstractWaylandOutput::physicalSize();
    //QSize size = AbstractWaylandOutput::physicalSize();
    // return  QSize(size.height(), size.width());
    return AbstractWaylandOutput::physicalSize() ;
}
void HwcomposerBackend::showFps()
{
    qDebug() << "fps:" << m_fps;
    m_fps = 0;
    QTimer::singleShot(1000, this, &HwcomposerBackend::showFps);
}
}  // namespace KWin
