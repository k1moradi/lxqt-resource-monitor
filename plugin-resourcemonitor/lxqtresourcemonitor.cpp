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
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScreen>
#include <QStandardPaths>
#include <QStringList>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

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

constexpr int OuterMargin = 0;
constexpr int MeterGap = 0;
constexpr int TextHorizontalPadding = 0;
constexpr int PreferredTextPixelSize = 10;
constexpr int MinimumTextPixelSize = 7;
constexpr int MinimumPopupWidth = 180;
constexpr int PreferredGraphWidth = 220;
constexpr int PopupGraphHeight = 22;
constexpr int PopupGap = 4;

QString formatByteRate(quint64 bytesPerSecond)
{
    return ResourceMonitorMath::formatBytes(bytesPerSecond) + QStringLiteral("/s");
}

QString formatByteRate(double bytesPerSecond)
{
    const double maximumRate = static_cast<double>(std::numeric_limits<quint64>::max());
    const quint64 roundedRate = bytesPerSecond >= maximumRate
        ? std::numeric_limits<quint64>::max()
        : static_cast<quint64>(std::max(0.0, std::round(bytesPerSecond)));
    return formatByteRate(roundedRate);
}

QColor blendWithSurface(const QColor &accent, const QColor &surface, double accentWeight)
{
    const double surfaceWeight = 1.0 - accentWeight;
    return QColor(qRound(accent.red() * accentWeight + surface.red() * surfaceWeight),
                  qRound(accent.green() * accentWeight + surface.green() * surfaceWeight),
                  qRound(accent.blue() * accentWeight + surface.blue() * surfaceWeight));
}

QColor contrastingResourceColor(QColor lightColor, QColor darkColor, const QColor &surface)
{
    lightColor.setAlpha(255);
    darkColor.setAlpha(255);
    const int lightDistance = std::abs(lightColor.lightness() - surface.lightness());
    const int darkDistance = std::abs(darkColor.lightness() - surface.lightness());
    return lightDistance >= darkDistance ? lightColor : darkColor;
}

class ResourceHistoryGraph final : public QWidget
{
public:
    ResourceHistoryGraph(const ResourceMonitorHistory::RollingEma *history,
                         const QColor &lightColor,
                         const QColor &darkColor,
                         QWidget *parent)
        : QWidget(parent)
        , m_history(history)
        , m_lightColor(lightColor)
        , m_darkColor(darkColor)
    {
        setAttribute(Qt::WA_OpaquePaintEvent);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(PopupGraphHeight - 2);
        setMaximumHeight(PopupGraphHeight);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return {PreferredGraphWidth, PopupGraphHeight};
    }

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        return {120, PopupGraphHeight - 2};
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QColor surface = palette().color(QPalette::Base);
        painter.fillRect(rect(), surface);
        if (width() <= 0 || height() <= 0)
            return;

        const int baselineY = height() - 1;
        painter.fillRect(QRect(0, baselineY, width(), 1), palette().color(QPalette::Mid));
        if (m_history == nullptr || m_history->sampleCount() == 0 || height() <= 1)
            return;

        const ResourceMonitorHistory::VisibleWindow window = m_history->visibleWindow(width());
        const int graphHeight = height() - 2;
        const QColor accent = contrastingResourceColor(m_lightColor,
                                                        m_darkColor,
                                                        surface);
        const QColor retainedColor = blendWithSurface(accent, surface, 0.58);
        for (std::size_t column = 0; column < window.columnCount; ++column)
        {
            const int filledHeight = std::clamp(
                static_cast<int>(std::lround(graphHeight
                                             * m_history->sampleAt(window.sampleStart + column) / 100.0)),
                0,
                graphHeight);
            if (filledHeight == 0)
                continue;

            const int x = window.firstColumn + static_cast<int>(column);
            const int y = baselineY - filledHeight;
            const QColor &color = column + 1 == window.columnCount ? accent : retainedColor;
            painter.fillRect(QRect(x, y, 1, filledHeight), color);
        }
    }

private:
    const ResourceMonitorHistory::RollingEma *m_history;
    QColor m_lightColor;
    QColor m_darkColor;
};

} // namespace

