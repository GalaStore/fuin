// SPDX-License-Identifier: MIT
// "Aplicaciones por defecto": file types with their default application and the installed
// candidates, read from and written to the system (KApplicationTrader / mimeapps.list).
// Nothing is hard-coded about applications; the list of types is derived from QMimeDatabase,
// the types the user has opened and the installed handlers.
#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>

class QProcess;

class DefaultAppsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY countChanged)

public:
    enum Roles {
        MimeTypeRole = Qt::UserRole + 1,
        CommentRole,
        IconNameRole,
        PatternsRole,
        SectionRole,
        SectionStartRole,
        DefaultIdRole,
        DefaultNameRole,
        DefaultIconRole,
        DefaultTerminalRole,
        AppsRole,
    };
    Q_ENUM(Roles)

    explicit DefaultAppsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    int count() const { return int(m_rows.size()); }
    bool busy() const { return m_busy; }
    bool loaded() const { return m_loaded; }

    // (Re)builds the list of types from the MIME database and KSycoca.
    Q_INVOKABLE void reload();
    // Clears the filter, makes sure mimeType is listed and returns its row (-1 if unknown).
    Q_INVOKABLE int focusMimeType(const QString &mimeType);
    // Canonical MIME type shown at row (empty when out of range).
    Q_INVOKABLE QString mimeTypeAt(int row) const;
    // Makes storageId the default application for mimeType (mimeapps.list) and refreshes KSycoca.
    Q_INVOKABLE bool setDefault(const QString &mimeType, const QString &storageId);
    // True when the icon theme provides name (used to pick an image or a glyph fallback in QML).
    Q_INVOKABLE bool hasIcon(const QString &name) const;

Q_SIGNALS:
    void filterChanged();
    void countChanged();
    void busyChanged();
    void defaultChanged(const QString &mimeType, const QString &appName);
    void failed(const QString &text);

private:
    struct Entry {
        QString mimeType;
        QString comment;
        QString iconName;
        QStringList patterns;
        QString section;
        QString defaultId;
        QString defaultName;
        QString defaultIcon;
        bool defaultTerminal = false;
        QVariantList apps;
    };
    const Entry &entryAt(int row) const;
    Entry makeEntry(const QString &mimeType, const QString &section) const;
    void resolveApps(Entry &e) const;
    void rebuildRows();
    void refreshApps();
    void finishSycocaRefresh(const QString &mimeType, const QString &appName);

    QList<Entry> m_all;   // listed types
    QList<Entry> m_extra; // types found only through the current search
    QList<int> m_rows;    // indices into m_all (>= 0) or m_extra (encoded as -1 - i)
    QString m_filter;
    QStringList m_pinned; // types added by focusMimeType()
    bool m_busy = false;
    bool m_loaded = false;
};
