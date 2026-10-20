// SPDX-License-Identifier: MIT
#include "AppInfo.h"

#include <QImage>
#include <QQuickWindow>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

static bool has(const char *exe)
{
    return !QStandardPaths::findExecutable(QString::fromLatin1(exe)).isEmpty();
}

QString AppInfo::version() const { return QCoreApplication::applicationVersion(); }
QString AppInfo::executable() const
{
    // Prefer the name on $PATH so snippets survive reinstalls.
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("archivos"));
    return onPath.isEmpty() ? QCoreApplication::applicationFilePath() : QStringLiteral("archivos");
}
bool AppInfo::hasFd() const { return has("fd"); }
bool AppInfo::hasRipgrep() const { return has("rg"); }
bool AppInfo::hasDuckdb() const { return has("duckdb"); }
bool AppInfo::hasOllama() const { return has("ollama"); }
bool AppInfo::hasClaude() const { return has("claude"); }
bool AppInfo::hasGit() const { return has("git"); }
QString AppInfo::homePath() const { return QDir::homePath(); }

QString AppInfo::claudeCommand() const
{
    return QStringLiteral("claude mcp add --scope user archivos -- %1 --mcp").arg(executable());
}

QString AppInfo::mcpJson() const
{
    return QStringLiteral("{\n  \"mcpServers\": {\n    \"archivos\": {\n      \"command\": \"%1\",\n      \"args\": [\"--mcp\"]\n    }\n  }\n}").arg(executable());
}

QString AppInfo::prettyPath(const QString &urlOrPath) const
{
    QUrl u(urlOrPath);
    QString p = urlOrPath;
    if (u.isLocalFile())
        p = u.toLocalFile();
    else if (!u.scheme().isEmpty() && u.scheme().size() > 1)
        return u.toDisplayString();
    const QString home = QDir::homePath();
    if (p == home)
        return QStringLiteral("~");
    if (p.startsWith(home + QLatin1Char('/')))
        return QLatin1Char('~') + p.mid(home.size());
    return p;
}

QString AppInfo::toLocal(const QString &url) const
{
    const QUrl u(url);
    return u.isLocalFile() ? u.toLocalFile() : url;
}

QString AppInfo::toUrl(const QString &path) const
{
    const QUrl u(path);
    if (!u.scheme().isEmpty() && u.scheme().size() > 1)
        return path;
    return QUrl::fromLocalFile(path).toString();
}

QStringList AppInfo::childDirs(const QString &dir, const QString &prefix) const
{
    QStringList out;
    const QStringList names = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot | (prefix.startsWith(QLatin1Char('.')) ? QDir::Hidden : QDir::Filters()), QDir::Name);
    for (const QString &n : names)
        if (n.startsWith(prefix))
            out << n;
    return out;
}

bool AppInfo::grabWindow(QObject *window, const QString &path) const
{
    auto *w = qobject_cast<QQuickWindow *>(window);
    return w && w->grabWindow().save(path);
}
