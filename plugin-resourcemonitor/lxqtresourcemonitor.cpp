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
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "lxqtresourcemonitor.h"
#include "resourcemonitormath.h"

#include "../panel/ilxqtpanelplugin.h"
#include "../panel/pluginsettings.h"

#include <QDebug>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QStringList>
#include <QTimerEvent>

#include <algorithm>
#include <cmath>

extern "C" {
#include <statgrab.h>
}

#ifdef __sg_public
// libstatgrab 0.90 and newer define this macro.
#define STATGRAB_NEWER_THAN_0_90 1
#endif

namespace
{
constexpr auto BarOrientTopDown = "topDown";
constexpr auto BarOrientLeftRight = "leftRight";
constexpr auto BarOrientRightLeft = "rightLeft";

constexpr int MeterCount = 3;
constexpr int OuterMargin = 1;
constexpr int MeterGap = 1;
constexpr int MeterInnerMargin = 1;
constexpr int TextHorizontalPadding = 1;
constexpr int PreferredTextPixelSize = 10;
constexpr int MinimumTextPixelSize = 7;

} // namespace

LXQtResourceMonitor::LXQtResourceMonitor(ILXQtPanelPlugin *plugin, QWidget *parent)
    : QFrame(parent)
    , m_plugin(plugin)
{
    setObjectName(QStringLiteral("LXQtResourceMonitor"));

    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&m_sizingWidget);

#ifdef STATGRAB_NEWER_THAN_0_90
    const sg_error initResult = sg_init(0);
#else
    const sg_error initResult = sg_init();
#endif
    if (initResult == SG_ERROR_NONE)
    {
        m_statgrabInitialized = true;
        if (sg_drop_privileges() != SG_ERROR_NONE)
            qWarning() << "Resource Monitor: failed to drop libstatgrab privileges";
    }
    else
    {
        qWarning() << "Resource Monitor: libstatgrab initialization failed";
    }

    // Keep the text compact, like the existing CPU Monitor. Pixel sizing is
    // predictable on narrow panel widgets and is reduced further when needed.
    m_font.setPixelSize(PreferredTextPixelSize);

    settingsChanged();
    refreshStats();
}

LXQtResourceMonitor::~LXQtResourceMonitor()
{
    if (m_timerId != -1)
        killTimer(m_timerId);

    if (m_statgrabInitialized)
        sg_shutdown();
}

bool LXQtResourceMonitor::isVerticalBarOrientation() const
{
    return m_barOrientation == BottomUpBar || m_barOrientation == TopDownBar;
}

void LXQtResourceMonitor::setSizes()
{
    // Width is the user-facing footprint of the complete three-meter widget.
    // Height is controlled by the panel.
    m_sizingWidget.setFixedWidth(m_widgetWidth);
    m_sizingWidget.setMinimumHeight(24);
}

void LXQtResourceMonitor::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    setSizes();
    update();
}

QRect LXQtResourceMonitor::meterRect(std::size_t resourceIndexValue) const
{
    const QRect availableRectangle = rect().adjusted(OuterMargin,
                                                      OuterMargin,
                                                      -OuterMargin,
                                                      -OuterMargin);
    if (availableRectangle.isEmpty() || resourceIndexValue >= static_cast<std::size_t>(MeterCount))
        return {};

    if (isVerticalBarOrientation())
    {
        const int usableWidth = std::max(0, availableRectangle.width() - MeterGap * (MeterCount - 1));
        const int baseWidth = usableWidth / MeterCount;
        const int remainder = usableWidth % MeterCount;

        int x = availableRectangle.left();
        for (std::size_t index = 0; index < resourceIndexValue; ++index)
            x += baseWidth + (static_cast<int>(index) < remainder ? 1 : 0) + MeterGap;

        const int width = baseWidth + (static_cast<int>(resourceIndexValue) < remainder ? 1 : 0);
        return QRect(x, availableRectangle.top(), width, availableRectangle.height());
    }

    const int usableHeight = std::max(0, availableRectangle.height() - MeterGap * (MeterCount - 1));
    const int baseHeight = usableHeight / MeterCount;
    const int remainder = usableHeight % MeterCount;

    int y = availableRectangle.top();
    for (std::size_t index = 0; index < resourceIndexValue; ++index)
        y += baseHeight + (static_cast<int>(index) < remainder ? 1 : 0) + MeterGap;

    const int height = baseHeight + (static_cast<int>(resourceIndexValue) < remainder ? 1 : 0);
    return QRect(availableRectangle.left(), y, availableRectangle.width(), height);
}