LXQtResourceMonitor::LXQtResourceMonitor(ILXQtPanelPlugin *plugin, QWidget *parent)
    : QFrame(parent)
    , m_plugin(plugin)
{
    setObjectName(QStringLiteral("LXQtResourceMonitor"));
    m_sizingWidget.setAttribute(Qt::WA_TransparentForMouseEvents);

    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(&m_sizingWidget);

    m_detailsPopup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    m_detailsPopup->setObjectName(QStringLiteral("LXQtResourceMonitorDetailsPopup"));
    m_detailsPopup->setFrameShape(QFrame::StyledPanel);
    m_detailsPopup->setFrameShadow(QFrame::Plain);
    m_detailsPopup->setLineWidth(1);
    m_detailsPopup->setAutoFillBackground(true);
    m_detailsPopup->setBackgroundRole(QPalette::Window);
    m_detailsPopup->setForegroundRole(QPalette::WindowText);
    m_detailsPopup->setMinimumWidth(MinimumPopupWidth);
    m_detailsPopup->setMaximumWidth(360);

    auto *popupLayout = new QVBoxLayout(m_detailsPopup);
    popupLayout->setContentsMargins(12, 10, 12, 10);
    popupLayout->setSpacing(0);

    auto *detailsHeading = new QLabel(tr("Resource Monitor"), m_detailsPopup);
    QFont headingFont = detailsHeading->font();
    headingFont.setWeight(QFont::DemiBold);
    detailsHeading->setFont(headingFont);
    popupLayout->addWidget(detailsHeading);

    auto *headingSeparator = new QFrame(m_detailsPopup);
    headingSeparator->setFrameShape(QFrame::HLine);
    headingSeparator->setFrameShadow(QFrame::Plain);
    popupLayout->addWidget(headingSeparator);

    m_noResourcesLabel = new QLabel(tr("No resources selected"), m_detailsPopup);
    m_noResourcesLabel->hide();
    popupLayout->addWidget(m_noResourcesLabel);

    const QFont secondaryFont = [this]() {
        QFont font = m_detailsPopup->font();
        if (font.pointSizeF() > 0.0)
            font.setPointSizeF(std::max(1.0, font.pointSizeF() - 1.0));
        else if (font.pixelSize() > 0)
            font.setPixelSize(std::max(8, font.pixelSize() - 1));
        return font;
    }();

    const std::array<QString, ResourceCount> resourceNames{
        tr("CPU"),
        tr("RAM"),
        tr("SWAP"),
        tr("Local disk"),
        tr("Local network"),
        tr("Internet")
    };
    for (std::size_t index = 0; index < ResourceCount; ++index)
    {
        PopupResourceDetails &details = m_popupResources[index];
        details.separator = new QFrame(m_detailsPopup);
        details.separator->setFrameShape(QFrame::HLine);
        details.separator->setFrameShadow(QFrame::Plain);
        details.separator->hide();
        popupLayout->addWidget(details.separator);

        details.section = new QWidget(m_detailsPopup);
        details.section->hide();
        auto *sectionLayout = new QVBoxLayout(details.section);
        sectionLayout->setContentsMargins(0, 5, 0, 5);
        sectionLayout->setSpacing(3);

        auto *headingLayout = new QHBoxLayout;
        headingLayout->setContentsMargins(0, 0, 0, 0);
        headingLayout->setSpacing(8);
        auto *name = new QLabel(resourceNames[index], details.section);
        QFont nameFont = details.section->font();
        nameFont.setWeight(QFont::DemiBold);
        name->setFont(nameFont);
        headingLayout->addWidget(name);
        headingLayout->addStretch(1);

        details.primaryValue = new QLabel(details.section);
        QFont primaryFont = details.primaryValue->font();
        primaryFont.setWeight(QFont::DemiBold);
        details.primaryValue->setFont(primaryFont);
        details.primaryValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        details.primaryValue->hide();
        headingLayout->addWidget(details.primaryValue);
        sectionLayout->addLayout(headingLayout);

        details.secondaryValue = new QLabel(details.section);
        details.secondaryValue->setFont(secondaryFont);
        details.secondaryValue->hide();
        sectionLayout->addWidget(details.secondaryValue);

        details.metricsRow = new QWidget(details.section);
        auto *metricsLayout = new QHBoxLayout(details.metricsRow);
        metricsLayout->setContentsMargins(0, 0, 0, 0);
        metricsLayout->setSpacing(12);

        std::array<QString, 2> metricCaptions{};
        switch (static_cast<Resource>(index))
        {
        case Resource::Disk:
            metricCaptions = {tr("↓ Read"), tr("↑ Write")};
            break;
        case Resource::LocalNet:
        case Resource::Internet:
            metricCaptions = {tr("↓ Receive"), tr("↑ Transmit")};
            break;
        default:
            break;
        }

        for (std::size_t metric = 0; metric < details.metricValues.size(); ++metric)
        {
            auto *metricCell = new QWidget(details.metricsRow);
            auto *metricLayout = new QVBoxLayout(metricCell);
            metricLayout->setContentsMargins(0, 0, 0, 0);
            metricLayout->setSpacing(0);

            auto *caption = new QLabel(metricCaptions[metric], metricCell);
            caption->setFont(secondaryFont);
            caption->setAlignment(metric == 0 ? Qt::AlignLeft : Qt::AlignRight);
            metricLayout->addWidget(caption);

            details.metricValues[metric] = new QLabel(metricCell);
            details.metricValues[metric]->setAlignment(metric == 0 ? Qt::AlignLeft : Qt::AlignRight);
            metricLayout->addWidget(details.metricValues[metric]);
            metricsLayout->addWidget(metricCell, 1);
        }
        details.metricsRow->hide();
        sectionLayout->addWidget(details.metricsRow);

        const Resource resource = static_cast<Resource>(index);
        details.graph = new ResourceHistoryGraph(&m_resources[index].history,
                                                  resourceLightColor(resource),
                                                  resourceDarkColor(resource),
                                                  details.section);
        details.graph->hide();
        sectionLayout->addWidget(details.graph);

        details.sessionPeakRow = new QWidget(details.section);
        auto *sessionPeakLayout = new QHBoxLayout(details.sessionPeakRow);
        sessionPeakLayout->setContentsMargins(0, 0, 0, 0);
        sessionPeakLayout->setSpacing(8);
        auto *sessionPeakLabel = new QLabel(tr("Combined session peak"), details.sessionPeakRow);
        sessionPeakLabel->setFont(secondaryFont);
        sessionPeakLayout->addWidget(sessionPeakLabel);
        sessionPeakLayout->addStretch(1);
        details.sessionPeakValue = new QLabel(details.sessionPeakRow);
        details.sessionPeakValue->setFont(secondaryFont);
        sessionPeakLayout->addWidget(details.sessionPeakValue);
        details.sessionPeakRow->hide();
        sectionLayout->addWidget(details.sessionPeakRow);

        popupLayout->addWidget(details.section);
    }

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
    m_ioSampleTimer.start();

    connect(&m_netCaptureProcess, &QProcess::readyReadStandardOutput,
            this, &LXQtResourceMonitor::readNetworkCaptureOutput);
    connect(&m_netCaptureProcess, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray errorOutput = m_netCaptureProcess.readAllStandardError().trimmed();
        if (!errorOutput.isEmpty())
            qWarning().noquote() << "Resource Monitor network capture:" << QString::fromLocal8Bit(errorOutput);
    });
    connect(&m_netCaptureProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            m_netCaptureConfigured = false;
        updateToolTip();
    });
    connect(&m_netCaptureProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int, QProcess::ExitStatus) {
                m_pendingNetworkBytes = {};
                updateToolTip();
                update();
            });

    m_historyTimerId = startTimer(HistorySampleIntervalMs, Qt::PreciseTimer);
    settingsChanged();
    refreshStats();
    update();
}

