/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2006 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.

Based on glcompmgr code by Felix Bellaby.
******************************************************************/



#include "scene_opengl.h"

#include "utils.h"
#include "client.h"
#include "effects.h"

#include <dlfcn.h>

namespace KWinInternal
{

//****************************************
// SceneOpenGL
//****************************************

GLXFBConfig SceneOpenGL::fbcdrawable;
GLXContext SceneOpenGL::context;
GLXPixmap SceneOpenGL::glxroot;
bool SceneOpenGL::tfp_mode; // using glXBindTexImageEXT (texture_from_pixmap)
bool SceneOpenGL::root_db; // destination drawable is double-buffered

typedef void (*glXBindTexImageEXT_func)( Display* dpy, GLXDrawable drawable,
    int buffer, const int* attrib_list );
typedef void (*glXReleaseTexImageEXT_func)( Display* dpy, GLXDrawable drawable, int buffer );
typedef void (*glXFuncPtr)();
typedef glXFuncPtr (*glXGetProcAddress_func)( const GLubyte* );
glXBindTexImageEXT_func glXBindTexImageEXT;
glXReleaseTexImageEXT_func glXReleaseTexImageEXT;
glXGetProcAddress_func glXGetProcAddress;

static void checkGLError( const char* txt )
    {
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        kWarning() << "GL error (" << txt << "): 0x" << QString::number( err, 16 ) << endl;
    }

const int root_db_attrs[] =
    {
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    None
    };

static const int root_buffer_attrs[] =
    {
    GLX_DOUBLEBUFFER, False,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    None
    };

const int drawable_attrs[] = 
    {
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 0,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    None
    };

const int drawable_tfp_attrs[] = 
    {
    GLX_DOUBLEBUFFER, False,
    GLX_DEPTH_SIZE, 0,
    GLX_RED_SIZE, 1,
    GLX_GREEN_SIZE, 1,
    GLX_BLUE_SIZE, 1,
    GLX_ALPHA_SIZE, 1,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    GLX_BIND_TO_TEXTURE_RGBA_EXT, True, // additional for tfp
    None
    };

static glXFuncPtr getProcAddress( const char* name )
    {
    glXFuncPtr ret = NULL;
    if( glXGetProcAddress != NULL )
        ret = glXGetProcAddress( ( const GLubyte* ) name );
    if( ret == NULL )
        ret = ( glXFuncPtr ) dlsym( RTLD_DEFAULT, name );
    return ret;
    }

SceneOpenGL::SceneOpenGL( Workspace* ws )
    : Scene( ws )
    {
    // TODO add checks where needed
    int dummy;
    if( !glXQueryExtension( display(), &dummy, &dummy ))
        return;
    glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddress" );
    if( glXGetProcAddress == NULL )
        glXGetProcAddress = (glXGetProcAddress_func) getProcAddress( "glxGetProcAddressARB" );
    glXBindTexImageEXT = (glXBindTexImageEXT_func) getProcAddress( "glXBindTexImageEXT" );
    glXReleaseTexImageEXT = (glXReleaseTexImageEXT_func) getProcAddress( "glXReleaseTexImageEXT" );
    tfp_mode = ( glXBindTexImageEXT != NULL && glXReleaseTexImageEXT != NULL );
    initBuffer();
    if( tfp_mode )
        {
        if( !findConfig( drawable_tfp_attrs, fbcdrawable ))
            {
            tfp_mode = false;
            if( !findConfig( drawable_attrs, fbcdrawable ))
                assert( false );
            }
        }
    else
        if( !findConfig( drawable_attrs, fbcdrawable ))
            assert( false );
    context = glXCreateNewContext( display(), fbcroot, GLX_RGBA_TYPE, NULL, GL_FALSE );
    glXMakeContextCurrent( display(), glxroot, glxroot, context );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( 0, displayWidth(), 0, displayHeight(), 0, 65535 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    checkGLError( "Init" );
    kDebug() << "Root DB:" << root_db << ", TFP:" << tfp_mode << endl;
    }

SceneOpenGL::~SceneOpenGL()
    {
    for( QMap< Toplevel*, Window >::Iterator it = windows.begin();
         it != windows.end();
         ++it )
        (*it).free();
    if( root_db )
        glXDestroyWindow( display(), glxroot );
    else
        {
        glXDestroyPixmap( display(), glxroot );
        XFreeGC( display(), gcroot );
        XFreePixmap( display(), buffer );
        }
    glXDestroyContext( display(), context );
    checkGLError( "Cleanup" );
    }

void SceneOpenGL::initBuffer()
    {
    XWindowAttributes attrs;
    XGetWindowAttributes( display(), rootWindow(), &attrs );
    if( findConfig( root_db_attrs, fbcroot, XVisualIDFromVisual( attrs.visual )))
        root_db = true;
    else
        {
        if( findConfig( root_buffer_attrs, fbcroot ))
            root_db = false;
        else
            assert( false );
        }
    if( root_db )
        {
        buffer = rootWindow();
        glxroot = glXCreateWindow( display(), fbcroot, buffer, NULL );
        glDrawBuffer( GL_BACK );
        }
    else
        {
        XGCValues gcattr;
        gcattr.subwindow_mode = IncludeInferiors;
        gcroot = XCreateGC( display(), rootWindow(), GCSubwindowMode, &gcattr );
        buffer = XCreatePixmap( display(), rootWindow(), displayWidth(), displayHeight(),
            QX11Info::appDepth());
        glxroot = glXCreatePixmap( display(), fbcroot, buffer, NULL );
        }
    }

static void debugFBConfig( GLXFBConfig* fbconfigs, int i, const int* attrs )
    {
    int pos = 0;
    while( attrs[ pos ] != (int)None )
        {
        int value;
        if( glXGetFBConfigAttrib( display(), fbconfigs[ i ], attrs[ pos ], &value )
            == Success )
            kDebug() << "ATTR: 0x" << QString::number( attrs[ pos ], 16 )
                << ": 0x" << QString::number( attrs[ pos + 1 ], 16 )
                << ": 0x" << QString::number( value, 16 ) << endl;
        else
            kDebug() << "ATTR FAIL: 0x" << QString::number( attrs[ pos ], 16 ) << endl;
        pos += 2;
        }
    }

bool SceneOpenGL::findConfig( const int* attrs, GLXFBConfig& config, VisualID visual )
    {
    int cnt;
    GLXFBConfig* fbconfigs = glXChooseFBConfig( display(), DefaultScreen( display()),
        attrs, &cnt );
    if( fbconfigs != NULL )
        {
        if( visual == None )
            {
            config = fbconfigs[ 0 ];
            kDebug() << "Found FBConfig" << endl;
            debugFBConfig( fbconfigs, 0, attrs );
            XFree( fbconfigs );
            return true;
            }
        else
            {
            for( int i = 0;
                 i < cnt;
                 ++i )
                {
                int value;
                glXGetFBConfigAttrib( display(), fbconfigs[ i ], GLX_VISUAL_ID, &value );
                if( value == (int)visual )
                    {
                    kDebug() << "Found FBConfig" << endl;
                    config = fbconfigs[ i ];
                    debugFBConfig( fbconfigs, i, attrs );
                    XFree( fbconfigs );
                    return true;
                    }
                }
            }
        }
#if 0 // for debug
    fbconfigs = glXGetFBConfigs( display(), DefaultScreen( display()), &cnt );
    for( int i = 0;
         i < cnt;
         ++i )
        {
        kDebug() << "Listing FBConfig:" << i << endl;
        debugFBConfig( fbconfigs, i, attrs );
        }
    if( fbconfigs != NULL )
        XFree( fbconfigs );
#endif
    return false;
    }

void SceneOpenGL::paint( QRegion damage, ToplevelList toplevels )
    {
    grabXServer();
    glXWaitX();
    glPushMatrix();
    glClearColor( 0, 0, 0, 1 );
    glClear( GL_COLOR_BUFFER_BIT );
    glScalef( 1, -1, 1 );
    glTranslatef( 0, -displayHeight(), 0 );
    foreach( Toplevel* c, toplevels )
        {
        assert( windows.contains( c ));
        stacking_order.append( &windows[ c ] );
        }
    ScreenPaintData data;
    WrapperEffect wrapper;
    effects->paintScreen( PAINT_REGION, damage, data, &wrapper );
    glPopMatrix();
    if( root_db )
        glXSwapBuffers( display(), glxroot );
    else
        {
        glFlush();
        glXWaitGL();
        XCopyArea( display(), buffer, rootWindow(), gcroot, 0, 0, displayWidth(), displayHeight(), 0, 0 );
        XFlush( display());
        }
    ungrabXServer();
    checkGLError( "PostPaint" );
    stacking_order.clear();
    }

// the function that'll be eventually called by wrapper.peformPaintScreen() above
void SceneOpenGL::WrapperEffect::paintScreen( int mask, QRegion region, ScreenPaintData& data )
    {
    if( mask & PAINT_REGION )
        static_cast< SceneOpenGL* >( scene )->paintSimpleScreen( region );
    else
        static_cast< SceneOpenGL* >( scene )->paintGenericScreen();
    }
    
// the generic painting code that should eventually handle even
// transformations
void SceneOpenGL::paintGenericScreen()
    {
    paintBackground( infiniteRegion());
    foreach( Window* w, stacking_order ) // bottom to top
        {
        if( !w->isVisible())
            continue;
        WindowPaintData data;
//        data.opacity = w->opacity();
        WrapperEffect wrapper;
        effects->paintWindow( w, PAINT_OPAQUE | PAINT_TRANSLUCENT, infiniteRegion(), data, &wrapper );
        }
    }

// the optimized case without any transformations at all
void SceneOpenGL::paintSimpleScreen( QRegion region )
    {
    QList< Phase2Data > phase2;
    // TODO repaint only damaged areas (means also don't do glXSwapBuffers and similar)
    region = QRegion( 0, 0, displayWidth(), displayHeight());
    for( int i = stacking_order.count() - 1; // top to bottom
         i >= 0;
         --i )
        {
        Window* w = stacking_order[ i ];
        if( !w->isVisible())
            continue;
        if( region.isEmpty()) // completely clipped
            continue;
        if( !w->isOpaque())
            {
            phase2.prepend( Phase2Data( w, region ));
            continue;
            }
        WindowPaintData data;
//        data.opacity = w->opacity();
        WrapperEffect wrapper;
        effects->paintWindow( w, PAINT_OPAQUE, region, data, &wrapper );
        // window is opaque, clip windows below
        region -= w->shape().translated( w->x(), w->y());
        }
    paintBackground( region );
    foreach( Phase2Data d, phase2 )
        {
        Window* w = d.window;
        WindowPaintData data;
//        data.opacity = w->opacity();
        WrapperEffect wrapper;
        effects->paintWindow( w, PAINT_TRANSLUCENT, d.region, data, &wrapper );
        }
    }

// the function that'll be eventually called by wrapper.performPaintWindow() above
void SceneOpenGL::WrapperEffect::paintWindow( Scene::Window* w, int mask, QRegion region, WindowPaintData& data )
    {
    static_cast< Window* >( w )->paint( region, mask );
    }

void SceneOpenGL::paintBackground( QRegion )
    {
// TODO?
    }

void SceneOpenGL::windowAdded( Toplevel* c )
    {
    assert( !windows.contains( c ));
    windows[ c ] = Window( c );
    }

void SceneOpenGL::windowDeleted( Toplevel* c )
    {
    assert( windows.contains( c ));
    windows[ c ].free();
    windows.remove( c );
    }

void SceneOpenGL::windowGeometryShapeChanged( Toplevel* c )
    {
    if( !windows.contains( c )) // this is ok, shape is not valid
        return;                 // by default
    Window& w = windows[ c ];
    w.discardShape();
    w.discardTexture();
    }

void SceneOpenGL::windowOpacityChanged( Toplevel* )
    {
#if 0 // not really needed, windows are painted on every repaint
      // and opacity is used when applying texture, not when
      // creating it
    if( !windows.contains( c )) // this is ok, texture is created
        return;                 // on demand
    Window& w = windows[ c ];
    w.discardTexture();
#endif
    }

//****************************************
// SceneOpenGL::Window
//****************************************

SceneOpenGL::Window::Window( Toplevel* c )
    : Scene::Window( c )
    , texture( 0 )
    , texture_y_inverted( false )
    , bound_pixmap( None )
    , bound_glxpixmap( None )
    {
    }

void SceneOpenGL::Window::free()
    {
    discardTexture();
    }

void SceneOpenGL::Window::bindTexture()
    {
    if( texture != 0 && toplevel->damage().isEmpty()
        && !tfp_mode ) // interestingly this makes tfp slower
        {
        // texture doesn't need updating, just bind it
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        return;
        }
    // TODO cache pixmaps here if possible
    Pixmap window_pix = toplevel->createWindowPixmap();
    Pixmap pix = window_pix;
    // HACK
    // When a window uses ARGB visual and has a decoration, the decoration
    // does use ARGB visual. When converting such window to a texture
    // the alpha for the decoration part is broken for some reason (undefined?).
    // I wasn't lucky converting KWin to use ARGB visuals for decorations,
    // so instead simply set alpha in those parts to opaque.
    // Without ALPHA_CLEAR_COPY the setting is done directly in the window
    // pixmap, which seems to be ok, but let's not risk trouble right now.
    // TODO check if this isn't a performance problem and how it can be done better
    Client* c = dynamic_cast< Client* >( toplevel );
    bool alpha_clear = c != NULL && c->hasAlpha() && !c->noBorder();
#define ALPHA_CLEAR_COPY
#ifdef ALPHA_CLEAR_COPY
    if( alpha_clear )
        {
        Pixmap p2 = XCreatePixmap( display(), pix, c->width(), c->height(), 32 );
        GC gc = XCreateGC( display(), pix, 0, NULL );
        XCopyArea( display(), pix, p2, gc, 0, 0, c->width(), c->height(), 0, 0 );
        pix = p2;
        XFreeGC( display(), gc );
        }
#endif
    if( alpha_clear )
        {
        XGCValues gcv;
        gcv.foreground = 0xff000000;
        gcv.plane_mask = 0xff000000;
        GC gc = XCreateGC( display(), pix, GCPlaneMask | GCForeground, &gcv );
        XFillRectangle( display(), pix, gc, 0, 0, c->width(), c->clientPos().y());
        XFillRectangle( display(), pix, gc, 0, 0, c->clientPos().x(), c->height());
        int tw = c->clientPos().x() + c->clientSize().width();
        int th = c->clientPos().y() + c->clientSize().height();
        XFillRectangle( display(), pix, gc, 0, th, c->width(), c->height() - th );
        XFillRectangle( display(), pix, gc, tw, 0, c->width() - tw, c->height());
        XFreeGC( display(), gc );
        glXWaitX();
        }
    if( tfp_mode )
        {
        if( texture == None )
            glGenTextures( 1, &texture );
        if( bound_pixmap != None )
            {
            glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
            glXDestroyGLXPixmap( display(), bound_glxpixmap );
            XFreePixmap( display(), bound_pixmap );
            }
        static const int attrs[] =
            {
            GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
            None
            };
        bound_pixmap = pix;
        bound_glxpixmap = glXCreatePixmap( display(), fbcdrawable, pix, attrs );
        int value;
        glXGetFBConfigAttrib( display(), fbcdrawable, GLX_Y_INVERTED_EXT, &value );
        // this is swapped in order to get a conversion of OpenGL coordinates
        // (binding to a texture is not affected by transforming the OpenGL scene)
        texture_y_inverted = value ? false : true;
        glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
        glXBindTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT, NULL );
        }
    else
        {
        GLXDrawable pixmap = glXCreatePixmap( display(), fbcdrawable, pix, NULL );
        glXMakeContextCurrent( display(), pixmap, pixmap, context );
        glReadBuffer( GL_FRONT );
        glDrawBuffer( GL_FRONT );
        if( texture == None )
            {
            glGenTextures( 1, &texture );
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            texture_y_inverted = true; // conversion to OpenGL coordinates
            glCopyTexImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                toplevel->hasAlpha() ? GL_RGBA : GL_RGB,
                0, 0, toplevel->width(), toplevel->height(), 0 );
            }
        else
            {
            glBindTexture( GL_TEXTURE_RECTANGLE_ARB, texture );
            if( !toplevel->damage().isEmpty())
                {
                foreach( QRect r, toplevel->damage().rects())
                    {
                    // convert to OpenGL coordinates (this is mapping
                    // the pixmap to a texture, this is not affected
                    // by transforming the OpenGL scene)
                    int gly = toplevel->height() - r.y() - r.height();
                    texture_y_inverted = true;
                    glCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_ARB, 0,
                        r.x(), gly, r.x(), gly, r.width(), r.height());
                    }
                }
            }
        // the pixmap is no longer needed, the texture will be updated
        // only when the window changes anyway, so no need to cache
        // the pixmap
        glXDestroyPixmap( display(), pixmap );
        XFreePixmap( display(), pix );
        if( root_db )
            glDrawBuffer( GL_BACK );
        glXMakeContextCurrent( display(), glxroot, glxroot, context );
        }