QRect LXQtResourceMonitor::fillRect(const QRect &meterRectangle, double percent) const
{
    if (meterRectangle.isEmpty())
        return {};

    const QRect innerRectangle = meterRectangle.adjusted(MeterInnerMargin,
                                                          MeterInnerMargin,
                                                          -MeterInnerMargin,
                                                          -MeterInnerMargin);
    if (innerRectangle.isEmpty())
        return {};

    const double boundedPercent = std::clamp(percent, 0.0, 100.0);
    const double fraction = boundedPercent / 100.0;

    if (isVerticalBarOrientation())
    {
        const int filledHeight = std::clamp(static_cast<int>(std::lround(innerRectangle.height() * fraction)),
                                            0,
                                            innerRectangle.height());
        if (filledHeight == 0)
            return {};

        if (m_barOrientation == TopDownBar)
            return QRect(innerRectangle.left(), innerRectangle.top(), innerRectangle.width(), filledHeight);

        return QRect(innerRectangle.left(),
                     innerRectangle.bottom() - filledHeight + 1,
                     innerRectangle.width(),
                     filledHeight);
    }

    const int filledWidth = std::clamp(static_cast<int>(std::lround(innerRectangle.width() * fraction)),
                                       0,
                                       innerRectangle.width());
    if (filledWidth == 0)
        return {};

    if (m_barOrientation == RightToLeftBar)
    {
        return QRect(innerRectangle.right() - filledWidth + 1,
                     innerRectangle.top(),
                     filledWidth,
                     innerRectangle.height());
    }

    return QRect(innerRectangle.left(), innerRectangle.top(), filledWidth, innerRectangle.height());
}

QColor LXQtResourceMonitor::resourceLightColor(Resource resource) const
{
    switch (resource)
    {
    case Resource::Cpu:
        return QColor(0, 196, 0, 160);       // Existing CPU Monitor family.
    case Resource::Memory:
        return QColor(48, 156, 255, 170);    // Blue: physical memory.
    case Resource::Swap:
        return QColor(255, 181, 32, 180);    // Amber: swap.
    case Resource::Count:
        break;
    }
    return palette().color(QPalette::Highlight);
}

QColor LXQtResourceMonitor::resourceDarkColor(Resource resource) const
{
    switch (resource)
    {
    case Resource::Cpu:
        return QColor(0, 128, 0, 255);
    case Resource::Memory:
        return QColor(0, 92, 184, 255);
    case Resource::Swap:
        return QColor(196, 112, 0, 255);
    case Resource::Count:
        break;
    }
    return palette().color(QPalette::Highlight);
}

QFont LXQtResourceMonitor::fittedTextFont(const QRect &meterRectangle) const
{
    QFont font = m_font;
    const QString widestText = QStringLiteral("100%");
    const int availableWidth = std::max(1, meterRectangle.width() - 2 * TextHorizontalPadding);
    const int availableHeight = std::max(1, meterRectangle.height() - 2);

    for (int pixelSize = PreferredTextPixelSize; pixelSize >= MinimumTextPixelSize; --pixelSize)
    {
        font.setPixelSize(pixelSize);
        const QFontMetrics metrics(font);
        if (metrics.horizontalAdvance(widestText) <= availableWidth && metrics.height() <= availableHeight)
            return font;
    }

    font.setPixelSize(MinimumTextPixelSize);
    return font;
}