LXQtResourceMonitor::~LXQtResourceMonitor()
{
    if (m_timerId != -1)
        killTimer(m_timerId);
    if (m_historyTimerId != -1)
        killTimer(m_historyTimerId);

    if (m_netCaptureProcess.state() != QProcess::NotRunning)
    {
        m_netCaptureProcess.terminate();
        if (!m_netCaptureProcess.waitForFinished(250))
        {
            m_netCaptureProcess.kill();
            m_netCaptureProcess.waitForFinished(250);
        }
    }

    if (m_statgrabInitialized)
        sg_shutdown();
}

bool LXQtResourceMonitor::isResourceEnabled(Resource resource) const
{
    return m_enabledResources[resourceIndex(resource)];
}

int LXQtResourceMonitor::enabledResourceCount() const
{
    return static_cast<int>(std::count(m_enabledResources.cbegin(), m_enabledResources.cend(), true));
}

void LXQtResourceMonitor::setSizes()
{
    // The width setting describes the default three-resource layout. Keep the
    // same per-resource width as optional meters are enabled or disabled.
    const int resourceCount = enabledResourceCount();
    m_sizingWidget.setFixedWidth(m_widgetWidth * resourceCount / 3);
    m_sizingWidget.setMinimumHeight(24);
}

void LXQtResourceMonitor::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    setSizes();
    update();
    if (m_detailsPopup != nullptr && m_detailsPopup->isVisible())
        positionDetailsPopup();
}

void LXQtResourceMonitor::changeEvent(QEvent *event)
{
    QFrame::changeEvent(event);

    if (m_detailsPopup == nullptr)
        return;

    switch (event->type())
    {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
    case QEvent::StyleChange:
    case QEvent::FontChange:
        m_detailsPopup->update();
        for (const PopupResourceDetails &details : m_popupResources)
        {
            if (details.graph != nullptr)
                details.graph->update();
        }
        if (m_detailsPopup->isVisible())
            positionDetailsPopup();
        break;
    default:
        break;
    }
}

void LXQtResourceMonitor::panelGeometryChanged()
{
    if (m_detailsPopup != nullptr && m_detailsPopup->isVisible())
        positionDetailsPopup();
}

QRect LXQtResourceMonitor::meterRect(int meterIndex, int meterCount) const
{
    const QRect availableRectangle = rect().adjusted(OuterMargin,
                                                      OuterMargin,
                                                      -OuterMargin,
                                                      -OuterMargin);
    if (availableRectangle.isEmpty() || meterCount <= 0 || meterIndex < 0 || meterIndex >= meterCount)
        return {};

    const int usableWidth = std::max(0, availableRectangle.width() - MeterGap * (meterCount - 1));
    const int baseWidth = usableWidth / meterCount;
    const int remainder = usableWidth % meterCount;

    int x = availableRectangle.left();
    for (int index = 0; index < meterIndex; ++index)
        x += baseWidth + (index < remainder ? 1 : 0) + MeterGap;

    const int width = baseWidth + (meterIndex < remainder ? 1 : 0);
    return QRect(x, availableRectangle.top(), width, availableRectangle.height());
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
    case Resource::Disk:
        return QColor(200, 112, 224, 190);  // Purple: local disk I/O.
    case Resource::LocalNet:
        return QColor(32, 196, 176, 190);   // Teal: local network I/O.
    case Resource::Internet:
        return QColor(255, 96, 96, 190);    // Red: Internet I/O.
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
    case Resource::Disk:
        return QColor(112, 48, 144, 255);
    case Resource::LocalNet:
        return QColor(0, 112, 96, 255);
    case Resource::Internet:
        return QColor(176, 32, 32, 255);
    case Resource::Count:
        break;
    }
    return palette().color(QPalette::Highlight);
}

