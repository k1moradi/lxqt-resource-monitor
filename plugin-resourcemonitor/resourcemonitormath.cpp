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

#include "resourcemonitormath.h"

#include <algorithm>

namespace ResourceMonitorMath
{
double calculatePercent(quint64 used, quint64 total)
{
    if (total == 0)
        return 0.0;

    return std::clamp((static_cast<double>(used) * 100.0) / static_cast<double>(total), 0.0, 100.0);
}

QString formatBytes(quint64 bytes)
{
    constexpr double Kibibyte = 1024.0;
    constexpr double Mebibyte = Kibibyte * 1024.0;
    constexpr double Gibibyte = Mebibyte * 1024.0;
    constexpr double Tebibyte = Gibibyte * 1024.0;

    const double byteValue = static_cast<double>(bytes);
    if (byteValue >= Tebibyte)
        return QStringLiteral("%1 TiB").arg(byteValue / Tebibyte, 0, 'f', 1);
    if (byteValue >= Gibibyte)
        return QStringLiteral("%1 GiB").arg(byteValue / Gibibyte, 0, 'f', 1);
    if (byteValue >= Mebibyte)
        return QStringLiteral("%1 MiB").arg(byteValue / Mebibyte, 0, 'f', 1);
    if (byteValue >= Kibibyte)
        return QStringLiteral("%1 KiB").arg(byteValue / Kibibyte, 0, 'f', 1);
    return QStringLiteral("%1 B").arg(bytes);
}
} // namespace ResourceMonitorMath
