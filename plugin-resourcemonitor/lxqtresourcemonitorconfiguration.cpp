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

#include "lxqtresourcemonitorconfiguration.h"
#include "ui_lxqtresourcemonitorconfiguration.h"

namespace
{
constexpr int DefaultWidgetWidth = 78;
constexpr int DefaultUpdateIntervalMs = 1000;
} // namespace

LXQtResourceMonitorConfiguration::LXQtResourceMonitorConfiguration(PluginSettings *settings, QWidget *parent)
    : LXQtPanelPluginConfigDialog(settings, parent)
    , m_ui(new Ui::LXQtResourceMonitorConfiguration)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("ResourceMonitorConfigurationWindow"));
    m_ui->setupUi(this);

    fillBarOrientations();

    connect(m_ui->buttons,
            &QDialogButtonBox::clicked,
            this,
            &LXQtResourceMonitorConfiguration::dialogButtonsAction);

    loadSettings();

    connect(m_ui->showTextCB,
            &QCheckBox::toggled,
            this,
            &LXQtResourceMonitorConfiguration::showTextChanged);
    connect(m_ui->widgetWidthSB,
            &QSpinBox::valueChanged,
            this,
            &LXQtResourceMonitorConfiguration::widgetWidthChanged);
    connect(m_ui->updateIntervalSpinBox,
            &QDoubleSpinBox::valueChanged,
            this,
            &LXQtResourceMonitorConfiguration::updateIntervalChanged);
    connect(m_ui->barOrientationCOB,
            &QComboBox::currentIndexChanged,
            this,
            &LXQtResourceMonitorConfiguration::barOrientationChanged);
}

LXQtResourceMonitorConfiguration::~LXQtResourceMonitorConfiguration()
{
    delete m_ui;
}

void LXQtResourceMonitorConfiguration::fillBarOrientations()
{
    m_ui->barOrientationCOB->addItem(tr("Bottom up"), QStringLiteral("bottomUp"));
    m_ui->barOrientationCOB->addItem(tr("Top down"), QStringLiteral("topDown"));
    m_ui->barOrientationCOB->addItem(tr("Left to right"), QStringLiteral("leftRight"));
    m_ui->barOrientationCOB->addItem(tr("Right to left"), QStringLiteral("rightLeft"));
}

void LXQtResourceMonitorConfiguration::loadSettings()
{
    m_lockSettingChanges = true;

    m_ui->showTextCB->setChecked(settings().value(QStringLiteral("showText"), true).toBool());
    m_ui->widgetWidthSB->setValue(settings().value(QStringLiteral("widgetWidth"), DefaultWidgetWidth).toInt());
    m_ui->updateIntervalSpinBox->setValue(
        settings().value(QStringLiteral("updateInterval"), DefaultUpdateIntervalMs).toInt() / 1000.0);

    int orientationIndex = m_ui->barOrientationCOB->findData(
        settings().value(QStringLiteral("barOrientation"), QStringLiteral("bottomUp")));
    if (orientationIndex < 0)
        orientationIndex = 0;
    m_ui->barOrientationCOB->setCurrentIndex(orientationIndex);

    m_lockSettingChanges = false;
}

void LXQtResourceMonitorConfiguration::showTextChanged(bool value)
{
    if (!m_lockSettingChanges)
        settings().setValue(QStringLiteral("showText"), value);
}

void LXQtResourceMonitorConfiguration::widgetWidthChanged(int value)
{
    if (!m_lockSettingChanges)
        settings().setValue(QStringLiteral("widgetWidth"), value);
}

void LXQtResourceMonitorConfiguration::updateIntervalChanged(double value)
{
    if (!m_lockSettingChanges)
        settings().setValue(QStringLiteral("updateInterval"), qRound(value * 1000.0));
}

void LXQtResourceMonitorConfiguration::barOrientationChanged(int index)
{
    if (!m_lockSettingChanges && index >= 0)
    {
        settings().setValue(QStringLiteral("barOrientation"),
                            m_ui->barOrientationCOB->itemData(index).toString());
    }
}