QFont LXQtResourceMonitor::fittedTextFont(const QRect &meterRectangle) const
{
    QFont font = m_font;
    const QString widestText = QStringLiteral("100");
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

    QColor newestColumnColor = resourceLightColor(resource);
    newestColumnColor.setAlpha(255);
    const QColor darkColor = resourceDarkColor(resource);
    const ResourceSnapshot &snapshot = m_resources[resourceIndex(resource)];
    if (snapshot.valid && snapshot.history.sampleCount() > 0)
    {
        // Each 1-pixel column is one EMA sample. The newest is on the right,
        // and every update advances the older columns one pixel to the left.
        const ResourceMonitorHistory::VisibleWindow window =
            snapshot.history.visibleWindow(meterRectangle.width());
        for (std::size_t column = 0; column < window.columnCount; ++column)
        {
            const int filledHeight = std::clamp(
                static_cast<int>(std::lround(meterRectangle.height()
                                             * snapshot.history.sampleAt(window.sampleStart + column) / 100.0)),
                0,
                meterRectangle.height());
            if (filledHeight == 0)
                continue;

            const int x = meterRectangle.left() + window.firstColumn + static_cast<int>(column);
            const int y = m_barOrientation == TopDownBar
                ? meterRectangle.top()
                : meterRectangle.bottom() - filledHeight + 1;
            const QColor &columnColor = column + 1 == window.columnCount
                ? newestColumnColor
                : darkColor;
            painter.fillRect(QRect(x, y, 1, filledHeight), columnColor);
        }
    }

    if (!m_showText)
        return;

    painter.setFont(fittedTextFont(meterRectangle));
    painter.setPen(m_fontColor.isValid() ? m_fontColor : palette().color(QPalette::WindowText));

    const QString text = snapshot.valid
        ? QStringLiteral("%1").arg(qRound(snapshot.percent))
        : QStringLiteral("--");
    painter.drawText(meterRectangle, Qt::AlignCenter, text);
}

void LXQtResourceMonitor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const int meterCount = enabledResourceCount();
    int meterIndex = 0;
    for (std::size_t index = 0; index < ResourceCount; ++index)
    {
        if (!m_enabledResources[index])
            continue;

        drawMeter(painter,
                  static_cast<Resource>(index),
                  meterRect(meterIndex, meterCount));
        ++meterIndex;
    }
}

void LXQtResourceMonitor::refreshStats()
{
    auto &cpuSnapshot = m_resources[resourceIndex(Resource::Cpu)];
    auto &memorySnapshot = m_resources[resourceIndex(Resource::Memory)];
    auto &swapSnapshot = m_resources[resourceIndex(Resource::Swap)];
    const qint64 elapsedMilliseconds = std::max<qint64>(1, m_ioSampleTimer.restart());

    if (!m_statgrabInitialized)
    {
        for (auto &snapshot : m_resources)
            snapshot.valid = false;
    }
    else
    {
        const ResourceMonitorMath::CpuUsageSample cpuUsage =
            ResourceMonitorMath::sampleCpuUsage();
        cpuSnapshot.valid = cpuUsage.valid;
        cpuSnapshot.percent = cpuUsage.percent;

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

        auto &diskSnapshot = m_resources[resourceIndex(Resource::Disk)];
#ifdef STATGRAB_NEWER_THAN_0_90
        size_t diskCount = 0;
        const sg_disk_io_stats *diskStats = sg_get_disk_io_stats_diff(&diskCount);
#else
        const sg_disk_io_stats *diskStats = sg_get_disk_io_stats_diff();
        const size_t diskCount = diskStats != nullptr ? 1 : 0;
#endif
        if (diskStats != nullptr && diskCount > 0)
        {
            quint64 readBytes = 0;
            quint64 writeBytes = 0;
            for (size_t index = 0; index < diskCount; ++index)
            {
                readBytes += diskStats[index].read_bytes;
                writeBytes += diskStats[index].write_bytes;
            }
            updateIoSnapshot(Resource::Disk, readBytes, writeBytes, elapsedMilliseconds);
        }
        else
        {
            diskSnapshot.valid = false;
        }
    }

    readNetworkCaptureOutput();
    const bool networkCaptureRunning = m_netCaptureConfigured
        && m_netCaptureProcess.state() == QProcess::Running;
    if (networkCaptureRunning)
    {
        updateIoSnapshot(Resource::LocalNet,
                         m_pendingNetworkBytes[0],
                         m_pendingNetworkBytes[1],
                         elapsedMilliseconds);
        updateIoSnapshot(Resource::Internet,
                         m_pendingNetworkBytes[2],
                         m_pendingNetworkBytes[3],
                         elapsedMilliseconds);
    }
    else
    {
        m_resources[resourceIndex(Resource::LocalNet)].valid = false;
        m_resources[resourceIndex(Resource::Internet)].valid = false;
    }
    m_pendingNetworkBytes = {};

    updateHistory();
    updateToolTip();
    if (m_detailsPopup->isVisible())
        updateDetailsPopup();
}

void LXQtResourceMonitor::updateHistory()
{
    for (ResourceSnapshot &snapshot : m_resources)
    {
        if (!snapshot.valid)
        {
            snapshot.history.update(0.0, false);
            continue;
        }

        snapshot.history.update(snapshot.percent, true);
    }
}

