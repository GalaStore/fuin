// SPDX-License-Identifier: MIT
#include "IconProvider.h"

#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QUrl>

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString name = QUrl::fromPercentEncoding(id.toUtf8());
    const int side = requestedSize.width() > 0 ? requestedSize.width() : 32;
    QIcon icon;
    if (QFileInfo(name).isAbsolute()) {
        if (QFileInfo::exists(name))
            icon = QIcon(name);
    } else if (!name.isEmpty()) {
        icon = QIcon::fromTheme(name);
    }
    QPixmap pm = icon.isNull() ? QPixmap() : icon.pixmap(side, side);
    if (pm.isNull()) {
        pm = QPixmap(side, side);
        pm.fill(Qt::transparent);
    }
    if (size)
        *size = pm.size();
    return pm;
}
