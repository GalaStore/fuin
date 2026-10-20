// SPDX-License-Identifier: MIT
// Live colour scheme + motion settings.
//
// Colours come from an optional matugen-style JSON file (watched; the whole UI cross-fades to a
// new palette when it changes). Without that file a built-in dark palette is used.
// Animation on/off and speed come from Archivos' own config or, optionally, a desktop-shell
// settings JSON (appearance.animations / appearance.animationSpeed).
#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantAnimation>

class Theme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor surface READ surface NOTIFY colorsChanged)
    Q_PROPERTY(QColor container READ container NOTIFY colorsChanged)
    Q_PROPERTY(QColor containerLow READ containerLow NOTIFY colorsChanged)
    Q_PROPERTY(QColor containerLowest READ containerLowest NOTIFY colorsChanged)
    Q_PROPERTY(QColor containerHigh READ containerHigh NOTIFY colorsChanged)
    Q_PROPERTY(QColor containerHighest READ containerHighest NOTIFY colorsChanged)
    Q_PROPERTY(QColor surfaceBright READ surfaceBright NOTIFY colorsChanged)
    Q_PROPERTY(QColor textMain READ textMain NOTIFY colorsChanged)
    Q_PROPERTY(QColor textDim READ textDim NOTIFY colorsChanged)
    Q_PROPERTY(QColor outline READ outline NOTIFY colorsChanged)
    Q_PROPERTY(QColor outlineVariant READ outlineVariant NOTIFY colorsChanged)
    Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged)
    Q_PROPERTY(QColor textOnPrimary READ textOnPrimary NOTIFY colorsChanged)
    Q_PROPERTY(QColor primaryContainer READ primaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor textOnPrimaryContainer READ textOnPrimaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondaryContainer READ secondaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor textOnSecondaryContainer READ textOnSecondaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor tertiary READ tertiary NOTIFY colorsChanged)
    Q_PROPERTY(QColor tertiaryContainer READ tertiaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor textOnTertiaryContainer READ textOnTertiaryContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor error READ error NOTIFY colorsChanged)
    Q_PROPERTY(QColor errorContainer READ errorContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor textOnError READ textOnError NOTIFY colorsChanged)
    Q_PROPERTY(QColor textOnErrorContainer READ textOnErrorContainer NOTIFY colorsChanged)
    Q_PROPERTY(QColor inverseSurface READ inverseSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor inverseOnSurface READ inverseOnSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor inversePrimary READ inversePrimary NOTIFY colorsChanged)
    Q_PROPERTY(QColor scrim READ scrim NOTIFY colorsChanged)
    Q_PROPERTY(QColor shadow READ shadow NOTIFY colorsChanged)

    Q_PROPERTY(bool light READ light NOTIFY colorsChanged)
    Q_PROPERTY(QString colorsSource READ colorsSource NOTIFY colorsChanged)
    Q_PROPERTY(bool animations READ animations NOTIFY motionChanged)
    Q_PROPERTY(qreal animationSpeed READ animationSpeed NOTIFY motionChanged)
    Q_PROPERTY(QString fontSans READ fontSans CONSTANT)
    Q_PROPERTY(QString fontMono READ fontMono CONSTANT)
    Q_PROPERTY(QString fontIcon READ fontIcon CONSTANT)
    Q_PROPERTY(bool sansHasRond READ sansHasRond CONSTANT)

public:
    explicit Theme(QObject *parent = nullptr);

    QColor surface() const { return m_cur.value(QStringLiteral("surface")); }
    QColor container() const { return m_cur.value(QStringLiteral("container")); }
    QColor containerLow() const { return m_cur.value(QStringLiteral("containerLow")); }
    QColor containerLowest() const { return m_cur.value(QStringLiteral("containerLowest")); }
    QColor containerHigh() const { return m_cur.value(QStringLiteral("containerHigh")); }
    QColor containerHighest() const { return m_cur.value(QStringLiteral("containerHighest")); }
    QColor surfaceBright() const { return m_cur.value(QStringLiteral("surfaceBright")); }
    QColor textMain() const { return m_cur.value(QStringLiteral("textMain")); }
    QColor textDim() const { return m_cur.value(QStringLiteral("textDim")); }
    QColor outline() const { return m_cur.value(QStringLiteral("outline")); }
    QColor outlineVariant() const { return m_cur.value(QStringLiteral("outlineVariant")); }
    QColor primary() const { return m_cur.value(QStringLiteral("primary")); }
    QColor textOnPrimary() const { return m_cur.value(QStringLiteral("textOnPrimary")); }
    QColor primaryContainer() const { return m_cur.value(QStringLiteral("primaryContainer")); }
    QColor textOnPrimaryContainer() const { return m_cur.value(QStringLiteral("textOnPrimaryContainer")); }
    QColor secondary() const { return m_cur.value(QStringLiteral("secondary")); }
    QColor secondaryContainer() const { return m_cur.value(QStringLiteral("secondaryContainer")); }
    QColor textOnSecondaryContainer() const { return m_cur.value(QStringLiteral("textOnSecondaryContainer")); }
    QColor tertiary() const { return m_cur.value(QStringLiteral("tertiary")); }
    QColor tertiaryContainer() const { return m_cur.value(QStringLiteral("tertiaryContainer")); }
    QColor textOnTertiaryContainer() const { return m_cur.value(QStringLiteral("textOnTertiaryContainer")); }
    QColor error() const { return m_cur.value(QStringLiteral("error")); }
    QColor errorContainer() const { return m_cur.value(QStringLiteral("errorContainer")); }
    QColor textOnError() const { return m_cur.value(QStringLiteral("textOnError")); }
    QColor textOnErrorContainer() const { return m_cur.value(QStringLiteral("textOnErrorContainer")); }
    QColor inverseSurface() const { return m_cur.value(QStringLiteral("inverseSurface")); }
    QColor inverseOnSurface() const { return m_cur.value(QStringLiteral("inverseOnSurface")); }
    QColor inversePrimary() const { return m_cur.value(QStringLiteral("inversePrimary")); }
    QColor scrim() const { return m_cur.value(QStringLiteral("scrim")); }
    QColor shadow() const { return m_cur.value(QStringLiteral("shadow")); }

    bool light() const { return m_light; }
    QString colorsSource() const { return m_source; }
    bool animations() const { return m_animations; }
    qreal animationSpeed() const { return m_speed; }
    QString fontSans() const { return m_sans; }
    QString fontMono() const { return m_mono; }
    QString fontIcon() const { return m_icon; }
    bool sansHasRond() const { return m_rond; }

    // Duration helper for QML: scaled by speed, 0 when animations are off.
    Q_INVOKABLE int dur(int ms) const;

Q_SIGNALS:
    void colorsChanged();
    void motionChanged();

private:
    void loadColors();
    void loadMotion();
    void rewatch();
    void applyPalette(const QHash<QString, QColor> &target, bool light, const QString &source);

    QHash<QString, QColor> m_cur, m_from, m_to;
    bool m_light = false;
    QString m_source;
    bool m_animations = true;
    qreal m_speed = 1.0;
    QString m_sans, m_mono, m_icon;
    bool m_rond = false;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QVariantAnimation m_fade;
};