void LXQtResourceMonitor::configureNetworkCapture()
{
    const bool networkMonitoringEnabled = isResourceEnabled(Resource::LocalNet)
        || isResourceEnabled(Resource::Internet);

    if (!networkMonitoringEnabled)
    {
        m_netCaptureConfigured = false;
        m_pendingNetworkBytes = {};
        m_netCaptureBuffer.clear();
        m_resources[resourceIndex(Resource::LocalNet)].valid = false;
        m_resources[resourceIndex(Resource::Internet)].valid = false;
        if (m_netCaptureProcess.state() != QProcess::NotRunning)
        {
            m_netCaptureProcess.terminate();
            if (!m_netCaptureProcess.waitForFinished(250))
            {
                m_netCaptureProcess.kill();
                m_netCaptureProcess.waitForFinished(250);
            }
        }
        return;
    }

    if (m_netCaptureProcess.state() != QProcess::NotRunning)
        return;

    const QString executable = QStandardPaths::findExecutable(QStringLiteral("resourcemonitor-netcap"));
    if (executable.isEmpty())
    {
        m_netCaptureConfigured = false;
        return;
    }

    m_netCaptureConfigured = true;
    m_pendingNetworkBytes = {};
    m_netCaptureBuffer.clear();
    m_netCaptureProcess.start(executable, QStringList{}, QIODevice::ReadOnly);
}

void LXQtResourceMonitor::readNetworkCaptureOutput()
{
    if (m_netCaptureProcess.state() == QProcess::NotRunning)
        return;

    m_netCaptureBuffer.append(m_netCaptureProcess.readAllStandardOutput());
    constexpr qsizetype MaximumBufferedBytes = 4096;
    if (m_netCaptureBuffer.size() > MaximumBufferedBytes)
    {
        m_netCaptureBuffer.clear();
        m_pendingNetworkBytes = {};
        return;
    }

    while (true)
    {
        const qsizetype newline = m_netCaptureBuffer.indexOf('\n');
        if (newline < 0)
            break;

        const QList<QByteArray> fields = m_netCaptureBuffer.left(newline).simplified().split(' ');
        m_netCaptureBuffer.remove(0, newline + 1);
        if (fields.size() != 4)
            continue;

        std::array<quint64, 4> counters{};
        bool validLine = true;
        for (std::size_t index = 0; index < counters.size(); ++index)
        {
            bool ok = false;
            counters[index] = fields[static_cast<qsizetype>(index)].toULongLong(&ok);
            validLine = validLine && ok;
        }
        if (!validLine)
            continue;

        for (std::size_t index = 0; index < counters.size(); ++index)
        {
            const quint64 remaining = std::numeric_limits<quint64>::max() - m_pendingNetworkBytes[index];
            m_pendingNetworkBytes[index] += std::min(counters[index], remaining);
        }
    }
}

void LXQtResourceMonitor::updateIoSnapshot(Resource resource,
                                           quint64 readBytes,
                                           quint64 writeBytes,
                                           qint64 elapsedMilliseconds)
{
    const std::size_t peakIndex = ioPeakIndex(resource);
    if (peakIndex >= m_ioPeakBytesPerSecond.size())
        return;

    ResourceSnapshot &snapshot = m_resources[resourceIndex(resource)];
    const double intervalSeconds = static_cast<double>(std::max<qint64>(1, elapsedMilliseconds)) / 1000.0;
    const double readRate = static_cast<double>(readBytes) / intervalSeconds;
    const double writeRate = static_cast<double>(writeBytes) / intervalSeconds;
    const double totalRate = readRate + writeRate;

    snapshot.readBytesPerSecond = static_cast<quint64>(std::llround(readRate));
    snapshot.writeBytesPerSecond = static_cast<quint64>(std::llround(writeRate));
    snapshot.valid = true;

    double &peakRate = m_ioPeakBytesPerSecond[peakIndex];
    const double previousPeakRate = peakRate;
    const bool networkResource = resource == Resource::LocalNet
        || resource == Resource::Internet;
    peakRate = networkResource
        ? std::max(totalRate, previousPeakRate)
        : std::max(totalRate, previousPeakRate * 0.9);

    if (networkResource && peakRate > previousPeakRate && previousPeakRate > 0.0)
    {
        // Keep retained EMA columns on the same scale as the new all-time peak.
        snapshot.history.scale(previousPeakRate / peakRate);
    }

    snapshot.percent = peakRate > 0.0
        ? std::clamp(totalRate * 100.0 / peakRate, 0.0, 100.0)
        : 0.0;
}

