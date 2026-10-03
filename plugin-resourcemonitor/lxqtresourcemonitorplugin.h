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

#ifndef LXQTRESOURCEMONITORPLUGIN_H
#define LXQTRESOURCEMONITORPLUGIN_H

#include "../panel/ilxqtpanelplugin.h"

#include <QObject>

#include <memory>

class LXQtResourceMonitor;

class LXQtResourceMonitorPlugin final : public QObject, public ILXQtPanelPlugin
{
    Q_OBJECT

public:
    explicit LXQtResourceMonitorPlugin(const ILXQtPanelPluginStartupInfo &startupInfo);
    ~LXQtResourceMonitorPlugin() override;

    [[nodiscard]] ILXQtPanelPlugin::Flags flags() const override
    {
        return PreferRightAlignment | HaveConfigDialog;
    }

    [[nodiscard]] QWidget *widget() override;
    [[nodiscard]] QString themeId() const override { return QStringLiteral("ResourceMonitor"); }
    [[nodiscard]] bool isSeparate() const override { return true; }
    [[nodiscard]] QDialog *configureDialog() override;

protected:
    void settingsChanged() override;

private:
    std::unique_ptr<QWidget> m_widget;
    LXQtResourceMonitor *m_content{nullptr}; // QObject parent owns this child widget.
};

class LXQtResourceMonitorPluginLibrary final : public QObject, public ILXQtPanelPluginLibrary
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "lxqt.org/Panel/PluginInterface/3.0")
    Q_INTERFACES(ILXQtPanelPluginLibrary)

public:
    [[nodiscard]] ILXQtPanelPlugin *instance(const ILXQtPanelPluginStartupInfo &startupInfo) const override
    {
        return new LXQtResourceMonitorPlugin(startupInfo);
    }
};

#endif // LXQTRESOURCEMONITORPLUGIN_H
