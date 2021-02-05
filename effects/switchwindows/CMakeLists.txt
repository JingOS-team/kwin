#######################################
# Config
set(kwin_switchwindows_config_SRCS switchwindows_config.cpp)
ki18n_wrap_ui(kwin_switchwindows_config_SRCS switchwindows_config.ui)
kconfig_add_kcfg_files(kwin_switchwindows_config_SRCS switchwindowsconfig.kcfgc)

add_library(kwin_switchwindows_config MODULE ${kwin_switchwindows_config_SRCS})

target_link_libraries(kwin_switchwindows_config
    KF5::ConfigWidgets
    KF5::I18n
    Qt5::DBus
    KWinEffectsInterface
)

kcoreaddons_desktop_to_json(kwin_switchwindows_config switchwindows_config.desktop SERVICE_TYPES kcmodule.desktop)

install(
    TARGETS
        kwin_switchwindows_config
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/kwin/effects/configs
)
