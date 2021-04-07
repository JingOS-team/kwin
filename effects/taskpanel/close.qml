/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
Image {
    property string btn_img:"/usr/share/kwin_icons/task/jt_close_normal.png"
    width: 34
    height: 34
    source: btn_img
    MouseArea {
          anchors.fill: parent
          onClicked: {  
              window.kill()
          }
    }
}