void LXQtResourceMonitor::updateToolTip()
{
    const ResourceSnapshot &cpuSnapshot = m_resources[resourceIndex(Resource::Cpu)];
    const ResourceSnapshot &memorySnapshot = m_resources[resourceIndex(Resource::Memory)];
    const ResourceSnapshot &swapSnapshot = m_resources[resourceIndex(Resource::Swap)];
    const ResourceSnapshot &diskSnapshot = m_resources[resourceIndex(Resource::Disk)];
    const ResourceSnapshot &localNetSnapshot = m_resources[resourceIndex(Resource::LocalNet)];
    const ResourceSnapshot &internetSnapshot = m_resources[resourceIndex(Resource::Internet)];

    QStringList lines;
    lines.reserve(enabledResourceCount());

    if (isResourceEnabled(Resource::Cpu))
    {
        lines.emplaceBack(cpuSnapshot.valid
            ? tr("CPU: %1%").arg(qRound(cpuSnapshot.percent))
            : tr("CPU: unavailable"));
    }

    if (isResourceEnabled(Resource::Memory))
    {
        lines.emplaceBack(memorySnapshot.valid
            ? tr("RAM: %1% — %2 / %3")
                  .arg(qRound(memorySnapshot.percent))
                  .arg(ResourceMonitorMath::formatBytes(memorySnapshot.usedBytes))
                  .arg(ResourceMonitorMath::formatBytes(memorySnapshot.totalBytes))
            : tr("RAM: unavailable"));
    }

    if (isResourceEnabled(Resource::Swap))
    {
        if (!swapSnapshot.valid)
            lines.emplaceBack(tr("SWAP: unavailable"));
        else if (swapSnapshot.totalBytes == 0)
            lines.emplaceBack(tr("SWAP: no active swap"));
        else
            lines.emplaceBack(tr("SWAP: %1% — %2 / %3")
                                  .arg(qRound(swapSnapshot.percent))
                                  .arg(ResourceMonitorMath::formatBytes(swapSnapshot.usedBytes))
                                  .arg(ResourceMonitorMath::formatBytes(swapSnapshot.totalBytes)));
    }

    if (isResourceEnabled(Resource::Disk))
    {
        lines.emplaceBack(diskSnapshot.valid
            ? tr("Local disk I/O: read %1, write %2")
                  .arg(formatByteRate(diskSnapshot.readBytesPerSecond))
                  .arg(formatByteRate(diskSnapshot.writeBytesPerSecond))
            : tr("Local disk I/O: unavailable"));
    }
    if (isResourceEnabled(Resource::LocalNet))
    {
        lines.emplaceBack(localNetSnapshot.valid
            ? tr("Local network I/O: receive %1, transmit %2")
                  .arg(formatByteRate(localNetSnapshot.readBytesPerSecond))
                  .arg(formatByteRate(localNetSnapshot.writeBytesPerSecond))
            : tr("Local network I/O: unavailable (capture helper with CAP_NET_RAW required)"));
    }
    if (isResourceEnabled(Resource::Internet))
    {
        lines.emplaceBack(internetSnapshot.valid
            ? tr("Internet I/O: receive %1, transmit %2")
                  .arg(formatByteRate(internetSnapshot.readBytesPerSecond))
                  .arg(formatByteRate(internetSnapshot.writeBytesPerSecond))
            : tr("Internet I/O: unavailable (capture helper with CAP_NET_RAW required)"));
    }

    if (lines.isEmpty())
        lines.emplaceBack(tr("No resources selected"));

    setToolTip(lines.join(QLatin1Char('\n')));
}

