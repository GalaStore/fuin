// SPDX-License-Identifier: MIT
#pragma once

#include <QLocale>
#include <QString>

namespace Format {
inline QString dataSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(qMax<qint64>(0, bytes)) + QStringLiteral(" B");
    return QLocale().formattedDataSize(bytes, 1);
}
}
