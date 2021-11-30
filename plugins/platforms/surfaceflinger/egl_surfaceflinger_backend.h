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
#ifndef KWIN_EGL_SURFACEFLINGER_BACKEND_H
#define KWIN_EGL_SURFACEFLINGER_BACKEND_H
#include "abstract_egl_backend.h"

namespace KWin
{

class SurfaceFlingerBackend;

class EglSurfaceFlingerBackend : public AbstractEglBackend
{
public:
    EglSurfaceFlingerBackend(SurfaceFlingerBackend *backend);
    virtual ~EglSurfaceFlingerBackend();
    bool usesOverlayWindow() const override;
    SceneOpenGLTexturePrivate *createBackendTexture(SceneOpenGLTexture *texture) override;
    void screenGeometryChanged(const QSize &size) override;
    QRegion beginFrame(int screenId) override;
    void endFrame(int screenId, const QRegion &renderedRegion, const QRegion &damagedRegion) override;
    void init() override;

protected:
    void present() override;

private:
    bool initializeEgl();
    bool initRenderingContext();
    bool initBufferConfigs();
    bool makeContextCurrent();
    SurfaceFlingerBackend *m_pBackend;
};

class EglSurfaceFlingerTexture : public AbstractEglTexture
{
public:
    virtual ~EglSurfaceFlingerTexture();

private:
    friend class EglSurfaceFlingerBackend;
    EglSurfaceFlingerTexture(SceneOpenGLTexture *texture, EglSurfaceFlingerBackend *backend);
};

}

#endif
