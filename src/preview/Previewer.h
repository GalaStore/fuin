// SPDX-License-Identifier: MIT
// QML-facing async wrapper around Inspector (runs in the thread pool, latest request wins).
#pragma once

#include <QObject>
#include <QVariantMap>

class Previewer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QVariantMap result READ result NOTIFY resultChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool parquetSupport READ parquetSupport CONSTANT)
public:
    explicit Previewer(QObject *parent = nullptr);
    QString source() const { return m_source; }
    void setSource(const QString &s);
    QVariantMap result() const { return m_result; }
    bool busy() const { return m_busy; }
    bool parquetSupport() const;
    Q_INVOKABLE void reload();
    // SQL snippet to query this data file with DuckDB.
    Q_INVOKABLE QString duckdbSql(const QString &path) const;

Q_SIGNALS:
    void sourceChanged();
    void resultChanged();
    void busyChanged();

private:
    QString m_source;
    QVariantMap m_result;
    bool m_busy = false;
    quint64 m_generation = 0;
};
