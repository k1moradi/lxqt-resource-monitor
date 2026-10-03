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

#ifndef LXQTRESOURCEMONITORCONFIGURATION_H
#define LXQTRESOURCEMONITORCONFIGURATION_H

#include "../panel/lxqtpanelpluginconfigdialog.h"
#include "../panel/pluginsettings.h"

namespace Ui
{
class LXQtResourceMonitorConfiguration;
}

class LXQtResourceMonitorConfiguration final : public LXQtPanelPluginConfigDialog
{
    Q_OBJECT

public:
    explicit LXQtResourceMonitorConfiguration(PluginSettings *settings, QWidget *parent = nullptr);
    ~LXQtResourceMonitorConfiguration() override;

private slots:
    void loadSettings() override;
    void showTextChanged(bool value);
    void widgetWidthChanged(int value);
    void updateIntervalChanged(double value);
    void barOrientationChanged(int index);

private:
    void fillBarOrientations();

    Ui::LXQtResourceMonitorConfiguration *m_ui;
    bool m_lockSettingChanges{false};
};

#endif // LXQTRESOURCEMONITORCONFIGURATION_H
