/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * LXQt - a lightweight, Qt based, desktop toolset
 * https://lxqt.org
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef LXQTRESOURCEMONITORHISTORY_H
#define LXQTRESOURCEMONITORHISTORY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ResourceMonitorHistory
{
// Widget width is capped at 300 px. Keep at most one 1-second EMA value per
// possible display column so drawing never has to stretch or interpolate data.
constexpr std::size_t ColumnCapacity = 300;

// Cached one-second coefficients for a five-second EMA time constant.
constexpr double EmaInputWeight = 0.18126924692201818;
constexpr double EmaRetainedWeight = 0.8187307530779818;

struct VisibleWindow
{
    std::size_t sampleStart{0};
    std::size_t columnCount{0};
    int firstColumn{0};
};

class RollingEma
{
public:
    void reset() noexcept
    {
        m_samples.fill(0.0);
        m_emaPercent = 0.0;
        m_sampleCount = 0;
        m_nextWriteIndex = 0;
    }

    void update(double percent, bool valid) noexcept
    {
        if (!valid)
        {
            reset();
            return;
        }

        const double input = std::isfinite(percent)
            ? std::clamp(percent, 0.0, 100.0)
            : 0.0;
        if (m_sampleCount == 0)
            m_emaPercent = input;
        else
            m_emaPercent = m_emaPercent * EmaRetainedWeight + input * EmaInputWeight;

        m_samples[m_nextWriteIndex] = m_emaPercent;
        m_nextWriteIndex = (m_nextWriteIndex + 1) % ColumnCapacity;
        m_sampleCount = std::min(m_sampleCount + 1, ColumnCapacity);
    }

    [[nodiscard]] std::size_t sampleCount() const noexcept { return m_sampleCount; }
    [[nodiscard]] double emaPercent() const noexcept { return m_emaPercent; }

    // Index zero is the oldest retained sample; the newest sample is last.
    [[nodiscard]] double sampleAt(std::size_t chronologicalIndex) const noexcept
    {
        if (chronologicalIndex >= m_sampleCount)
            return 0.0;

        const std::size_t oldestIndex = m_sampleCount == ColumnCapacity
            ? m_nextWriteIndex
            : 0;
        return m_samples[(oldestIndex + chronologicalIndex) % ColumnCapacity];
    }

    // Map one retained EMA sample to one pixel. Newer history stays at the
    // right edge until the available columns fill, then advances left.
    [[nodiscard]] VisibleWindow visibleWindow(int displayWidth) const noexcept
    {
        if (displayWidth <= 0 || m_sampleCount == 0)
            return {};

        const std::size_t columnCount = std::min(
            m_sampleCount,
            static_cast<std::size_t>(displayWidth));
        return {m_sampleCount - columnCount,
                columnCount,
                displayWidth - static_cast<int>(columnCount)};
    }

private:
    std::array<double, ColumnCapacity> m_samples{};
    double m_emaPercent{0.0};
    std::size_t m_sampleCount{0};
    std::size_t m_nextWriteIndex{0};
};
} // namespace ResourceMonitorHistory

#endif // LXQTRESOURCEMONITORHISTORY_H
