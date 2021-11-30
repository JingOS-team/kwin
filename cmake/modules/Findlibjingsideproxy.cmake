#.rst:
# Findperfetto
# -------
#=============================================================================
#2020 dengbaona <dengbaoan@jingos.com>
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by Findjingsideproxy.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use Findjingsideproxy.cmake")
endif()

if(NOT WIN32)
    MESSAGE( STATUS "===================================Findjingsideproxy START===========================================")
    find_package(PkgConfig)
    pkg_check_modules(PKG_libjingsideproxy QUIET libjingsideproxy)
    ##############################################
    # perfetto
    find_library(libjingsideproxy_LIBRARY
        NAMES
            libjing_android_proxy.so
        HINTS
            ${PKG_jingsideproxy_LIBRARY_DIRS}
    )
    find_path(libjingsideproxy_INCLUDE_DIR
        NAMES
            JingAndroidProxy.h
        HINTS
	/usr/include/jingsideproxy/
	#   ${PKG_jingsideproxy_INCLUDE_DIRS}
    )
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(libjingsideproxy
        FOUND_VAR
            libjingsideproxy_FOUND
        REQUIRED_VARS
            libjingsideproxy_LIBRARY
            libjingsideproxy_INCLUDE_DIR
        VERSION_VAR
            libjingsideproxy_VERSION
    )

MESSAGE( STATUS "====2==" ${libjingsideproxy_LIBRARY})
MESSAGE( STATUS "===1===" ${libjingsideproxy_INCLUDE_DIR})

else()
    message(STATUS "Findjingsideproxy.cmake cannot find libjingsideproxy on Windows systems.")
    set(libjingsideproxy_FOUND FALSE)
endif()

MESSAGE( STATUS "===================================Findjingsideproxy END===========================================" ${libjingsideproxy_FOUND})

