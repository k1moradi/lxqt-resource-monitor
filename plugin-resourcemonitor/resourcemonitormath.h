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

#ifndef LXQTRESOURCEMONITORMATH_H
#define LXQTRESOURCEMONITORMATH_H

#include <QString>

#include <QtGlobal>

namespace ResourceMonitorMath
{
[[nodiscard]] double calculatePercent(quint64 used, quint64 total);
[[nodiscard]] QString formatBytes(quint64 bytes);
} // namespace ResourceMonitorMath

#endif // LXQTRESOURCEMONITORMATH_H
