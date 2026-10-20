// SPDX-License-Identifier: MIT
// Sidebar sections: places, projects, extra folders, drives (Solid) with usage, network, trash.
#pragma once

#include <QAbstractListModel>
#include <QTimer>

class PlacesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles { SectionRole = Qt::UserRole + 1, NameRole, UrlRole, IconRole, UsageRole, DetailRole, MountedRole, RemovableRole, UdiRole };

    explicit PlacesModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void mount(const QString &udi);
    Q_INVOKABLE void unmount(const QString &udi);
    Q_INVOKABLE void reload();
    Q_INVOKABLE QString sectionAt(int row) const { return row >= 0 && row < m_entries.size() ? m_entries.at(row).section : QString(); }

Q_SIGNALS:
    void countChanged();
    void mounted(const QString &url);
    void message(const QString &text, const QString &kind);

private:
    struct Entry {
        QString section, name, url, icon, detail, udi;
        qreal usage = -1;
        bool mounted = true, removable = false;
    };
    QList<Entry> m_entries;
    QTimer m_usageTimer;
};
