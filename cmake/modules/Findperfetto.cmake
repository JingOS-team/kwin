#.rst:
# Findperfetto
# -------
#=============================================================================
#2020 dengbaona <dengbaoan@jingos.com>
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by Findperfetto.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use Findperfetto.cmake")
endif()

if(NOT WIN32)
    MESSAGE( STATUS "===================================Findperfetto START===========================================")
    find_package(PkgConfig)
    pkg_check_modules(PKG_perfetto QUIET libperfetto)
    ##############################################
    # perfetto
    find_library(libperfetto_LIBRARY
        NAMES
            libperfetto-sdk.so
        HINTS
            ${PKG_perfetto_LIBRARY_DIRS}
    )
    find_path(libperfetto_INCLUDE_DIR
        NAMES
            perfetto.h
        HINTS
            ${PKG_perfetto_INCLUDE_DIRS}
    )
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libperfetto
        FOUND_VAR
            libperfetto_FOUND
        REQUIRED_VARS
            libperfetto_LIBRARY
            libperfetto_INCLUDE_DIR
        VERSION_VAR
            libperfetto_VERSION
    )
    if(libperfetto_FOUND AND NOT TARGET libperfetto::sdk)
        add_library(libperfetto::sdk UNKNOWN IMPORTED)
        set_target_properties(libperfetto::sdk PROPERTIES
            IMPORTED_LOCATION "${libperfetto_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${libperfetto_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${libperfetto_INCLUDE_DIR}"
        )
    endif()

    mark_as_advanced(libperfetto_LIBRARY libperfetto_INCLUDE_DIR)
    MESSAGE( STATUS "   =================")
    MESSAGE( STATUS "    libperfetto_LIBRARY_DIRS           =  ${libperfetto_LIBRARY_DIRS}")
    MESSAGE( STATUS "    libperfetto_LIBRARY                =  ${libperfetto_LIBRARY}")
    MESSAGE( STATUS "    libperfetto_DEFINITIONS            =  ${libperfetto_DEFINITIONS}")
    MESSAGE( STATUS "    libperfetto_INCLUDE_DIR            =  ${libperfetto_INCLUDE_DIR}")
    MESSAGE( STATUS "   =================")
    # libperfetto
    ###############################################################################################

    if(libperfetto_FOUND)
        set(libperfetto_FOUND TRUE)
    else()
        set(libperfetto_FOUND FALSE)
    endif()

else()
    message(STATUS "Findlibperfetto.cmake cannot find libperfetto on Windows systems.")
    set(libperfetto_FOUND FALSE)
endif()

MESSAGE( STATUS "===================================Findlibperfetto END===========================================")

include(FeatureSummary)
set_package_properties(libperfetto PROPERTIES
    URL "https://github.com/libperfetto"
    DESCRIPTION "libperfetto allows to run bionic-based HW adaptations in glibc systems."
)
