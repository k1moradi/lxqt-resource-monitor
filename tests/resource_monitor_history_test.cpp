#include "resourcemonitorhistory.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check(bool passed, const char *description)
{
    if (passed)
        return;

    std::cerr << "FAIL: " << description << '\n';
    ++failures;
}

void checkNear(double actual, double expected, const char *description)
{
    check(std::abs(actual - expected) < 0.000001, description);
}
} // namespace

int main()
{
    using ResourceMonitorHistory::ColumnCapacity;
    using ResourceMonitorHistory::RollingEma;

    RollingEma history;
    check(history.sampleCount() == 0, "history starts empty");
    check(history.visibleWindow(19).columnCount == 0, "empty history draws no columns");
    check(history.visibleWindow(0).columnCount == 0, "zero width draws no columns");
    check(history.visibleWindow(-1).columnCount == 0, "negative width draws no columns");

    history.update(75.0, true);
    check(history.sampleCount() == 1, "first update stores one sample");
    checkNear(history.emaPercent(), 75.0, "first sample initializes EMA directly");
    checkNear(history.sampleAt(0), 75.0, "first sample is the newest history value");
    auto window = history.visibleWindow(19);
    check(window.sampleStart == 0 && window.columnCount == 1 && window.firstColumn == 18,
          "new sample occupies one pixel at the right edge");

    history.update(0.0, true);
    check(history.sampleCount() == 2, "second update stores a second sample");
    checkNear(history.emaPercent(), 75.0 * ResourceMonitorHistory::EmaRetainedWeight,
              "EMA decays with the cached retained weight");
    checkNear(history.sampleAt(0), 75.0, "older sample remains to the left");
    checkNear(history.sampleAt(1), history.emaPercent(), "new EMA sample is at the right");
    window = history.visibleWindow(19);
    check(window.sampleStart == 0 && window.columnCount == 2 && window.firstColumn == 17,
          "each update adds one adjacent pixel column from the right");

    std::vector<double> expected{75.0, history.emaPercent()};
    double expectedEma = history.emaPercent();
    for (std::size_t sample = 2; sample < ColumnCapacity + 8; ++sample)
    {
        const double input = static_cast<double>(sample % 101);
        expectedEma = expectedEma * ResourceMonitorHistory::EmaRetainedWeight
            + input * ResourceMonitorHistory::EmaInputWeight;
        history.update(input, true);
        expected.push_back(expectedEma);
    }

    check(history.sampleCount() == ColumnCapacity, "history is bounded by display capacity");
    const std::size_t expectedStart = expected.size() - ColumnCapacity;
    for (std::size_t index = 0; index < ColumnCapacity; ++index)
        checkNear(history.sampleAt(index), expected[expectedStart + index],
                  "ring wrap retains samples in oldest-to-newest order");

    window = history.visibleWindow(19);
    check(window.sampleStart == ColumnCapacity - 19
              && window.columnCount == 19
              && window.firstColumn == 0,
          "19-pixel display shows 19 adjacent samples across the full width");
    window = history.visibleWindow(16);
    check(window.sampleStart == ColumnCapacity - 16
              && window.columnCount == 16
              && window.firstColumn == 0,
          "narrow display shows the newest sample per pixel without interpolation");
    window = history.visibleWindow(100);
    check(window.sampleStart == ColumnCapacity - 100
              && window.columnCount == 100
              && window.firstColumn == 0,
          "wide display maps each available pixel to one retained sample");

    for (std::size_t sample = 0; sample < 100000; ++sample)
        history.update(static_cast<double>(sample % 101), true);
    check(history.sampleCount() == ColumnCapacity,
          "long-running sampling remains bounded after many ring wraps");
    bool retainedValuesAreValid = true;
    for (std::size_t index = 0; index < history.sampleCount(); ++index)
    {
        const double value = history.sampleAt(index);
        retainedValuesAreValid = retainedValuesAreValid
            && std::isfinite(value)
            && value >= 0.0
            && value <= 100.0;
    }
    check(retainedValuesAreValid, "all retained values remain finite and clamped after long sampling");

    history.update(20.0, false);
    check(history.sampleCount() == 0, "invalid resource data clears stale history");
    checkNear(history.emaPercent(), 0.0, "invalid data clears the EMA state");
    history.update(30.0, true);
    checkNear(history.emaPercent(), 30.0, "first sample after recovery starts a fresh EMA");

    RollingEma edgeCases;
    edgeCases.update(150.0, true);
    checkNear(edgeCases.emaPercent(), 100.0, "EMA input is clamped above 100 percent");
    edgeCases.reset();
    edgeCases.update(-10.0, true);
    checkNear(edgeCases.emaPercent(), 0.0, "EMA input is clamped below zero percent");
    edgeCases.reset();
    edgeCases.update(std::numeric_limits<double>::quiet_NaN(), true);
    checkNear(edgeCases.emaPercent(), 0.0, "NaN input cannot poison the EMA");
    edgeCases.reset();
    edgeCases.update(std::numeric_limits<double>::infinity(), true);
    checkNear(edgeCases.emaPercent(), 0.0, "infinite input cannot poison the EMA");

    if (failures != 0)
        std::cerr << failures << " resource monitor history check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
