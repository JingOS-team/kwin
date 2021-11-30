//
// Created by lijing x on 2021/10/20.
//

#ifndef KWIN_ANDROIDASSISTANCE_H
#define KWIN_ANDROIDASSISTANCE_H

#include <functional>
#include <QScopedPointer>

namespace KWin {
    class AndroidAssistance {
    public:
        /**
         * @brief get hardware info from kwin_wayland
        */
        typedef struct HardwareInfo {
            int width_pixel;
            int height_pixel;
            int dpi;
            int vsync_period;
        } HardwareInfo_t;

        explicit AndroidAssistance(const std::string &file_path);

        void setCallback(const HardwareInfo_t info);
        void wakeupVsync();

        // using HardwareInfoCallback = std::function<HardwareInfo_t()>;
        HardwareInfo_t DisplayInfoCallback();

        static std::string AndroidCompatibleExist();

    private:
        void *m_handle;
    };
}
#endif //KWIN_ANDROIDASSISTANCE_H
