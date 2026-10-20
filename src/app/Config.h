// SPDX-License-Identifier: MIT
// Persistent preferences and resolved external paths (XDG based, nothing hard-coded).
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantList>

class Config : public QObject
{
    Q_OBJECT
    // view preferences (persisted)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY changed)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY changed)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY changed)
    Q_PROPERTY(QString sortField READ sortField WRITE setSortField NOTIFY changed)
    Q_PROPERTY(bool sortReversed READ sortReversed WRITE setSortReversed NOTIFY changed)
    Q_PROPERTY(int iconSize READ iconSize WRITE setIconSize NOTIFY changed)
    // apps
    Q_PROPERTY(QString terminal READ terminal NOTIFY changed)
    Q_PROPERTY(QString editor READ editor NOTIFY changed)
    // AI / MCP
    Q_PROPERTY(bool mcpAllowWrite READ mcpAllowWrite WRITE setMcpAllowWrite NOTIFY changed)
    Q_PROPERTY(QString contextFormat READ contextFormat WRITE setContextFormat NOTIFY changed)
    Q_PROPERTY(QString ollamaUrl READ ollamaUrl CONSTANT)
    Q_PROPERTY(QString configFile READ configFile CONSTANT)

public:
    static Config *instance();

    QString viewMode() const;
    void setViewMode(const QString &v);
    bool showHidden() const;
    void setShowHidden(bool v);
    bool showPreview() const;
    void setShowPreview(bool v);
    QString sortField() const;
    void setSortField(const QString &v);
    bool sortReversed() const;
    void setSortReversed(bool v);
    int iconSize() const;
    void setIconSize(int v);

    QString terminal() const;
    QString editor() const;

    bool mcpAllowWrite() const;
    void setMcpAllowWrite(bool v);
    QString contextFormat() const;
    void setContextFormat(const QString &v);
    QString ollamaUrl() const;
    QStringList mcpRoots() const;

    QString configFile() const;

    // MIME types of files opened from Fūin, most recent first (feeds "Aplicaciones por defecto").
    QStringList openedMimeTypes() const;
    void noteOpenedMimeType(const QString &mimeType);

    // External, optional files.
    QString colorsFile() const;        // matugen-style JSON palette
    QString shellSettingsFile() const; // optional desktop-shell settings (appearance.animations ...)

    // Places
    QStringList projectDirs() const;   // existing project folders (e.g. ~/Dev)
    QVariantList extraPlaces() const;  // [{name, path, icon}] that exist

    static QString expand(const QString &path);
    static QString homePath();
    static QString socketPath();

    Q_INVOKABLE void sync();

Q_SIGNALS:
    void changed();

private:
    explicit Config(QObject *parent = nullptr);
    QVariant value(const QString &key, const QVariant &def) const;
    void set(const QString &key, const QVariant &v);
    QString shellApp(const QString &key) const;

    QSettings m_settings;
};