void LXQtResourceMonitor::updateDetailsPopup()
{
    bool geometryChanged = false;
    bool hasEnabledResources = false;
    const auto setVisibility = [&geometryChanged](QWidget *widget, bool visible) {
        const bool shouldBeHidden = !visible;
        if (widget->isHidden() != shouldBeHidden)
            geometryChanged = true;
        widget->setVisible(visible);
    };
    const auto setTextIfChanged = [](QLabel *label, const QString &text) {
        if (label->text() != text)
            label->setText(text);
    };
    const auto formatPercent = [this](double percent) {
        return tr("%L1%").arg(percent, 0, 'f', 1);
    };
    const QString unavailable = tr("Unavailable");
    const auto setTextAndVisibility = [&setVisibility, &setTextIfChanged](QLabel *label,
                                                                          const QString &text,
                                                                          bool visible) {
        setTextIfChanged(label, text);
        setVisibility(label, visible);
    };

    for (std::size_t index = 0; index < ResourceCount; ++index)
    {
        const Resource resource = static_cast<Resource>(index);
        PopupResourceDetails &details = m_popupResources[index];
        const bool enabled = isResourceEnabled(resource);
        setVisibility(details.separator, enabled && hasEnabledResources);
        setVisibility(details.section, enabled);
        if (!enabled)
            continue;

        hasEnabledResources = true;
        const ResourceSnapshot &snapshot = m_resources[index];
        const auto showHistory = [&details, &snapshot, &setVisibility]() {
            const bool hasHistory = snapshot.valid && snapshot.history.sampleCount() > 0;
            setVisibility(details.graph, hasHistory);
            if (details.graph->isVisible())
                details.graph->update();
        };

        switch (resource)
        {
        case Resource::Cpu:
            setTextAndVisibility(details.primaryValue,
                                 snapshot.valid ? formatPercent(snapshot.percent) : unavailable,
                                 true);
            setVisibility(details.secondaryValue, false);
            setVisibility(details.metricsRow, false);
            setVisibility(details.sessionPeakRow, false);
            showHistory();
            break;
        case Resource::Memory:
            if (snapshot.valid && snapshot.totalBytes > 0)
            {
                setTextAndVisibility(details.primaryValue, formatPercent(snapshot.percent), true);
                setTextAndVisibility(details.secondaryValue,
                                     tr("%1 / %2")
                                         .arg(ResourceMonitorMath::formatBytes(snapshot.usedBytes))
                                         .arg(ResourceMonitorMath::formatBytes(snapshot.totalBytes)),
                                     true);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                showHistory();
            }
            else
            {
                setTextAndVisibility(details.primaryValue, unavailable, true);
                setVisibility(details.secondaryValue, false);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                setVisibility(details.graph, false);
            }
            break;
        case Resource::Swap:
            if (!snapshot.valid)
            {
                setTextAndVisibility(details.primaryValue, unavailable, true);
                setVisibility(details.secondaryValue, false);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                setVisibility(details.graph, false);
            }
            else if (snapshot.totalBytes == 0)
            {
                setTextAndVisibility(details.primaryValue, tr("No active swap"), true);
                setVisibility(details.secondaryValue, false);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                setVisibility(details.graph, false);
            }
            else
            {
                setTextAndVisibility(details.primaryValue, formatPercent(snapshot.percent), true);
                setTextAndVisibility(details.secondaryValue,
                                     tr("%1 / %2")
                                         .arg(ResourceMonitorMath::formatBytes(snapshot.usedBytes))
                                         .arg(ResourceMonitorMath::formatBytes(snapshot.totalBytes)),
                                     true);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                showHistory();
            }
            break;
        case Resource::Disk:
            if (snapshot.valid)
            {
                setTextAndVisibility(details.primaryValue, QString{}, false);
                setVisibility(details.secondaryValue, false);
                setTextIfChanged(details.metricValues[0], formatByteRate(snapshot.readBytesPerSecond));
                setTextIfChanged(details.metricValues[1], formatByteRate(snapshot.writeBytesPerSecond));
                setVisibility(details.metricsRow, true);
                setVisibility(details.sessionPeakRow, false);
                showHistory();
            }
            else
            {
                setTextAndVisibility(details.primaryValue, unavailable, true);
                setVisibility(details.secondaryValue, false);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                setVisibility(details.graph, false);
            }
            break;
        case Resource::LocalNet:
        case Resource::Internet:
            if (snapshot.valid)
            {
                const std::size_t peakIndex = ioPeakIndex(resource);
                setTextAndVisibility(details.primaryValue, QString{}, false);
                setVisibility(details.secondaryValue, false);
                setTextIfChanged(details.metricValues[0], formatByteRate(snapshot.readBytesPerSecond));
                setTextIfChanged(details.metricValues[1], formatByteRate(snapshot.writeBytesPerSecond));
                setVisibility(details.metricsRow, true);
                // The existing network high-water mark is max(receive + transmit).
                setTextIfChanged(details.sessionPeakValue,
                                 formatByteRate(m_ioPeakBytesPerSecond[peakIndex]));
                setVisibility(details.sessionPeakRow, true);
                showHistory();
            }
            else
            {
                setTextAndVisibility(details.primaryValue, unavailable, true);
                setVisibility(details.secondaryValue, false);
                setVisibility(details.metricsRow, false);
                setVisibility(details.sessionPeakRow, false);
                setVisibility(details.graph, false);
            }
            break;
        case Resource::Count:
            break;
        }
    }

    setVisibility(m_noResourcesLabel, !hasEnabledResources);
    if (m_detailsPopup->isVisible() && geometryChanged)
        positionDetailsPopup();
}

void LXQtResourceMonitor::showDetailsPopup()
{
    updateDetailsPopup();
    if (m_plugin != nullptr && m_plugin->panel() != nullptr)
        m_plugin->panel()->willShowWindow(m_detailsPopup);
    positionDetailsPopup();
    m_detailsPopup->show();
    m_detailsPopup->raise();
}

