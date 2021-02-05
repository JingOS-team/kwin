/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KWIN_SWITCHWINDOWS_CONFIG_H
#define KWIN_SWITCHWINDOWS_CONFIG_H

#include <kcmodule.h>

#include "ui_switchwindows_config.h"


namespace KWin
{

class SwitchWindowsEffectConfigForm : public QWidget, public Ui::SwitchWindowsEffectConfigForm
{
    Q_OBJECT
public:
    explicit SwitchWindowsEffectConfigForm(QWidget* parent);
};

class SwitchWindowsEffectConfig : public KCModule
{
    Q_OBJECT
public:
    explicit SwitchWindowsEffectConfig(QWidget* parent = nullptr, const QVariantList& args = QVariantList());

public Q_SLOTS:
    void save() override;

private:
    SwitchWindowsEffectConfigForm* m_ui;
};

} // namespace

#endif
