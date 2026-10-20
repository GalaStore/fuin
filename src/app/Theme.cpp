// SPDX-License-Identifier: MIT
#include "Theme.h"
#include "Config.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
// role -> (default dark colour, Material 3 snake_case alias used by plain matugen JSON output)
struct Role {
    const char *name;
    const char *def;
    const char *m3;
};
constexpr Role kRoles[] = {
    {"surface", "#16111b", "surface"},
    {"container", "#221e28", "surface_container"},
    {"containerLow", "#1e1a24", "surface_container_low"},
    {"containerLowest", "#110c16", "surface_container_lowest"},
    {"containerHigh", "#2d2833", "surface_container_high"},
    {"containerHighest", "#38333e", "surface_container_highest"},
    {"surfaceBright", "#3d3742", "surface_bright"},
    {"textMain", "#e9dfee", "on_surface"},
    {"textDim", "#ccc3d2", "on_surface_variant"},
    {"outline", "#968e9b", "outline"},
    {"outlineVariant", "#4a4450", "outline_variant"},
    {"primary", "#d8b9ff", "primary"},
    {"textOnPrimary", "#450086", "on_primary"},
    {"primaryContainer", "#6300bb", "primary_container"},
    {"textOnPrimaryContainer", "#eddcff", "on_primary_container"},
    {"secondary", "#dabde2", "secondary"},
    {"secondaryContainer", "#553f5e", "secondary_container"},
    {"textOnSecondaryContainer", "#f7d9ff", "on_secondary_container"},
    {"tertiary", "#eab6e4", "tertiary"},
    {"tertiaryContainer", "#61395f", "tertiary_container"},
    {"textOnTertiaryContainer", "#ffd6f8", "on_tertiary_container"},
    {"error", "#ffb4ab", "error"},
    {"errorContainer", "#93000a", "error_container"},
    {"textOnError", "#690005", "on_error"},
    {"textOnErrorContainer", "#ffdad6", "on_error_container"},
    {"inverseSurface", "#e9dfee", "inverse_surface"},
    {"inverseOnSurface", "#342e39", "inverse_on_surface"},
    {"inversePrimary", "#8200f3", "inverse_primary"},
    {"scrim", "#000000", "scrim"},
    {"shadow", "#000000", "shadow"},
};

QString pickFamily(const QStringList &wanted, const QString &fallback)
{
    const QStringList fams = QFontDatabase::families();
    for (const QString &w : wanted)
        if (fams.contains(w, Qt::CaseInsensitive))
            return w;
    return fallback;
}
}

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    for (const Role &r : kRoles)
        m_cur.insert(QString::fromLatin1(r.name), QColor(QString::fromLatin1(r.def)));
    m_source = QStringLiteral("builtin");

    m_sans = pickFamily({QStringLiteral("Google Sans Flex"), QStringLiteral("Inter"), QStringLiteral("Noto Sans")},
                        QFontDatabase::systemFont(QFontDatabase::GeneralFont).family());
    m_rond = m_sans == QLatin1String("Google Sans Flex");
    m_mono = pickFamily({QStringLiteral("CaskaydiaCove NF"), QStringLiteral("JetBrainsMono Nerd Font"), QStringLiteral("JetBrains Mono"),
                         QStringLiteral("Noto Sans Mono")},
                        QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
    m_icon = pickFamily({QStringLiteral("Material Symbols Rounded"), QStringLiteral("Material Symbols Outlined")}, QStringLiteral("Material Symbols Rounded"));

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(120);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        loadColors();
        loadMotion();
        rewatch();
    });
    // Watch files *and* their folders: matugen and most editors replace files atomically.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(Config::instance(), &Config::changed, this, &Theme::loadMotion);

    m_fade.setStartValue(0.0);
    m_fade.setEndValue(1.0);
    m_fade.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const qreal t = v.toReal();
        for (auto it = m_to.cbegin(); it != m_to.cend(); ++it) {
            const QColor a = m_from.value(it.key()), b = it.value();
            m_cur[it.key()] = QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                                               a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
        }
        Q_EMIT colorsChanged();
    });

    loadColors();
    loadMotion();
    rewatch();
}

