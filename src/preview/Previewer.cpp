// SPDX-License-Identifier: MIT
#include "Previewer.h"
#include "Inspector.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointer>
#include <QUrl>
#include <QtConcurrent>

Previewer::Previewer(QObject *parent)
    : QObject(parent)
{
}

bool Previewer::parquetSupport() const
{
    return Inspector::hasParquet();
}

void Previewer::setSource(const QString &s)
{
    if (s == m_source)
        return;
    m_source = s;
    Q_EMIT sourceChanged();
    reload();
}

void Previewer::reload()
{
    const quint64 gen = ++m_generation;
    m_result.clear();
    Q_EMIT resultChanged();
    QString path = m_source;
    const QUrl u(m_source);
    if (u.isLocalFile())
        path = u.toLocalFile();
    if (path.isEmpty() || !QFileInfo(path).isAbsolute()) { // remote URLs: no inspection
        if (m_busy) {
            m_busy = false;
            Q_EMIT busyChanged();
        }
        return;
    }
    m_busy = true;
    Q_EMIT busyChanged();
    auto *w = new QFutureWatcher<QVariantMap>(this);
    connect(w, &QFutureWatcher<QVariantMap>::finished, this, [this, w, gen] {
        w->deleteLater();
        if (gen != m_generation)
            return;
        m_result = w->result();
        m_busy = false;
        Q_EMIT resultChanged();
        Q_EMIT busyChanged();
    });
    w->setFuture(QtConcurrent::run([path] { return Inspector::inspect(path); }));
}

QString Previewer::duckdbSql(const QString &path) const
{
    QString p = path;
    const QUrl u(path);
    if (u.isLocalFile())
        p = u.toLocalFile();
    p.replace(QLatin1Char('\''), QStringLiteral("''"));
    const QString lower = p.toLower();
    QString fn = QStringLiteral("read_csv_auto");
    if (lower.endsWith(QLatin1String(".parquet")))
        fn = QStringLiteral("read_parquet");
    else if (lower.endsWith(QLatin1String(".json")) || lower.endsWith(QLatin1String(".jsonl")) || lower.endsWith(QLatin1String(".ndjson")))
        fn = QStringLiteral("read_json_auto");
    return QStringLiteral("SELECT * FROM %1('%2') LIMIT 100;").arg(fn, p);
}
