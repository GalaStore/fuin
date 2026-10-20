// SPDX-License-Identifier: MIT
// image://icon/<percent-encoded name or absolute path> -> freedesktop theme icon (QIcon::fromTheme).
// Unknown names yield a transparent pixmap; QML checks DefaultApps.hasIcon() to show a glyph instead.
#pragma once

#include <QQuickImageProvider>

class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
