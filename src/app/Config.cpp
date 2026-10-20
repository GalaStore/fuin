// SPDX-License-Identifier: MIT
#include "Config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/archivos/archivos.conf");
}
}

Config *Config::instance()
{
    static Config *s = new Config;
    return s;
}

Config::Config(QObject *parent)
    : QObject(parent)
    , m_settings(configPath(), QSettings::IniFormat)
{
}

QString Config::homePath()
{
    return QDir::homePath();
}

QString Config::expand(const QString &path)
{
    QString p = path.trimmed();
    if (p == QLatin1String("~"))
        return homePath();
    if (p.startsWith(QLatin1String("~/")))
        return homePath() + p.mid(1);
    if (p.startsWith(QLatin1String("$HOME")))
        return homePath() + p.mid(5);
    return p;
}

QString Config::socketPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    return dir + QStringLiteral("/archivos.sock");
}

QVariant Config::value(const QString &key, const QVariant &def) const
{
    return m_settings.value(key, def);
}

void Config::set(const QString &key, const QVariant &v)
{
    if (m_settings.value(key) == v)
        return;
    m_settings.setValue(key, v);
    Q_EMIT changed();
}

void Config::sync()
{
    m_settings.sync();
    Q_EMIT changed();
}

QString Config::viewMode() const { return value(QStringLiteral("view/mode"), QStringLiteral("grid")).toString(); }
void Config::setViewMode(const QString &v) { set(QStringLiteral("view/mode"), v); }
bool Config::showHidden() const { return value(QStringLiteral("view/showHidden"), false).toBool(); }
void Config::setShowHidden(bool v) { set(QStringLiteral("view/showHidden"), v); }
bool Config::showPreview() const { return value(QStringLiteral("view/showPreview"), true).toBool(); }
void Config::setShowPreview(bool v) { set(QStringLiteral("view/showPreview"), v); }
QString Config::sortField() const { return value(QStringLiteral("view/sortField"), QStringLiteral("name")).toString(); }
void Config::setSortField(const QString &v) { set(QStringLiteral("view/sortField"), v); }
bool Config::sortReversed() const { return value(QStringLiteral("view/sortReversed"), false).toBool(); }
void Config::setSortReversed(bool v) { set(QStringLiteral("view/sortReversed"), v); }
int Config::iconSize() const { return value(QStringLiteral("view/iconSize"), 96).toInt(); }
void Config::setIconSize(int v) { set(QStringLiteral("view/iconSize"), qBound(48, v, 192)); }

bool Config::mcpAllowWrite() const
{
    // Re-read from disk: the MCP process and the GUI are different processes.
    QSettings fresh(configPath(), QSettings::IniFormat);
    return fresh.value(QStringLiteral("mcp/allowWrite"), false).toBool();
}
void Config::setMcpAllowWrite(bool v)
{
    set(QStringLiteral("mcp/allowWrite"), v);
    m_settings.sync();
}
QString Config::contextFormat() const { return value(QStringLiteral("ai/contextFormat"), QStringLiteral("markdown")).toString(); }
void Config::setContextFormat(const QString &v) { set(QStringLiteral("ai/contextFormat"), v); }
QString Config::ollamaUrl() const { return value(QStringLiteral("ai/ollamaUrl"), QStringLiteral("http://127.0.0.1:11434")).toString(); }

QStringList Config::mcpRoots() const
{
    QSettings fresh(configPath(), QSettings::IniFormat);
    const QStringList raw = fresh.value(QStringLiteral("mcp/roots"), QStringList{QStringLiteral("~")}).toStringList();
    QStringList out;
    for (const QString &r : raw) {
        const QString p = QDir::cleanPath(expand(r));
        if (!p.isEmpty())
            out << p;
    }
    return out;
}

QString Config::configFile() const { return m_settings.fileName(); }

QString Config::colorsFile() const
{
    // 1. explicit setting, 2. env, 3. own cache file, 4. the common Quickshell/matugen location.
    const QString set = value(QStringLiteral("theme/colorsFile"), QString()).toString();
    if (!set.isEmpty())
        return expand(set);
    const QString env = qEnvironmentVariable("ARCHIVOS_COLORS");
    if (!env.isEmpty())
        return expand(env);
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    const QString own = cache + QStringLiteral("/archivos/colors.json");
    if (QFileInfo::exists(own))
        return own;
    return cache + QStringLiteral("/qs/colors.json");
}

QString Config::shellSettingsFile() const
{
    const QString set = value(QStringLiteral("theme/shellSettingsFile"), QString()).toString();
    if (!set.isEmpty())
        return expand(set);
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/rice/settings.json");
}

QString Config::shellApp(const QString &key) const
{
    QFile f(shellSettingsFile());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto apps = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("apps")).toObject();
    return apps.value(key).toString();
}

static QString firstInPath(const QStringList &candidates)
{
    for (const QString &c : candidates)
        if (!c.isEmpty() && !QStandardPaths::findExecutable(c).isEmpty())
            return c;
    return {};
}

QString Config::terminal() const
{
    const QString set = value(QStringLiteral("apps/terminal"), QString()).toString();
    if (!set.isEmpty())
        return set;
    return firstInPath({shellApp(QStringLiteral("terminal")), qEnvironmentVariable("TERMINAL"), QStringLiteral("kitty"), QStringLiteral("foot"),
                        QStringLiteral("alacritty"), QStringLiteral("wezterm"), QStringLiteral("konsole"), QStringLiteral("gnome-terminal")});
}

QString Config::editor() const
{
    const QString set = value(QStringLiteral("apps/editor"), QString()).toString();
    if (!set.isEmpty())
        return set;
    return firstInPath({shellApp(QStringLiteral("editor")), QStringLiteral("cursor"), QStringLiteral("code"), QStringLiteral("codium"), QStringLiteral("zed"),
                        QStringLiteral("kate")});
}

QStringList Config::projectDirs() const
{
    const QStringList raw = value(QStringLiteral("places/projects"),
                                  QStringList{QStringLiteral("~/Dev"), QStringLiteral("~/Projects"), QStringLiteral("~/src"), QStringLiteral("~/code")})
                                .toStringList();
    QStringList out;
    for (const QString &r : raw) {
        const QString p = expand(r);
        if (QFileInfo(p).isDir())
            out << p;
    }
    return out;
}

QVariantList Config::extraPlaces() const
{
    // name|path|icon entries; only existing folders are shown.
    const QStringList raw = value(QStringLiteral("places/extra"),
                                  QStringList{QStringLiteral("Fondos|~/Wallpapers|wallpaper"), QStringLiteral("Fondos|~/Pictures/Wallpapers|wallpaper"),
                                              QStringLiteral("Modelos IA|~/.cache/huggingface/hub|neurology"),
                                              QStringLiteral("Modelos Ollama|~/.ollama/models|neurology")})
                                .toStringList();
    QVariantList out;
    QStringList seenNames;
    for (const QString &r : raw) {
        const QStringList f = r.split(QLatin1Char('|'));
        if (f.size() < 2)
            continue;
        const QString p = expand(f.at(1));
        if (!QFileInfo(p).isDir() || seenNames.contains(f.at(0)))
            continue;
        seenNames << f.at(0);
        out << QVariantMap{{QStringLiteral("name"), f.at(0)}, {QStringLiteral("path"), p},
                           {QStringLiteral("icon"), f.size() > 2 ? f.at(2) : QStringLiteral("folder")}};
    }
    return out;
}
