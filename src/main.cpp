// SPDX-License-Identifier: MIT
// Fūin — a keyboard-first, AI-aware file manager built on KDE Frameworks KIO and Qt Quick.
#include "ai/ContextPacker.h"
#include "ai/McpServer.h"
#include "app/AppInfo.h"
#include "app/Config.h"
#include "app/DefaultAppsModel.h"
#include "app/IconProvider.h"
#include "app/Theme.h"
#include "fs/FileOps.h"
#include "fs/FolderModel.h"
#include "fs/PlacesModel.h"
#include "fs/ThumbnailProvider.h"
#include "ipc/Ipc.h"
#include "preview/CodeHighlighter.h"
#include "preview/Previewer.h"

#include <KConfig>
#include <KConfigGroup>
#include <KService>
#include <KSharedConfig>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <iostream>

#ifndef FUIN_VERSION
#define FUIN_VERSION "0.1.0"
#endif

using namespace Qt::StringLiterals;

static QString argToUrl(const QString &arg)
{
    if (arg.isEmpty())
        return QUrl::fromLocalFile(QDir::homePath()).toString();
    const QUrl u(arg);
    if (!u.scheme().isEmpty() && u.scheme().size() > 1)
        return u.toString();
    return QUrl::fromLocalFile(QFileInfo(Config::expand(arg)).absoluteFilePath()).toString();
}

// KIO runs Terminal=true applications (vim, htop, ...) through [General] TerminalApplication of
// the application's cascading config (fuinrc -> kdeglobals), defaulting to "konsole". Outside
// Plasma that key is usually unset or names a terminal that is not installed, so opening a text
// file whose default app is vim fails. Point KIO at the terminal Fūin already resolves. The write
// goes to Fūin's own ~/.config/fuinrc and never to kdeglobals, which belongs to the desktop.
static void ensureKioTerminal()
{
    const KConfigGroup effective(KSharedConfig::openConfig(), u"General"_s);
    const QString configured = QProcess::splitCommand(effective.readPathEntry("TerminalApplication", QString())).value(0);
    if (!configured.isEmpty() && !QStandardPaths::findExecutable(configured).isEmpty())
        return;
    const QString terminal = Config::instance()->terminal();
    if (terminal.isEmpty())
        return;

    KConfig local(u"fuinrc"_s, KConfig::SimpleConfig); // app-local file only, no kdeglobals cascade
    KConfigGroup general(&local, u"General"_s);
    general.writePathEntry("TerminalApplication", terminal);
    // KTerminalLauncherJob prefers TerminalService (a desktop file id) when it is set.
    const KService::Ptr service = KService::serviceByDesktopName(QFileInfo(terminal).fileName());
    if (service && service->isApplication())
        general.writeEntry("TerminalService", service->storageId());
    else
        general.deleteEntry("TerminalService");
    local.sync();
    KSharedConfig::openConfig()->reparseConfiguration();
}

