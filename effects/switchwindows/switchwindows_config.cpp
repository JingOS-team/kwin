/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2021 guoxiang yang <yangguoxiang@jingos.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "switchwindows_config.h"
// KConfigSkeleton
#include "switchwindowsconfig.h"
#include <config-kwin.h>

#include <kwineffects_interface.h>

#include <kconfiggroup.h>
#include <KAboutData>
#include <KPluginFactory>

#include <QVBoxLayout>

K_PLUGIN_FACTORY_WITH_JSON(SwitchWindowsEffectConfigFactory,
                           "switchwindows_config.json",
                           registerPlugin<KWin::SwitchWindowsEffectConfig>();)

namespace KWin
{

SwitchWindowsEffectConfigForm::SwitchWindowsEffectConfigForm(QWidget* parent) : QWidget(parent)
{
    setupUi(this);
}

SwitchWindowsEffectConfig::SwitchWindowsEffectConfig(QWidget* parent, const QVariantList& args) :
    KCModule(parent, args)
{
    m_ui = new SwitchWindowsEffectConfigForm(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    layout->addWidget(m_ui);

    SwitchWindowsConfig::instance(KWIN_CONFIG);
    addConfig(SwitchWindowsConfig::self(), m_ui);

    load();
}

void SwitchWindowsEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("switchwindows"));
}

} // namespace

#include "switchwindows_config.moc"