void LXQtResourceMonitor::positionDetailsPopup()
{
    enum class Side
    {
        Above,
        Below,
        Left,
        Right
    };

    const ILXQtPanel *panel = m_plugin != nullptr ? m_plugin->panel() : nullptr;
    const ILXQtPanel::Position panelPosition = panel != nullptr
        ? panel->position()
        : ILXQtPanel::PositionBottom;

    Side preferredSide = Side::Above;
    switch (panelPosition)
    {
    case ILXQtPanel::PositionBottom:
        preferredSide = Side::Above;
        break;
    case ILXQtPanel::PositionTop:
        preferredSide = Side::Below;
        break;
    case ILXQtPanel::PositionLeft:
        preferredSide = Side::Right;
        break;
    case ILXQtPanel::PositionRight:
        preferredSide = Side::Left;
        break;
    }

    const auto oppositeSide = [](Side side) {
        switch (side)
        {
        case Side::Above:
            return Side::Below;
        case Side::Below:
            return Side::Above;
        case Side::Left:
            return Side::Right;
        case Side::Right:
            return Side::Left;
        }
        return Side::Above;
    };
    const QRect widgetGeometry(mapToGlobal(QPoint(0, 0)), size());
    const auto positionForSide = [&widgetGeometry](Side side, const QSize &popupSize) {
        switch (side)
        {
        case Side::Above:
            return QPoint(widgetGeometry.left(), widgetGeometry.top() - popupSize.height() - PopupGap);
        case Side::Below:
            return QPoint(widgetGeometry.left(), widgetGeometry.bottom() + 1 + PopupGap);
        case Side::Left:
            return QPoint(widgetGeometry.left() - popupSize.width() - PopupGap, widgetGeometry.top());
        case Side::Right:
            return QPoint(widgetGeometry.right() + 1 + PopupGap, widgetGeometry.top());
        }
        return widgetGeometry.topLeft();
    };

    QScreen *screen = QGuiApplication::screenAt(mapToGlobal(rect().center()));
    if (screen == nullptr)
        screen = QGuiApplication::primaryScreen();

    if (screen == nullptr)
    {
        m_detailsPopup->adjustSize();
        const QRect targetGeometry(positionForSide(preferredSide, m_detailsPopup->size()),
                                   m_detailsPopup->size());
        if (m_detailsPopup->geometry() != targetGeometry)
            m_detailsPopup->setGeometry(targetGeometry);
        return;
    }

    const QRect availableGeometry = screen->availableGeometry();
    const int maximumWidth = std::max(1, std::min(360, availableGeometry.width()));
    const int maximumHeight = std::max(1, availableGeometry.height());
    m_detailsPopup->setMinimumWidth(std::min(MinimumPopupWidth, maximumWidth));
    m_detailsPopup->setMaximumSize(maximumWidth, maximumHeight);
    m_detailsPopup->adjustSize();

    const QSize popupSize = m_detailsPopup->size();
    QPoint position = positionForSide(preferredSide, popupSize);
    if (!availableGeometry.contains(QRect(position, popupSize)))
    {
        const Side alternateSide = oppositeSide(preferredSide);
        const QPoint alternatePosition = positionForSide(alternateSide, popupSize);
        if (availableGeometry.contains(QRect(alternatePosition, popupSize)))
        {
            position = alternatePosition;
        }
        else
        {
            const auto availableSpaceForSide = [&availableGeometry, &widgetGeometry](Side side) {
                switch (side)
                {
                case Side::Above:
                    return std::max(0, widgetGeometry.top() - PopupGap - availableGeometry.top());
                case Side::Below:
                    return std::max(0,
                                    availableGeometry.bottom() + 1
                                        - (widgetGeometry.bottom() + 1 + PopupGap));
                case Side::Left:
                    return std::max(0, widgetGeometry.left() - PopupGap - availableGeometry.left());
                case Side::Right:
                    return std::max(0,
                                    availableGeometry.right() + 1
                                        - (widgetGeometry.right() + 1 + PopupGap));
                }
                return 0;
            };
            const int popupExtentForSide = preferredSide == Side::Above || preferredSide == Side::Below
                ? popupSize.height()
                : popupSize.width();
            if (availableSpaceForSide(preferredSide) < popupExtentForSide
                && availableSpaceForSide(alternateSide) > availableSpaceForSide(preferredSide))
            {
                position = alternatePosition;
            }
        }
    }

    const int maximumX = std::max(availableGeometry.left(),
                                  availableGeometry.right() - popupSize.width() + 1);
    const int maximumY = std::max(availableGeometry.top(),
                                  availableGeometry.bottom() - popupSize.height() + 1);
    position.setX(std::clamp(position.x(), availableGeometry.left(), maximumX));
    position.setY(std::clamp(position.y(), availableGeometry.top(), maximumY));
    const QRect targetGeometry(position, popupSize);
    if (m_detailsPopup->geometry() != targetGeometry)
        m_detailsPopup->setGeometry(targetGeometry);
}

void LXQtResourceMonitor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (m_detailsPopup->isVisible())
            m_detailsPopup->hide();
        else
            showDetailsPopup();
        event->accept();
        return;
    }

    QFrame::mousePressEvent(event);
}

void LXQtResourceMonitor::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_historyTimerId)
    {
        refreshStats();
        update();
        return;
    }

    if (event->timerId() == m_timerId)
    {
        update();
        return;
    }

    QFrame::timerEvent(event);
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
    m_enabledResources[resourceIndex(Resource::Cpu)] =
        m_plugin->settings()->value(QStringLiteral("monitorCpu"), true).toBool();
    m_enabledResources[resourceIndex(Resource::Memory)] =
        m_plugin->settings()->value(QStringLiteral("monitorMemory"), true).toBool();
    m_enabledResources[resourceIndex(Resource::Swap)] =
        m_plugin->settings()->value(QStringLiteral("monitorSwap"), true).toBool();
    m_enabledResources[resourceIndex(Resource::Disk)] =
        m_plugin->settings()->value(QStringLiteral("monitorDisk"), false).toBool();
    m_enabledResources[resourceIndex(Resource::LocalNet)] =
        m_plugin->settings()->value(QStringLiteral("monitorLocalNet"), false).toBool();
    m_enabledResources[resourceIndex(Resource::Internet)] =
        m_plugin->settings()->value(QStringLiteral("monitorInternet"), false).toBool();
    m_updateIntervalMs = std::clamp(m_plugin->settings()->value(QStringLiteral("updateInterval"), DefaultUpdateIntervalMs).toInt(),
                                    MinimumUpdateIntervalMs,
                                    MaximumUpdateIntervalMs);

    const QString barOrientation = m_plugin->settings()
        ->value(QStringLiteral("barOrientation"), QStringLiteral("bottomUp"))
        .toString();

    if (barOrientation == QLatin1String(BarOrientTopDown))
        m_barOrientation = TopDownBar;
    else
        m_barOrientation = BottomUpBar;

    configureNetworkCapture();
    m_timerId = startTimer(m_updateIntervalMs);
    setSizes();
    updateToolTip();
    if (m_detailsPopup->isVisible())
        updateDetailsPopup();
    update();
}
