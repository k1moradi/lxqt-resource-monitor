#include "resourcemonitormath.h"

#include <cmath>
#include <iostream>
#include <string>

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

void checkPercent(quint64 used, quint64 total, double expected, const char *description)
{
    check(std::abs(ResourceMonitorMath::calculatePercent(used, total) - expected) < 0.001, description);
}

void checkBytes(quint64 bytes, const char *expected, const char *description)
{
    check(ResourceMonitorMath::formatBytes(bytes).toStdString() == std::string(expected), description);
}
} // namespace

int main()
{
    sg_cpu_percents cpuSample{};
    cpuSample.user = 20.0;
    cpuSample.kernel = 10.0;
    cpuSample.nice = 5.0;
    const auto zeroEntrySample = ResourceMonitorMath::makeCpuUsageSample(&cpuSample, 0);
    check(zeroEntrySample.valid, "non-null CPU sample remains valid when reported entries is zero");
    check(std::abs(zeroEntrySample.percent - 35.0) < 0.001, "CPU usage sums user, kernel, and nice");

    sg_cpu_percents highCpuSample{};
    highCpuSample.user = 80.0;
    highCpuSample.kernel = 30.0;
    const auto clampedCpuSample = ResourceMonitorMath::makeCpuUsageSample(&highCpuSample, 0);
    check(clampedCpuSample.valid, "high non-null CPU sample remains valid");
    check(clampedCpuSample.percent == 100.0, "CPU percentage is clamped to 100");

    const auto missingCpuSample = ResourceMonitorMath::makeCpuUsageSample(nullptr, 0);
    check(!missingCpuSample.valid, "null CPU sample is unavailable");
    check(missingCpuSample.percent == 0.0, "missing CPU sample has a safe zero value");

    checkPercent(0, 0, 0.0, "zero total produces zero percent");
    checkPercent(25, 100, 25.0, "ordinary percentage");
    checkPercent(1, 3, 100.0 / 3.0, "fractional percentage precision");
    checkPercent(150, 100, 100.0, "percentage is clamped to 100");

    checkBytes(0, "0 B", "zero bytes formatting");
    checkBytes(1023, "1023 B", "bytes below KiB boundary");
    checkBytes(1024, "1.0 KiB", "KiB boundary");
    checkBytes(1024ULL * 1024, "1.0 MiB", "MiB boundary");
    checkBytes(1024ULL * 1024 * 1024, "1.0 GiB", "GiB boundary");
    checkBytes(1024ULL * 1024 * 1024 * 1024, "1.0 TiB", "TiB boundary");

    if (failures != 0)
        std::cerr << failures << " resource monitor math check(s) failed\n";
    return failures == 0 ? 0 : 1;
}
