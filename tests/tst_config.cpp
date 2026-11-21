// SPDX-License-Identifier: MIT
#include "app/Config.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class TestConfig : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/fuin/fuin.conf"));
        QVERIFY(Config::instance()->configFile().contains(QLatin1String(".qttest")));
    }

    void cleanupTestCase() { QFile::remove(Config::instance()->configFile()); }

    void textKeyEmptyMeansAutomatic()
    {
        Config *c = Config::instance();
        QSignalSpy terminal(c, &Config::terminalChanged);
        c->setTerminalSetting(QStringLiteral("  my-term  "));
        QCOMPARE(c->terminalSetting(), QStringLiteral("my-term"));
        QCOMPARE(c->terminal(), QStringLiteral("my-term"));
        QCOMPARE(terminal.count(), 1);
        c->setTerminalSetting(QStringLiteral("my-term"));
        QCOMPARE(terminal.count(), 1);
        c->setTerminalSetting(QString());
        QVERIFY(c->terminalSetting().isEmpty());
        QCOMPARE(c->terminal(), c->terminalDetected());
        QCOMPARE(terminal.count(), 2);
        QVERIFY(!QSettings(c->configFile(), QSettings::IniFormat).contains(QStringLiteral("apps/terminal")));
    }

    void emptyListIsKeptAndResetRestoresDefaults()
    {
        Config *c = Config::instance();
        const QStringList defaults = c->projectsSetting();
        QVERIFY(defaults.contains(QStringLiteral("~/Dev")));
        QSignalSpy places(c, &Config::placesChanged);
        c->setProjectsSetting({});
        QVERIFY(c->projectsSetting().isEmpty());
        QVERIFY(c->projectDirs().isEmpty());
        QCOMPARE(places.count(), 1);
        c->setProjectsSetting({QStringLiteral("~"), QStringLiteral("  "), QStringLiteral("/no/such/dir")});
        QCOMPARE(c->projectsSetting(), (QStringList{QStringLiteral("~"), QStringLiteral("/no/such/dir")}));
        QCOMPARE(c->projectDirs(), QStringList{QDir::homePath()});
        c->resetToDefault(QStringLiteral("places/projects"));
        QCOMPARE(c->projectsSetting(), defaults);
        QCOMPARE(places.count(), 3);
    }

    void extraPlacesAndMcpRoots()
    {
        Config *c = Config::instance();
        c->setExtraPlacesSetting({QStringLiteral("Home again|~|star"), QStringLiteral("broken")});
        const QVariantList extra = c->extraPlaces();
        QCOMPARE(extra.size(), 1);
        QCOMPARE(extra.first().toMap().value(QStringLiteral("path")).toString(), QDir::homePath());

        QCOMPARE(c->mcpRoots(), QStringList{QDir::homePath()});
        c->setMcpRootsSetting({QStringLiteral("/tmp"), QStringLiteral("~/x/../y")});
        QCOMPARE(c->mcpRoots(), (QStringList{QStringLiteral("/tmp"), QDir::homePath() + QStringLiteral("/y")}));
        c->setMcpRootsSetting({});
        QVERIFY(c->mcpRoots().isEmpty());
        c->resetToDefault(QStringLiteral("mcp/roots"));
        QCOMPARE(c->mcpRoots(), QStringList{QDir::homePath()});
    }

    void themeFilesAndMotion()
    {
        Config *c = Config::instance();
        QSignalSpy theme(c, &Config::themeFilesChanged);
        c->setColorsFileSetting(QStringLiteral("~/colors.json"));
        QCOMPARE(c->colorsFile(), QDir::homePath() + QStringLiteral("/colors.json"));
        c->setShellSettingsFileSetting(QStringLiteral("/etc/shell.json"));
        QCOMPARE(c->shellSettingsFile(), QStringLiteral("/etc/shell.json"));
        QCOMPARE(theme.count(), 2);
        c->setShellSettingsFileSetting(QString());
        QVERIFY(c->shellSettingsFile().endsWith(QLatin1String("/rice/settings.json")));

        QVERIFY(!c->motionOverridden());
        c->setAnimationSpeed(9);
        QVERIFY(c->motionOverridden());
        QSettings s(c->configFile(), QSettings::IniFormat);
        QCOMPARE(s.value(QStringLiteral("appearance/animationSpeed")).toDouble(), 5.0);
        c->resetToDefault(QStringLiteral("appearance/animationSpeed"));
        QVERIFY(!c->motionOverridden());
    }

    void helpers()
    {
        Config *c = Config::instance();
        QVERIFY(c->isDir(QStringLiteral("~")));
        QVERIFY(!c->isDir(QStringLiteral("/no/such/dir")));
        QVERIFY(c->isFile(c->configFile()));
        QVERIFY(!c->isFile(QStringLiteral("~")));
        QVERIFY(c->hasProgram(QStringLiteral("sh")));
        QVERIFY(c->hasProgram(QStringLiteral(" /bin/sh ")));
        QVERIFY(!c->hasProgram(QStringLiteral("no-such-program-fuin")));
        QVERIFY(!c->hasProgram(QString()));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "tst_config.moc"
