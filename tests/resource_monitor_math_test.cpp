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