void LXQtResourceMonitor::drawMeter(QPainter &painter,
                                    Resource resource,
                                    const QRect &meterRectangle)
{
    if (meterRectangle.isEmpty())
        return;

    const QColor lightColor = resourceLightColor(resource);
    const QColor darkColor = resourceDarkColor(resource);

    // A resource-colored outline remains visible even at 0%, so CPU/RAM/SWAP
    // are distinguishable without spending panel width on text labels.
    QColor outlineColor = darkColor;
    outlineColor.setAlpha(150);
    painter.setPen(QPen(outlineColor, 1));
    painter.drawRect(meterRectangle.adjusted(0, 0, -1, -1));

    const ResourceSnapshot &snapshot = m_resources[resourceIndex(resource)];
    const QRect filledRectangle = fillRect(meterRectangle, snapshot.valid ? snapshot.percent : 0.0);
    if (!filledRectangle.isEmpty())
    {
        QLinearGradient gradient;
        if (isVerticalBarOrientation())
            gradient = QLinearGradient(filledRectangle.left(), 0, filledRectangle.right(), 0);
        else
            gradient = QLinearGradient(0, filledRectangle.top(), 0, filledRectangle.bottom());

        gradient.setSpread(QLinearGradient::ReflectSpread);
        gradient.setColorAt(0.0, lightColor);
        gradient.setColorAt(0.5, darkColor);
        gradient.setColorAt(1.0, lightColor);
        painter.fillRect(filledRectangle, gradient);
    }

    if (!m_showText)
        return;

    painter.setFont(fittedTextFont(meterRectangle));
    painter.setPen(m_fontColor.isValid() ? m_fontColor : palette().color(QPalette::WindowText));

    const QString text = snapshot.valid
        ? QStringLiteral("%1%").arg(qRound(snapshot.percent))
        : QStringLiteral("--");
    painter.drawText(meterRectangle, Qt::AlignCenter, text);
}

void LXQtResourceMonitor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    drawMeter(painter, Resource::Cpu, meterRect(resourceIndex(Resource::Cpu)));
    drawMeter(painter, Resource::Memory, meterRect(resourceIndex(Resource::Memory)));
    drawMeter(painter, Resource::Swap, meterRect(resourceIndex(Resource::Swap)));
}

void LXQtResourceMonitor::refreshStats()
{
    auto &cpuSnapshot = m_resources[resourceIndex(Resource::Cpu)];
    auto &memorySnapshot = m_resources[resourceIndex(Resource::Memory)];
    auto &swapSnapshot = m_resources[resourceIndex(Resource::Swap)];

    if (!m_statgrabInitialized)
    {
        cpuSnapshot.valid = false;
        memorySnapshot.valid = false;
        swapSnapshot.valid = false;
        updateToolTip();
        update();
        return;
    }

#ifdef STATGRAB_NEWER_THAN_0_90
    size_t cpuCount = 0;
    const sg_cpu_percents *cpuPercentages = sg_get_cpu_percents(&cpuCount);
    cpuSnapshot.valid = cpuPercentages != nullptr && cpuCount > 0;
#else
    const sg_cpu_percents *cpuPercentages = sg_get_cpu_percents();
    cpuSnapshot.valid = cpuPercentages != nullptr;
#endif
    if (cpuSnapshot.valid)
    {
        // Match the existing LXQt CPU Monitor: user + kernel + nice.
        cpuSnapshot.percent = std::clamp(static_cast<double>(cpuPercentages->user
                                                             + cpuPercentages->kernel
                                                             + cpuPercentages->nice),
                                         0.0,
                                         100.0);
    }
    else
    {
        cpuSnapshot.percent = 0.0;
    }

#ifdef STATGRAB_NEWER_THAN_0_90
    size_t memoryCount = 0;
    const sg_mem_stats *memoryStats = sg_get_mem_stats(&memoryCount);
    memorySnapshot.valid = memoryStats != nullptr && memoryCount > 0;
#else
    const sg_mem_stats *memoryStats = sg_get_mem_stats();
    memorySnapshot.valid = memoryStats != nullptr;
#endif
    if (memorySnapshot.valid)
    {
        memorySnapshot.usedBytes = static_cast<quint64>(memoryStats->used);
        memorySnapshot.totalBytes = static_cast<quint64>(memoryStats->total);
        memorySnapshot.percent = ResourceMonitorMath::calculatePercent(memorySnapshot.usedBytes, memorySnapshot.totalBytes);
    }
    else
    {
        memorySnapshot.percent = 0.0;
        memorySnapshot.usedBytes = 0;
        memorySnapshot.totalBytes = 0;
    }

#ifdef STATGRAB_NEWER_THAN_0_90
    size_t swapCount = 0;
    const sg_swap_stats *swapStats = sg_get_swap_stats(&swapCount);
    swapSnapshot.valid = swapStats != nullptr && swapCount > 0;
#else
    const sg_swap_stats *swapStats = sg_get_swap_stats();
    swapSnapshot.valid = swapStats != nullptr;
#endif
    if (swapSnapshot.valid)
    {
        swapSnapshot.usedBytes = static_cast<quint64>(swapStats->used);
        swapSnapshot.totalBytes = static_cast<quint64>(swapStats->total);
        swapSnapshot.percent = ResourceMonitorMath::calculatePercent(swapSnapshot.usedBytes, swapSnapshot.totalBytes);
    }
    else
    {
        swapSnapshot.percent = 0.0;
        swapSnapshot.usedBytes = 0;
        swapSnapshot.totalBytes = 0;
    }

    updateToolTip();
    update();
}

