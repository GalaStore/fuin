// SPDX-License-Identifier: MIT
#include "portal/PortalLook.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QStyle>
#include <QTemporaryDir>
#include <QTest>

class TestPortalLook : public QObject
{
    Q_OBJECT

    QTemporaryDir m_home;
    QString m_iconTheme;
    QFont m_font;
    QPalette m_palette;

    void write(const QString &relative, const QByteArray &content)
    {
        const QString path = m_home.filePath(relative);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_home.path()));
        m_iconTheme = QIcon::themeName();
        m_font = QApplication::font();
        m_palette = QApplication::palette();
    }

    void init()
    {
        QDir(m_home.path()).removeRecursively();
        QDir().mkpath(m_home.path());
        QIcon::setThemeName(m_iconTheme);
        QApplication::setFont(m_font);
        QApplication::setPalette(m_palette);
    }

    void missingFilesChangeNothing()
    {
        PortalLook::apply("qt6ct");
        PortalLook::apply("");
        QCOMPARE(QIcon::themeName(), m_iconTheme);
        QCOMPARE(QApplication::font(), m_font);
        QCOMPARE(QApplication::palette().color(QPalette::Window), m_palette.color(QPalette::Window));
    }

    void readsQt6ct()
    {
        write(QStringLiteral("qt6ct/colors/dark.conf"),
              "[ColorScheme]\n"
              "active_colors=#f8ddd1, #2b1c15, #423129, #36271f, #170b06, #261811, #f8ddd1, #f8ddd1, #f8ddd1, #261811, #1d100a\n"
              "inactive_colors=#f8ddd1, #2b1c15\n"
              "disabled_colors=#a38c82, not-a-colour\n");
        write(QStringLiteral("qt6ct/qt6ct.conf"),
              "[Appearance]\n"
              "color_scheme_path=" + QFile::encodeName(m_home.filePath(QStringLiteral("qt6ct/colors/dark.conf"))) + "\n"
              "custom_palette=true\n"
              "icon_theme=fuin-test-icons\n"
              "style=Fusion\n"
              "[Fonts]\n"
              "general=\"Fuin Test Sans,13,-1,5,400,0,0,0,0,0,0,0,0,0,0,1\"\n");
        PortalLook::apply("qt6ct");
        QCOMPARE(QIcon::themeName(), QStringLiteral("fuin-test-icons"));
        QCOMPARE(QApplication::style()->name(), QStringLiteral("fusion"));
        QCOMPARE(QApplication::font().family(), QStringLiteral("Fuin Test Sans"));
        QCOMPARE(QApplication::font().pointSize(), 13);
        QCOMPARE(QApplication::palette().color(QPalette::Active, QPalette::Window), QColor(0x1d100a));
        QCOMPARE(QApplication::palette().color(QPalette::Active, QPalette::Base), QColor(0x261811));
        QCOMPARE(QApplication::palette().color(QPalette::Disabled, QPalette::WindowText), QColor(0xa38c82));
    }

    void paletteOnlyWhenCustom()
    {
        write(QStringLiteral("qt6ct/qt6ct.conf"), "[Appearance]\ncustom_palette=false\ncolor_scheme_path=/nonexistent\nstyle=NoSuchStyle\n");
        const QString style = QApplication::style()->name();
        PortalLook::apply("qt6ct");
        QCOMPARE(QApplication::style()->name(), style);
        QCOMPARE(QApplication::palette().color(QPalette::Window), m_palette.color(QPalette::Window));
    }

    void otherThemesUseKdeOrGtkIcons()
    {
        write(QStringLiteral("qt6ct/qt6ct.conf"), "[Appearance]\nicon_theme=ignored\n");
        write(QStringLiteral("gtk-3.0/settings.ini"), "[Settings]\ngtk-icon-theme-name=gtk-icons\n");
        PortalLook::apply("gtk3");
        QCOMPARE(QIcon::themeName(), QStringLiteral("gtk-icons"));
        write(QStringLiteral("kdeglobals"), "[Icons]\nTheme=kde-icons\n");
        PortalLook::apply("kde");
        QCOMPARE(QIcon::themeName(), QStringLiteral("kde-icons"));
    }
};

QTEST_MAIN(TestPortalLook)
#include "tst_portallook.moc"
