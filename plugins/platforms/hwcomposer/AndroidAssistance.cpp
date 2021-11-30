//
// Created by lijing x on 2021/10/20.
//

#include "AndroidAssistance.h"
#include "hwcomposer_backend.h"

#include <iostream>
#include <unistd.h>
#include <dlfcn.h>
#include <QDebug>

using namespace KWin;

AndroidAssistance::AndroidAssistance(const std::string &file_path) {
    m_handle = dlopen(file_path.c_str(), RTLD_LAZY);
    if (m_handle == nullptr) {
        qDebug() << QString("open %1 failed err: %2").arg(file_path.c_str()).arg(errno);
        return;
    }
}

void AndroidAssistance::setCallback(const HardwareInfo_t info) {
    qDebug() << __func__;
    using HardwareInfoCallback = std::function<HardwareInfo_t()>;
    void (*set_cb)(HardwareInfoCallback);
    set_cb = (void (*)(HardwareInfoCallback)) dlsym(m_handle, "SetHardwareInfoCallback");
    if (set_cb == nullptr) {
        qDebug() << QString("can't find SetHardwareInfoCallback");
        return;
    }

    set_cb([info]() {
        return info;
    });
}

std::string AndroidAssistance::AndroidCompatibleExist() {
    qDebug() << __func__;
    const std::string lib_name = "libhwcomposer_proxy_d.so";
    std::vector <std::string> path_list;
    path_list.push_back("/lib/");
    path_list.push_back("/usr/lib/aarch64-linux-gnu/");
    for (auto path : path_list) {
        std::string full_path = path + lib_name;
        if (access(full_path.c_str(), F_OK) == 0) {
            return full_path;
        }
    }

    return "";
}

void AndroidAssistance::wakeupVsync() {
    if (m_handle == nullptr) {
        return;
    }

    void (*wakeup)();
    wakeup = (void (*)()) dlsym(m_handle, "VsyncWakeUp");
    if (wakeup == nullptr) {
        qDebug() << QString("can't find VsyncWakeUp");
        return;
    }

    wakeup();
}
