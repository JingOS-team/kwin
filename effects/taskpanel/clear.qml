/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
import QtQuick 2.0
import QtQuick.Layouts 1.2
/*import org.kde.plasma.components 3.0 as Plasma

Plasma.Button {
    objectName: "clearButton"
    enabled: effects.desktops < 20
    icon.name: "jclose-clear"
    
}
*/
/*Rectangle {
      width: 100
      height: 100
      border.width: 0
      radius: 10
  }
*/
    Image {
      width: 120
      height: 120
      source: '/usr/share/kwin_icons/task/jt_clear_normal.png'
      MouseArea {
          anchors.fill: parent
          onClicked: { taskpanel.clearWindows() }
      }
    }