void LXQtResourceMonitor::updateToolTip()
{
    const ResourceSnapshot &cpuSnapshot = m_resources[resourceIndex(Resource::Cpu)];
    const ResourceSnapshot &memorySnapshot = m_resources[resourceIndex(Resource::Memory)];
    const ResourceSnapshot &swapSnapshot = m_resources[resourceIndex(Resource::Swap)];

    QStringList lines;
    lines.reserve(3);

    lines.emplaceBack(cpuSnapshot.valid
        ? tr("CPU: %1%").arg(qRound(cpuSnapshot.percent))
        : tr("CPU: unavailable"));

    lines.emplaceBack(memorySnapshot.valid
        ? tr("RAM: %1% — %2 / %3")
              .arg(qRound(memorySnapshot.percent))
              .arg(ResourceMonitorMath::formatBytes(memorySnapshot.usedBytes))
              .arg(ResourceMonitorMath::formatBytes(memorySnapshot.totalBytes))
        : tr("RAM: unavailable"));

    if (!swapSnapshot.valid)
    {
        lines.emplaceBack(tr("SWAP: unavailable"));
    }
    else if (swapSnapshot.totalBytes == 0)
    {
        lines.emplaceBack(tr("SWAP: not configured"));
    }
    else
    {
        lines.emplaceBack(tr("SWAP: %1% — %2 / %3")
                              .arg(qRound(swapSnapshot.percent))
                              .arg(ResourceMonitorMath::formatBytes(swapSnapshot.usedBytes))
                              .arg(ResourceMonitorMath::formatBytes(swapSnapshot.totalBytes)));
    }

    setToolTip(lines.join(QLatin1Char('\n')));
}

void LXQtResourceMonitor::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId)
    {
        QFrame::timerEvent(event);
        return;
    }

    refreshStats();
}

void LXQtResourceMonitor::settingsChanged()
{
    if (m_timerId != -1)
    {
        killTimer(m_timerId);
        m_timerId = -1;
    }

    m_showText = m_plugin->settings()->value(QStringLiteral("showText"), true).toBool();
    m_widgetWidth = std::clamp(m_plugin->settings()->value(QStringLiteral("widgetWidth"), DefaultWidgetWidth).toInt(),
                               MinimumWidgetWidth,
                               MaximumWidgetWidth);
    m_updateIntervalMs = std::clamp(m_plugin->settings()->value(QStringLiteral("updateInterval"), DefaultUpdateIntervalMs).toInt(),
                                    MinimumUpdateIntervalMs,
                                    MaximumUpdateIntervalMs);

    const QString barOrientation = m_plugin->settings()
        ->value(QStringLiteral("barOrientation"), QStringLiteral("bottomUp"))
        .toString();

    if (barOrientation == QLatin1String(BarOrientRightLeft))
        m_barOrientation = RightToLeftBar;
    else if (barOrientation == QLatin1String(BarOrientLeftRight))
        m_barOrientation = LeftToRightBar;
    else if (barOrientation == QLatin1String(BarOrientTopDown))
        m_barOrientation = TopDownBar;
    else
        m_barOrientation = BottomUpBar;

    m_timerId = startTimer(m_updateIntervalMs);
    setSizes();
    update();
}
