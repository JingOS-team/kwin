#.rst:
# Findlibhybris
# -------
#
# Try to find libhybris on a Unix system.

#=============================================================================
# SPDX-FileCopyrightText: 2015 Martin Gräßlin <mgraesslin@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by Findlibhybris.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use Findlibhybris.cmake")
endif()

if(NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PKG_libhardware QUIET libhardware)
    pkg_check_modules(PKG_androidheaders QUIET android-headers)
    pkg_check_modules(PKG_hwcomposerwindow QUIET hwcomposer-egl)
    pkg_check_modules(PKG_hybriseglplatform QUIET hybris-egl-platform)
    pkg_check_modules(PKG_surface_flinger QUIET libsf)
    pkg_check_modules(PKG_hwc2 QUIET libhwc2)
    MESSAGE( STATUS "===================================Findlibhybris START===========================================")
    ##############################################
    # libhardware
    set(libhardware_DEFINITIONS ${PKG_libhardware_CFLAGS_OTHER})
    set(libhardware_VERSION ${PKG_libhardware_VERSION})

    find_library(libhardware_LIBRARY
        NAMES
            libhardware.so
        HINTS
            ${PKG_libhardware_LIBRARY_DIRS}
    )
    find_path(libhardware_INCLUDE_DIR
        NAMES
            android-version.h
        HINTS
            ${PKG_androidheaders_INCLUDE_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libhardware
        FOUND_VAR
            libhardware_FOUND
        REQUIRED_VARS
            libhardware_LIBRARY
            libhardware_INCLUDE_DIR
        VERSION_VAR
            libhardware_VERSION
    )

    if(libhardware_FOUND AND NOT TARGET libhybris::libhardware)
        add_library(libhybris::libhardware UNKNOWN IMPORTED)
        set_target_properties(libhybris::libhardware PROPERTIES
            IMPORTED_LOCATION "${libhardware_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libhardware_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libhardware_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(libhardware_LIBRARY libhardware_INCLUDE_DIR)
    MESSAGE( STATUS "   =================")
    MESSAGE( STATUS "    PKG_androidheaders_INCLUDE_DIRS    =  ${PKG_androidheaders_INCLUDE_DIRS}")
    MESSAGE( STATUS "    PKG_libhardware_CFLAGS_OTHER       =  ${PKG_libhardware_CFLAGS_OTHER}")
    MESSAGE( STATUS "    PKG_libhardware_VERSION            =  ${PKG_libhardware_VERSION}")
    MESSAGE( STATUS "    PKG_libhardware_LIBRARY_DIRS       =  ${PKG_libhardware_LIBRARY_DIRS}")
    MESSAGE( STATUS "    libhardware_LIBRARY                =  ${libhardware_LIBRARY}")
    MESSAGE( STATUS "    libhardware_DEFINITIONS            =  ${libhardware_DEFINITIONS}")
    MESSAGE( STATUS "    libhardware_INCLUDE_DIR            =  ${libhardware_INCLUDE_DIR}")
    MESSAGE( STATUS "   =================")
    # libhardware
    ###############################################################################################


    ##############################################
    # hwcomposerWindow
    set(libhwcomposer_DEFINITIONS ${PKG_hwcomposerwindow_CFLAGS_OTHER})
    set(libhwcomposer_VERSION ${PKG_hwcomposerwindow_VERSION})

    find_library(libhwcomposer_LIBRARY
        NAMES
            libhybris-hwcomposerwindow.so
        HINTS
            ${PKG_hwcomposerwindow_LIBRARY_DIRS}

    )

    find_path(libhwcomposer_INCLUDE_DIR
        NAMES
            hwcomposer_window.h
        HINTS
            ${PKG_hwcomposerwindow_INCLUDE_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libhwcomposer
        FOUND_VAR
            libhwcomposer_FOUND
        REQUIRED_VARS
            libhwcomposer_LIBRARY
            libhwcomposer_INCLUDE_DIR
        VERSION_VAR
            libhwcomposer_VERSION
    )

    if(libhwcomposer_FOUND AND NOT TARGET libhybris::hwcomposer)
        add_library(libhybris::hwcomposer UNKNOWN IMPORTED)
        set_target_properties(libhybris::hwcomposer PROPERTIES
            IMPORTED_LOCATION "${libhwcomposer_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libhardware_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libhwcomposer_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(libhwcomposer_LIBRARY libhwcomposer_INCLUDE_DIR)

    MESSAGE( STATUS "   =================")
    MESSAGE( STATUS "    PKG_hwcomposerwindow_CFLAGS_OTHER  =  ${PKG_hwcomposerwindow_CFLAGS_OTHER}")
    MESSAGE( STATUS "    PKG_hwcomposerwindow_VERSION       =  ${PKG_hwcomposerwindow_VERSION}")
    MESSAGE( STATUS "    PKG_hwcomposerwindow_LIBRARY_DIRS  =  ${PKG_hwcomposerwindow_LIBRARY_DIRS}")
    MESSAGE( STATUS "    PKG_hwcomposerwindow_INCLUDE_DIRS  =  ${PKG_hwcomposerwindow_INCLUDE_DIRS}")
    MESSAGE( STATUS "    libhwcomposer_LIBRARY              =  ${libhwcomposer_LIBRARY}")
    MESSAGE( STATUS "    libhwcomposer_INCLUDE_DIR          =  ${libhwcomposer_INCLUDE_DIR}")
    MESSAGE( STATUS "   =================")
    # hwcomposerWindow
    ###############################################################################################


    ##############################################
    # surface_flinger
    set(libsurface_flinger_DEFINITIONS ${PKG_surface_flinger_CFLAGS_OTHER})
    set(libsurface_flinger_VERSION     ${PKG_surface_flinger_VERSION})
    find_library(libsurface_flinger_LIBRARY
        NAMES
            libsf.so
        HINTS
            ${PKG_surface_flinger_LIBRARY_DIRS}
    )

    find_path(libsurface_flinger_INCLUDE_DIRS
        NAMES
            hybris/surface_flinger/surface_flinger_compatibility_layer.h
        HINTS
            ${PKG_surface_flinger_INCLUDE_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libsurface_flinger
        FOUND_VAR
            libsurface_flinger_FOUND
        REQUIRED_VARS
            libsurface_flinger_LIBRARY
            libsurface_flinger_INCLUDE_DIRS
        VERSION_VAR
            libsurface_flinger_VERSION
    )

    if(libsurface_flinger_FOUND AND NOT TARGET libhybris::sf)
        add_library(libhybris::sf UNKNOWN IMPORTED)
        set_target_properties(libhybris::sf PROPERTIES
            IMPORTED_LOCATION "${libsurface_flinger_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libsurface_flinger_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libsurface_flinger_INCLUDE_DIRS}"
        )
    endif()
    mark_as_advanced(libsurface_flinger_LIBRARY libsurface_flinger_INCLUDE_DIRS)
    MESSAGE( STATUS "   ==============================================================================")
    MESSAGE( STATUS "    PKG_surface_flinger_VERSION        =  ${PKG_surface_flinger_VERSION}")
    MESSAGE( STATUS "    PKG_surface_flinger_CFLAGS_OTHER   =  ${PKG_surface_flinger_CFLAGS_OTHER}")
    MESSAGE( STATUS "    libsurface_flinger_LIBRARY         =  ${libsurface_flinger_LIBRARY}")
    MESSAGE( STATUS "    libsurface_flinger_INCLUDE_DIRS     =  ${libsurface_flinger_INCLUDE_DIRS}")
    MESSAGE( STATUS "    PKG_surface_flinger_INCLUDE_DIRS   =  ${PKG_surface_flinger_INCLUDE_DIRS}")
    MESSAGE( STATUS "    PKG_surface_flinger_LIBRARY_DIRS   =  ${PKG_surface_flinger_LIBRARY_DIRS}")
    MESSAGE( STATUS "   ==============================================================================")
    # surface_flinger
    ###############################################################################################

    ##############################################
    # hwcomposer
    set(libhwc2_DEFINITIONS ${PKG_hwc2_CFLAGS_OTHER})
    set(libhwc2_VERSION     ${PKG_hwc2_VERSION})
    find_library(libhwc2_LIBRARY
        NAMES
            libhwc2.so
        HINTS
            ${PKG_hwc2_LIBRARY_DIRS}
    )

    find_library(libsync_LIBRARY
        NAMES
            libsync.so
        HINTS
            ${PKG_hwc2_LIBRARY_DIRS}
    )


    find_path(libhwc2_INCLUDE_DIRS
        NAMES
            hybris/hwc2/hwc2_compatibility_layer.h
        HINTS
            ${PKG_hwc2_INCLUDE_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libhwc2
        FOUND_VAR
            libhwc2_FOUND
        REQUIRED_VARS
            libhwc2_LIBRARY
            libhwc2_INCLUDE_DIRS
        VERSION_VAR
            libhwc2_VERSION
    )

    if(libhwc2_FOUND AND NOT TARGET libhybris::hwc2 AND NOT TARGET libhybris::sync)
        add_library(libhybris::hwc2 UNKNOWN IMPORTED)
        set_target_properties(libhybris::hwc2 PROPERTIES
            IMPORTED_LOCATION "${libhwc2_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libhwc2_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libhwc2_INCLUDE_DIRS}"
        )

        add_library(libhybris::sync UNKNOWN IMPORTED)
        set_target_properties(libhybris::sync PROPERTIES
            IMPORTED_LOCATION "${libsync_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libhwc2_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libhwc2_INCLUDE_DIR}"
        )
    endif()
    mark_as_advanced(libhwc2_LIBRARY libhwc2_INCLUDE_DIRS)
    MESSAGE( STATUS "   ==============================================================================")
    MESSAGE( STATUS "    PKG_hwc2_VERSION        =  ${PKG_hwc2_VERSION}")
    MESSAGE( STATUS "    PKG_hwc2_CFLAGS_OTHER   =  ${PKG_hwc2_CFLAGS_OTHER}")
    MESSAGE( STATUS "    libhwc2_LIBRARY         =  ${libhwc2_LIBRARY}")
    MESSAGE( STATUS "    libhwc2_INCLUDE_DIRS     =  ${libhwc2_INCLUDE_DIRS}")
    MESSAGE( STATUS "    PKG_hwc2_INCLUDE_DIRS   =  ${PKG_hwc2_INCLUDE_DIRS}")
    MESSAGE( STATUS "    PKG_hwc2_LIBRARY_DIRS   =  ${PKG_hwc2_LIBRARY_DIRS}")
    MESSAGE( STATUS "   ==============================================================================")
    # hwcomposer
    ###############################################################################################


    ##############################################
    # hybriseglplatform
    set(hybriseglplatform_DEFINITIONS ${PKG_hybriseglplatform_CFLAGS_OTHER})
    set(hybriseglplatform_VERSION ${PKG_hybriseglplatform_VERSION})

    find_library(hybriseglplatform_LIBRARY
        NAMES
            libhybris-eglplatformcommon.so
        HINTS
            ${PKG_hybriseglplatform_LIBRARY_DIRS}
    )
    find_path(hybriseglplatform_INCLUDE_DIR
        NAMES
            eglplatformcommon.h
        HINTS
            ${PKG_hybriseglplatform_INCLUDE_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(hybriseglplatform
        FOUND_VAR
            hybriseglplatform_FOUND
        REQUIRED_VARS
            hybriseglplatform_LIBRARY
            hybriseglplatform_INCLUDE_DIR
        VERSION_VAR
            hybriseglplatform_VERSION
    )

    if(hybriseglplatform_FOUND AND NOT TARGET libhybris::hybriseglplatform)
        add_library(libhybris::hybriseglplatform UNKNOWN IMPORTED)
        set_target_properties(libhybris::hybriseglplatform PROPERTIES
            IMPORTED_LOCATION "${hybriseglplatform_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${hybriseglplatform_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${hybriseglplatform_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(hybriseglplatform_LIBRARY hybriseglplatform_INCLUDE_DIR)

    MESSAGE( STATUS "   =================")
    MESSAGE( STATUS "    PKG_hybriseglplatform_CFLAGS_OTHER =  ${PKG_hybriseglplatform_CFLAGS_OTHER}")
    MESSAGE( STATUS "    PKG_hybriseglplatform_VERSION      =  ${PKG_hybriseglplatform_VERSION}")
    MESSAGE( STATUS "    PKG_hybriseglplatform_LIBRARY_DIRS =  ${PKG_hybriseglplatform_LIBRARY_DIRS}")
    MESSAGE( STATUS "    PKG_hybriseglplatform_INCLUDE_DIRS =  ${PKG_hybriseglplatform_INCLUDE_DIRS}")
    MESSAGE( STATUS "    hybriseglplatform_LIBRARY          =  ${hybriseglplatform_LIBRARY}")
    MESSAGE( STATUS "    hybriseglplatform_DEFINITIONS      =  ${hybriseglplatform_DEFINITIONS}")
    MESSAGE( STATUS "    hybriseglplatform_INCLUDE_DIR      =  ${hybriseglplatform_INCLUDE_DIR}")
    MESSAGE( STATUS "   =================")
    # hybriseglplatform
    ###############################################################################################

    if(libhardware_FOUND AND libhwcomposer_FOUND AND hybriseglplatform_FOUND AND libsurface_flinger_FOUND)
        set(libhybris_FOUND TRUE)
    else()
        set(libhybris_FOUND FALSE)
    endif()

else()
    message(STATUS "Findlibhardware.cmake cannot find libhybris on Windows systems.")
    set(libhybris_FOUND FALSE)
endif()

MESSAGE( STATUS "===================================Findlibhybris END===========================================")

include(FeatureSummary)
set_package_properties(libhybris PROPERTIES
    URL "https://github.com/libhybris/libhybris"
    DESCRIPTION "libhybris allows to run bionic-based HW adaptations in glibc systems."
)
