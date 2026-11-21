// SPDX-License-Identifier: MIT
#include "PortalLook.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

namespace {
QString configFile(const QString &relative)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u'/' + relative;
}

// QSettings splits unquoted values at commas.
QString text(const QVariant &v)
{
    return v.userType() == QMetaType::QStringList ? v.toStringList().join(u',') : v.toString();
}

// qt6ct colour schemes list one colour per QPalette::ColorRole, in enum order.
bool readPalette(const QString &path, QPalette &palette)
{
    if (!QFile::exists(path))
        return false;
    const QSettings s(path, QSettings::IniFormat);
    const struct {
        const char *key;
        QPalette::ColorGroup group;
    } groups[] = {{"ColorScheme/active_colors", QPalette::Active},
                  {"ColorScheme/inactive_colors", QPalette::Inactive},
                  {"ColorScheme/disabled_colors", QPalette::Disabled}};
    bool any = false;
    for (const auto &g : groups) {
        const QStringList colors = text(s.value(QLatin1String(g.key))).split(u',', Qt::SkipEmptyParts);
        for (int i = 0; i < colors.size() && i < QPalette::NColorRoles; ++i) {
            const QColor c = QColor::fromString(colors.at(i).trimmed());
            if (c.isValid()) {
                palette.setColor(g.group, QPalette::ColorRole(i), c);
                any = true;
            }
        }
    }
    return any;
}

QString otherIconTheme()
{
    const QSettings kde(configFile(QStringLiteral("kdeglobals")), QSettings::IniFormat);
    const QString kdeTheme = kde.value(QStringLiteral("Icons/Theme")).toString();
    if (!kdeTheme.isEmpty())
        return kdeTheme;
    const QSettings gtk(configFile(QStringLiteral("gtk-3.0/settings.ini")), QSettings::IniFormat);
    return gtk.value(QStringLiteral("Settings/gtk-icon-theme-name")).toString();
}
}

void PortalLook::apply(const QByteArray &platformTheme)
{
    const bool qtct = platformTheme.startsWith("qt6ct") || platformTheme.startsWith("qt5ct");
    const QString qtctFile = configFile(QStringLiteral("qt6ct/qt6ct.conf"));
    if (!qtct || !QFile::exists(qtctFile)) {
        if (const QString icons = otherIconTheme(); !icons.isEmpty())
            QIcon::setThemeName(icons);
        return;
    }

    const QSettings s(qtctFile, QSettings::IniFormat);
    if (const QString icons = s.value(QStringLiteral("Appearance/icon_theme")).toString(); !icons.isEmpty())
        QIcon::setThemeName(icons);
    if (const QString style = s.value(QStringLiteral("Appearance/style")).toString();
        !style.isEmpty() && QStyleFactory::keys().contains(style, Qt::CaseInsensitive))
        QApplication::setStyle(style);

    QFont font;
    if (const QString f = text(s.value(QStringLiteral("Fonts/general"))); !f.isEmpty() && font.fromString(f))
        QApplication::setFont(font);

    QPalette palette = QApplication::style()->standardPalette();
    if (s.value(QStringLiteral("Appearance/custom_palette")).toBool()
        && readPalette(s.value(QStringLiteral("Appearance/color_scheme_path")).toString(), palette)) {
        QApplication::setPalette(palette);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QGuiApplication::styleHints()->setColorScheme(palette.color(QPalette::Window).lightness() < 128 ? Qt::ColorScheme::Dark
                                                                                                         : Qt::ColorScheme::Light);
#endif
    }
}