int Theme::dur(int ms) const
{
    if (!m_animations)
        return 0;
    return int(ms / qMax<qreal>(0.1, m_speed));
}

void Theme::rewatch()
{
    const QStringList files{Config::instance()->colorsFile(), Config::instance()->shellSettingsFile(), Config::instance()->configFile()};
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    for (const QString &f : files) {
        const QFileInfo fi(f);
        if (fi.exists())
            m_watcher.addPath(f);
        if (fi.absoluteDir().exists())
            m_watcher.addPath(fi.absolutePath());
    }
}

void Theme::loadColors()
{
    const QString path = Config::instance()->colorsFile();
    QFile f(path);
    QHash<QString, QColor> target;
    for (const Role &r : kRoles)
        target.insert(QString::fromLatin1(r.name), QColor(QString::fromLatin1(r.def)));
    bool light = false;
    QString source = QStringLiteral("builtin");
    if (f.open(QIODevice::ReadOnly)) {
        QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        // plain `matugen --json hex` output: {"colors": {"dark": {...}, "light": {...}}} or {"colors": {role: {"dark": .., "light": ..}}}
        QString mode = o.value(QStringLiteral("mode")).toString(QStringLiteral("dark"));
        if (o.contains(QStringLiteral("colors"))) {
            const QJsonObject c = o.value(QStringLiteral("colors")).toObject();
            if (c.contains(mode))
                o = c.value(mode).toObject();
            else {
                QJsonObject flat;
                for (auto it = c.begin(); it != c.end(); ++it)
                    flat.insert(it.key(), it.value().isObject() ? it.value().toObject().value(mode) : it.value());
                o = flat;
            }
        }
        int hits = 0;
        for (const Role &r : kRoles) {
            QJsonValue v = o.value(QString::fromLatin1(r.name));
            if (v.isUndefined())
                v = o.value(QString::fromLatin1(r.m3));
            const QColor c(v.toString());
            if (c.isValid()) {
                target[QString::fromLatin1(r.name)] = c;
                ++hits;
            }
        }
        if (hits > 0) {
            light = mode == QLatin1String("light");
            source = path;
        }
    }
    applyPalette(target, light, source);
}

void Theme::applyPalette(const QHash<QString, QColor> &target, bool light, const QString &source)
{
    if (target == m_cur && light == m_light)
        return;
    m_light = light;
    m_source = source;
    if (!m_animations) {
        m_cur = target;
        Q_EMIT colorsChanged();
        return;
    }
    m_fade.stop();
    m_from = m_cur;
    m_to = target;
    m_fade.setDuration(dur(600));
    m_fade.start();
}

void Theme::loadMotion()
{
    bool anim = true;
    qreal speed = 1.0;
    QFile f(Config::instance()->shellSettingsFile());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject a = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("appearance")).toObject();
        anim = a.value(QStringLiteral("animations")).toBool(true);
        speed = a.value(QStringLiteral("animationSpeed")).toDouble(1.0);
    }
    QSettings s(Config::instance()->configFile(), QSettings::IniFormat);
    if (s.contains(QStringLiteral("appearance/animations")))
        anim = s.value(QStringLiteral("appearance/animations")).toBool();
    if (s.contains(QStringLiteral("appearance/animationSpeed")))
        speed = s.value(QStringLiteral("appearance/animationSpeed")).toDouble();
    if (qEnvironmentVariableIsSet("ARCHIVOS_NO_ANIMATIONS"))
        anim = false;
    speed = qBound<qreal>(0.1, speed <= 0 ? 1.0 : speed, 5.0);
    if (anim != m_animations || !qFuzzyCompare(speed, m_speed)) {
        m_animations = anim;
        m_speed = speed;
        Q_EMIT motionChanged();
    }
}
