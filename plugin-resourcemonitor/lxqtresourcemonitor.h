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

#ifndef LXQTRESOURCEMONITOR_H
#define LXQTRESOURCEMONITOR_H

#include <QColor>
#include <QElapsedTimer>
#include <QFont>
#include <QFrame>
#include <QProcess>
#include <QByteArray>
#include <array>
#include <cstddef>

class ILXQtPanelPlugin;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QTimerEvent;

class LXQtResourceMonitor final : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(QColor fontColor READ fontColor WRITE setFontColor)

public:
    enum BarOrientation
    {
        BottomUpBar,
        TopDownBar,
        RightToLeftBar,
        LeftToRightBar
    };

    explicit LXQtResourceMonitor(ILXQtPanelPlugin *plugin, QWidget *parent = nullptr);
    ~LXQtResourceMonitor() override;

    void settingsChanged();

    void setFontColor(const QColor &value) { m_fontColor = value; }
    [[nodiscard]] QColor fontColor() const { return m_fontColor; }

protected:
    void timerEvent(QTimerEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class Resource : std::size_t
    {
        Cpu = 0,
        Memory,
        Swap,
        Disk,
        LocalNet,
        Internet,
        Count
    };

    struct ResourceSnapshot
    {
        double percent{0.0};
        quint64 usedBytes{0};
        quint64 totalBytes{0};
        quint64 readBytesPerSecond{0};
        quint64 writeBytesPerSecond{0};
        bool valid{false};
    };

    static constexpr std::size_t ResourceCount = static_cast<std::size_t>(Resource::Count);
    static constexpr int DefaultWidgetWidth = 57;
    static constexpr int MinimumWidgetWidth = 48;
    static constexpr int MaximumWidgetWidth = 300;
    static constexpr int DefaultUpdateIntervalMs = 1000;
    static constexpr int MinimumUpdateIntervalMs = 500;
    static constexpr int MaximumUpdateIntervalMs = 10000000;

    [[nodiscard]] static constexpr std::size_t resourceIndex(Resource resource)
    {
        return static_cast<std::size_t>(resource);
    }

    [[nodiscard]] bool isResourceEnabled(Resource resource) const;
    [[nodiscard]] int enabledResourceCount() const;
    [[nodiscard]] bool isVerticalBarOrientation() const;
    [[nodiscard]] QRect meterRect(int meterIndex, int meterCount) const;
    [[nodiscard]] QRect fillRect(const QRect &meterRectangle, double percent) const;
    [[nodiscard]] QColor resourceLightColor(Resource resource) const;
    [[nodiscard]] QColor resourceDarkColor(Resource resource) const;
    [[nodiscard]] QFont fittedTextFont(const QRect &meterRectangle) const;

    void setSizes();
    void configureNetworkCapture();
    void readNetworkCaptureOutput();
    void refreshStats();
    void updateIoSnapshot(Resource resource,
                          quint64 readBytes,
                          quint64 writeBytes,
                          qint64 elapsedMilliseconds);
    void updateToolTip();
    void drawMeter(QPainter &painter, Resource resource, const QRect &meterRectangle);

    ILXQtPanelPlugin *m_plugin;
    QWidget m_sizingWidget;
    std::array<ResourceSnapshot, static_cast<std::size_t>(Resource::Count)> m_resources{};

    bool m_showText{true};
    int m_widgetWidth{DefaultWidgetWidth};
    BarOrientation m_barOrientation{BottomUpBar};
    int m_updateIntervalMs{DefaultUpdateIntervalMs};
    int m_timerId{-1};
    bool m_statgrabInitialized{false};
    std::array<bool, ResourceCount> m_enabledResources{true, true, true, false, false, false};
    std::array<double, 3> m_ioPeakBytesPerSecond{};
    QProcess m_netCaptureProcess;
    QByteArray m_netCaptureBuffer;
    std::array<quint64, 4> m_pendingNetworkBytes{};
    bool m_netCaptureConfigured{false};
    QElapsedTimer m_ioSampleTimer;

    QFont m_font;
    QColor m_fontColor;
};

#endif // LXQTRESOURCEMONITOR_H
