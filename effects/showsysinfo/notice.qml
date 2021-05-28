/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
Rectangle {
      width: 100
      height: 100
      color: "#A0000000"
      border.width: 0
      radius: 5
    
    Text {
        anchors.centerIn : parent
        color: "#FFFFFF"
        text: i18n("Press again to close the window")
    }
}

