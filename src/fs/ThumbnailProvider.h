// SPDX-License-Identifier: MIT
// image://thumb/<percent-encoded url>@<mtime> -> KIO::PreviewJob thumbnail (freedesktop cache aware).
#pragma once

#include <QQuickAsyncImageProvider>

class ThumbnailProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
