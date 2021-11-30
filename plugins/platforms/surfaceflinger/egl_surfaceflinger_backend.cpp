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
#include "egl_surfaceflinger_backend.h"
#include "surfaceflinger_backend.h"
#include "logging.h"

namespace KWin
{

EglSurfaceFlingerBackend::EglSurfaceFlingerBackend(SurfaceFlingerBackend *backend)
    : AbstractEglBackend()
    , m_pBackend(backend)
{
    // EGL is always direct rendering
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
    //setBlocksForRetrace(true);
}

EglSurfaceFlingerBackend::~EglSurfaceFlingerBackend()
{
    cleanup();
}

bool EglSurfaceFlingerBackend::initializeEgl()
{
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__;
    EGLDisplay display = m_pBackend->sceneEglDisplay();

    if (display == EGL_NO_DISPLAY) {
        display = m_pBackend->display();
    }
    if (display == EGL_NO_DISPLAY) {
	return false;
    }
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__;
    setEglDisplay(display);
    return initEglAPI();
}

void EglSurfaceFlingerBackend::init()
{
    qCDebug(KWIN_SURFACEFLINGER) << "EglSurfaceFlingerBackend::init";
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "EglSurfaceFlingerBackend::init";
    if (!initializeEgl()) {
        setFailed("Failed to initialize egl");
        return;
    }
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "initialize egl succeed";

    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "initialize rendering context succeed";

    initKWinGL();
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "initKWinGL succeed";
    initBufferAge();
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "initBufferAge succeed";
    initWayland();
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "initBufferAge succeed";

    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__<< "EglSurfaceFlingerBackend done!";
}

bool EglSurfaceFlingerBackend::initBufferConfigs()
{
    setConfig(m_pBackend->config());

    return true;
}

bool EglSurfaceFlingerBackend::initRenderingContext()
{
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__;
    if (!initBufferConfigs()) {
        return false;
    }
    qCDebug(KWIN_SURFACEFLINGER)<<"[dba debug]"<<__FILE__<<__LINE__<<__FUNCTION__;
    //![dba] createContext();
    setContext(m_pBackend->context());
    setSurface(m_pBackend->createEglSurface());

    return makeContextCurrent();
}

bool EglSurfaceFlingerBackend::makeContextCurrent()
{
    m_pBackend->makeCurrent();
    
    return true;
}

void EglSurfaceFlingerBackend::present()
{
    if (lastDamage().isEmpty()) {
        return;
    }

    eglSwapBuffers(eglDisplay(), surface());
    setLastDamage(QRegion());
}

void EglSurfaceFlingerBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

QRegion EglSurfaceFlingerBackend::beginFrame(int screenId)
{
    present();
    return QRegion(QRect(QPoint(0, 0), m_pBackend->size()));
}

void EglSurfaceFlingerBackend::endFrame(int screenId,const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
    setLastDamage(renderedRegion);
}

SceneOpenGLTexturePrivate *EglSurfaceFlingerBackend::createBackendTexture(SceneOpenGLTexture *texture)
{
    return new EglSurfaceFlingerTexture(texture, this);
}

bool EglSurfaceFlingerBackend::usesOverlayWindow() const
{
    return false;
}

EglSurfaceFlingerTexture::EglSurfaceFlingerTexture(SceneOpenGLTexture *texture, EglSurfaceFlingerBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglSurfaceFlingerTexture::~EglSurfaceFlingerTexture() = default;

}
