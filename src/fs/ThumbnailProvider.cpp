// SPDX-License-Identifier: MIT
#include "ThumbnailProvider.h"

#include <KFileItem>
#include <KIO/PreviewJob>

#include <QCoreApplication>
#include <QImage>
#include <QMutex>
#include <QPointer>
#include <QQuickTextureFactory>
#include <QUrl>

namespace {
class ThumbResponse : public QQuickImageResponse
{
public:
    ThumbResponse(const QUrl &url, const QSize &size)
    {
        QPointer<ThumbResponse> self(this);
        // KIO jobs must live in the GUI thread.
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, url, size] {
                if (!self)
                    return;
                KFileItem item(url);
                auto *job = KIO::filePreview(KFileItemList{item}, size.isValid() && !size.isEmpty() ? size : QSize(256, 256));
                job->setScaleType(KIO::PreviewJob::ScaledAndCached);
                job->setIgnoreMaximumSize(false);
                QObject::connect(job, &KIO::PreviewJob::generated, job, [self](const KFileItem &, const QImage &img) {
                    if (self)
                        self->finish(img);
                });
                QObject::connect(job, &KIO::PreviewJob::failed, job, [self](const KFileItem &) {
                    if (self)
                        self->finish(QImage());
                });
                QObject::connect(job, &KJob::result, job, [self, job] {
                    if (self && job->error())
                        self->finish(QImage());
                });
                self->m_job = job;
            },
            Qt::QueuedConnection);
    }

    void finish(const QImage &img)
    {
        {
            QMutexLocker lock(&m_mutex);
            if (m_done)
                return;
            m_done = true;
            m_image = img;
        }
        Q_EMIT finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        QMutexLocker lock(&m_mutex);
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        QMutexLocker lock(&m_mutex);
        return m_image.isNull() ? QStringLiteral("no thumbnail") : QString();
    }

    void cancel() override
    {
        QPointer<KIO::PreviewJob> job = m_job;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [job] {
                if (job)
                    job->kill();
            },
            Qt::QueuedConnection);
    }

    QPointer<KIO::PreviewJob> m_job;

private:
    mutable QMutex m_mutex;
    QImage m_image;
    bool m_done = false;
};
}

QQuickImageResponse *ThumbnailProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    QString enc = id;
    const int at = enc.lastIndexOf(QLatin1Char('@'));
    if (at > 0)
        enc = enc.left(at); // "@mtime" only busts QML's image cache
    const QUrl url(QUrl::fromPercentEncoding(enc.toLatin1()));
    return new ThumbResponse(url, requestedSize);
}
