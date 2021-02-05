/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2020 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "showsysinfo.h"

#include <QQuickItem>
#include <QQmlContext>

namespace KWin
{
ShowSysInfo::ShowSysInfo()
{
    _noticeView = new EffectQuickScene(this);

    connect(_noticeView, &EffectQuickView::repaintNeeded, this, []() {
        effects->addRepaintFull();
    });

    _noticeView->setSource(QUrl(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/effects/showsysinfo/notice.qml"))));

    QQuickItem *rootItem = _noticeView->rootItem();
    if (!rootItem) {
        delete _noticeView;
        return;
    }

    _noticeView->show();
    _noticeView->rootItem()->setOpacity(0.8);
    _noticeView->setGeometry(QRect(0, 0, 300, 300));
}

void ShowSysInfo::postPaintScreen()
{
    if (effects->activeWindow()) {
        QPoint sCenter = effects->clientArea(WorkArea, effects->activeWindow()).center();
        QRect rect(0, 0, 410, 50);
        rect.moveCenter(sCenter);
        _noticeView->setGeometry(rect);
        _noticeView->rootItem()->setOpacity(0.8);
        effects->renderEffectQuickView(_noticeView);
    }

    effects->postPaintScreen();
}

bool ShowSysInfo::isActive() const
{
    return effects->showCloseNotice() && !effects->isScreenLocked();
}

bool ShowSysInfo::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

}
