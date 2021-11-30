set(SURFACEFLINGER_SOURCES
    egl_surfaceflinger_backend.cpp
    logging.cpp
    screens_surfaceflinger.cpp
    surfaceflinger_backend.cpp
    surfaceflinger_screeninfo.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/platformsupport/scenes/opengl)
add_library(KWinWaylandSurfaceFlingerBackend MODULE ${SURFACEFLINGER_SOURCES})
set_target_properties(KWinWaylandSurfaceFlingerBackend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/org.kde.kwin.waylandbackends/")
target_link_libraries(KWinWaylandSurfaceFlingerBackend
    kwin
    SceneOpenGLBackend
    libhybris::libhardware
    libhybris::sf
    libhybris::hybriseglplatform
)

install(
    TARGETS
        KWinWaylandSurfaceFlingerBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