static int runHeadless(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(u"fuin"_s);
    app.setApplicationVersion(QString::fromLatin1(FUIN_VERSION));
    QCommandLineParser p;
    p.addOption({u"mcp"_s, u"Run the MCP server on stdio."_s});
    p.addOption({u"context"_s, u"Print a context pack for the given paths."_s});
    p.addOption({u"format"_s, u"Context pack format: markdown | xml."_s, u"format"_s, u"markdown"_s});
    p.addPositionalArgument(u"paths"_s, u"Paths"_s);
    p.process(app);
    if (p.isSet(u"mcp"_s)) {
        McpServer server;
        return server.run();
    }
    QStringList paths = p.positionalArguments();
    if (paths.isEmpty())
        paths << QDir::currentPath();
    const auto r = ContextPacker::build(paths, p.value(u"format"_s));
    std::cout << r.text.toStdString();
    std::cerr << "≈ " << r.tokens << " tokens · " << r.included.size() << " files (" << r.summarized << " as schema) · " << r.skipped.size()
              << " skipped · " << r.secretsBlocked << " secrets blocked" << std::endl;
    return 0;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--mcp" || a == "--context")
            return runHeadless(argc, argv);
    }

    QApplication::setApplicationName(u"fuin"_s);
    QApplication::setApplicationDisplayName(u"Fūin"_s);
    QApplication::setApplicationVersion(QString::fromLatin1(FUIN_VERSION));
    QApplication::setOrganizationDomain(u"galactuuus.github.io"_s);
    QApplication::setDesktopFileName(u"fuin"_s); // Wayland app_id / X11 class
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon::fromTheme(u"fuin"_s, QIcon(u":/assets/fuin.svg"_s)));
    QQuickStyle::setStyle(u"Basic"_s);

    QCommandLineParser p;
    p.setApplicationDescription(u"Fūin — a KIO file manager with AI features"_s);
    p.addHelpOption();
    p.addVersionOption();
    QCommandLineOption reveal(u"reveal"_s, u"Open the folder of <path> and select it."_s, u"path"_s);
    QCommandLineOption preview(u"preview"_s, u"Reveal <path> and show its preview."_s, u"path"_s);
    QCommandLineOption newWindow(u"new-window"_s, u"Always open a new window."_s);
    p.addOption(reveal);
    p.addOption(preview);
    p.addOption(newWindow);
    p.addOption({u"mcp"_s, u"Run the MCP server on stdio (for Claude Code, Cursor, ...)."_s});
    p.addOption({u"context"_s, u"Print a Markdown/XML context pack of the given paths to stdout."_s});
    p.addPositionalArgument(u"url"_s, u"Folder, file or KIO URL to open."_s, u"[url]"_s);
    p.process(app);

    ensureKioTerminal();

    const QString start = argToUrl(p.positionalArguments().value(0));
    const QString revealUrl = p.isSet(reveal) ? argToUrl(p.value(reveal)) : p.isSet(preview) ? argToUrl(p.value(preview)) : QString();

    // Single instance: forward to the running process.
    if (IpcClient::isRunning()) {
        if (!revealUrl.isEmpty())
            IpcClient::request({{u"cmd"_s, p.isSet(preview) ? u"preview"_s : u"reveal"_s}, {u"path"_s, revealUrl}});
        else
            IpcClient::request({{u"cmd"_s, u"open"_s}, {u"url"_s, start}, {u"newWindow"_s, true}});
        return 0;
    }

    auto *config = Config::instance();
    auto *theme = new Theme(&app);
    auto *ops = new FileOps(&app);
    auto *places = new PlacesModel(&app);
    auto *info = new AppInfo(&app);
    auto *defaultApps = new DefaultAppsModel(&app);
    auto *ipc = new IpcServer(ops, &app);
    ipc->listen();

    qmlRegisterSingletonInstance("Fuin", 1, 0, "Theme", theme);
    qmlRegisterSingletonInstance("Fuin", 1, 0, "Config", config);
    qmlRegisterSingletonInstance("Fuin", 1, 0, "FileOps", ops);
    qmlRegisterSingletonInstance("Fuin", 1, 0, "Places", places);
    qmlRegisterSingletonInstance("Fuin", 1, 0, "AppInfo", info);
    qmlRegisterSingletonInstance("Fuin", 1, 0, "DefaultApps", defaultApps);
    qmlRegisterSingletonInstance("Fuin", 1, 0, "Ipc", ipc);
    qmlRegisterType<FolderModel>("Fuin", 1, 0, "FolderModel");
    qmlRegisterType<Previewer>("Fuin", 1, 0, "Previewer");
    qmlRegisterType<CodeHighlighter>("Fuin", 1, 0, "CodeHighlighter");
    qmlRegisterType<ContextPack>("Fuin", 1, 0, "ContextPack");

    QQmlApplicationEngine engine;
    engine.addImageProvider(u"thumb"_s, new ThumbnailProvider);
    engine.addImageProvider(u"icon"_s, new IconProvider);
    engine.setInitialProperties({{u"startUrl"_s, revealUrl.isEmpty() ? start : revealUrl},
                                 {u"revealUrl"_s, revealUrl},
                                 {u"startPreview"_s, p.isSet(preview)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QCoreApplication::quit);
    engine.load(QUrl(u"qrc:/qml/Main.qml"_s));
    return app.exec();
}