#ifdef ALPHA_CLEAR_COPY
    if( alpha_clear )
        XFreePixmap( display(), window_pix );
#endif
    }

void SceneOpenGL::Window::discardTexture()
    {
    if( texture != 0 )
        {
        if( tfp_mode )
            {
            glXReleaseTexImageEXT( display(), bound_glxpixmap, GLX_FRONT_LEFT_EXT );
            glXDestroyGLXPixmap( display(), bound_glxpixmap );
            XFreePixmap( display(), bound_pixmap );
            bound_pixmap = None;
            bound_glxpixmap = None;
            }
        glDeleteTextures( 1, &texture );
        }
    texture = 0;
    }

static void quadPaint( int x1, int y1, int x2, int y2, bool invert_y )
    {
    glTexCoord2i( x1, invert_y ? y2 : y1 );
    glVertex2i( x1, y1 );
    glTexCoord2i( x2, invert_y ? y2 : y1 );
    glVertex2i( x2, y1 );
    glTexCoord2i( x2, invert_y ? y1 : y2 );
    glVertex2i( x2, y2 );
    glTexCoord2i( x1, invert_y ? y1 : y2 );
    glVertex2i( x1, y2 );
    }

void SceneOpenGL::Window::paint( QRegion region, int mask )
    {
    if( mask & ( PAINT_OPAQUE | PAINT_TRANSLUCENT ))
        {}
    else if( mask & PAINT_OPAQUE )
        {
        if( !isOpaque())
            return;
        }
    else if( mask & PAINT_TRANSLUCENT )
        {
        if( isOpaque())
            return;
        }
    // paint only requested areas
    if( region != infiniteRegion()) // avoid integer overflow
        region.translate( -x(), -y());
    region &= shape();
    if( region.isEmpty())
        return;
    bindTexture();
    glPushMatrix();
    glTranslatef( x(), y(), 0 );
    bool was_blend = glIsEnabled( GL_BLEND );
    if( !isOpaque())
        {
        glEnable( GL_BLEND );
        glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        }
    if( toplevel->opacity() != 1.0 )
        {
        if( toplevel->hasAlpha())
            {
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
            glColor4f( toplevel->opacity(), toplevel->opacity(), toplevel->opacity(),
                toplevel->opacity());
            }
        else
            {
            float constant_alpha[] = { 0, 0, 0, toplevel->opacity() };
            glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SRC0_RGB, GL_TEXTURE );
            glTexEnvi( GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE );
            glTexEnvi( GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_CONSTANT );
            glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, constant_alpha );
            }
        }
    glEnable( GL_TEXTURE_RECTANGLE_ARB );
    glBegin( GL_QUADS );
    foreach( QRect r, region.rects())
        {
        quadPaint( r.x(), r.y(), r.x() + r.width(), r.y() + r.height(),
            texture_y_inverted );
        }
    glEnd();
    glPopMatrix();
    if( toplevel->opacity() != 1.0 )
        {
        glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
        glColor4f( 0, 0, 0, 0 );
        }
    if( !was_blend )
        glDisable( GL_BLEND );
    glDisable( GL_TEXTURE_RECTANGLE_ARB );
    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, 0 );
    }

} // namespace
