/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "lxqtresourcemonitorplugin.h"

#include "lxqtresourcemonitor.h"
#include "lxqtresourcemonitorconfiguration.h"

#include <QVBoxLayout>
#include <QWidget>

LXQtResourceMonitorPlugin::LXQtResourceMonitorPlugin(const ILXQtPanelPluginStartupInfo &startupInfo)
    : QObject()
    , ILXQtPanelPlugin(startupInfo)
    , m_widget(std::make_unique<QWidget>())
{
    m_content = new LXQtResourceMonitor(this, m_widget.get());

    auto *layout = new QVBoxLayout(m_widget.get());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_content);
    layout->setStretchFactor(m_content, 1);
}

LXQtResourceMonitorPlugin::~LXQtResourceMonitorPlugin() = default;

QWidget *LXQtResourceMonitorPlugin::widget()
{
    return m_widget.get();
}

QDialog *LXQtResourceMonitorPlugin::configureDialog()
{
    return new LXQtResourceMonitorConfiguration(settings());
}

void LXQtResourceMonitorPlugin::settingsChanged()
{
    if (m_content != nullptr)
        m_content->settingsChanged();
}
