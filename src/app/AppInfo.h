// SPDX-License-Identifier: MIT
// Runtime facts for the UI: version, optional tools present, MCP setup snippets.
#pragma once

#include <QObject>

class AppInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString executable READ executable CONSTANT)
    Q_PROPERTY(bool hasFd READ hasFd CONSTANT)
    Q_PROPERTY(bool hasRipgrep READ hasRipgrep CONSTANT)
    Q_PROPERTY(bool hasDuckdb READ hasDuckdb CONSTANT)
    Q_PROPERTY(bool hasOllama READ hasOllama CONSTANT)
    Q_PROPERTY(bool hasClaude READ hasClaude CONSTANT)
    Q_PROPERTY(bool hasGit READ hasGit CONSTANT)
    Q_PROPERTY(bool semanticSearchEnabled READ semanticSearchEnabled CONSTANT)
    Q_PROPERTY(QString homePath READ homePath CONSTANT)
public:
    using QObject::QObject;
    QString version() const;
    QString executable() const;
    bool hasFd() const;
    bool hasRipgrep() const;
    bool hasDuckdb() const;
    bool hasOllama() const;
    bool hasClaude() const;
    bool hasGit() const;
    bool semanticSearchEnabled() const { return false; } // feature flag: see docs/DESIGN.md
    QString homePath() const;
    Q_INVOKABLE QString claudeCommand() const;
    Q_INVOKABLE QString mcpJson() const;
    Q_INVOKABLE QString prettyPath(const QString &urlOrPath) const;
    Q_INVOKABLE QString toLocal(const QString &url) const;
    Q_INVOKABLE QString toUrl(const QString &path) const;
    Q_INVOKABLE bool grabWindow(QObject *window, const QString &path) const; // dev automation / docs screenshots
    Q_INVOKABLE QStringList childDirs(const QString &dir, const QString &prefix) const;
};
