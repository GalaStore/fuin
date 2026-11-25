// SPDX-License-Identifier: MIT
#include "Terminal.h"

#include <QFileInfo>
#include <QStandardPaths>

namespace Terminal {

namespace {

[[maybe_unused]] QString firstInPath(const QStringList &candidates)
{
    for (const QString &c : candidates)
        if (!c.isEmpty() && !QStandardPaths::findExecutable(c).isEmpty())
            return c;
    return {};
}

}

#if defined(Q_OS_WIN)

QString detect()
{
    return firstInPath({QStringLiteral("wt.exe"), QStringLiteral("cmd.exe")});
}

Launch launch(const QString &terminal, const QString &dir, const QStringList &command)
{
    const QString bin = QFileInfo(terminal).fileName().toLower();
    if (bin == QLatin1String("wt.exe") || bin == QLatin1String("wt")) {
        Launch l{terminal, {QStringLiteral("-d"), dir}, dir};
        if (!command.isEmpty())
            l.args << command;
        return l;
    }
    if (bin == QLatin1String("cmd.exe") || bin == QLatin1String("cmd")) {
        Launch l{QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QStringLiteral("start"), QString(), QStringLiteral("/d"), dir, terminal}, dir};
        if (!command.isEmpty())
            l.args << QStringLiteral("/k") << command;
        return l;
    }
    return {terminal, {}, dir};
}

#elif defined(Q_OS_MACOS)

QString detect()
{
    return QStringLiteral("Terminal");
}

static QString shellQuote(const QString &s)
{
    QString q = s;
    q.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + q + QLatin1Char('\'');
}

static QString appleScriptQuote(const QString &s)
{
    QString q = s;
    q.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    q.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
}

Launch launch(const QString &terminal, const QString &dir, const QStringList &command)
{
    if (command.isEmpty())
        return {QStringLiteral("open"), {QStringLiteral("-a"), terminal, dir}, dir};
    QStringList quoted;
    for (const QString &word : command)
        quoted << shellQuote(word);
    const QString script = QStringLiteral("cd %1 && %2").arg(shellQuote(dir), quoted.join(QLatin1Char(' ')));
    return {QStringLiteral("osascript"),
            {QStringLiteral("-e"), QStringLiteral("tell application %1 to activate").arg(appleScriptQuote(terminal)), QStringLiteral("-e"),
             QStringLiteral("tell application %1 to do script %2").arg(appleScriptQuote(terminal), appleScriptQuote(script))},
            dir};
}

#else

QString detect()
{
    return firstInPath({qEnvironmentVariable("TERMINAL"), QStringLiteral("xdg-terminal-exec"), QStringLiteral("kitty"), QStringLiteral("foot"),
                        QStringLiteral("alacritty"), QStringLiteral("wezterm"), QStringLiteral("konsole"), QStringLiteral("gnome-terminal")});
}

Launch launch(const QString &terminal, const QString &dir, const QStringList &command)
{
    const QString bin = QFileInfo(terminal).fileName();
    Launch l{terminal, {}, dir};
    if (bin == QLatin1String("kitty"))
        l.args << QStringLiteral("--directory") << dir;
    else if (bin == QLatin1String("foot") || bin == QLatin1String("alacritty"))
        l.args << QStringLiteral("--working-directory") << dir;
    else if (bin == QLatin1String("konsole"))
        l.args << QStringLiteral("--workdir") << dir;
    else if (bin == QLatin1String("wezterm"))
        l.args << QStringLiteral("start") << QStringLiteral("--cwd") << dir;
    else if (bin == QLatin1String("gnome-terminal"))
        l.args << QStringLiteral("--working-directory=") + dir;
    // xdg-terminal-exec: the working directory is inherited and the command follows directly.
    if (!command.isEmpty()) {
        if (bin == QLatin1String("gnome-terminal"))
            l.args << QStringLiteral("--");
        else if (bin != QLatin1String("wezterm") && bin != QLatin1String("xdg-terminal-exec"))
            l.args << QStringLiteral("-e");
        l.args << command;
    }
    return l;
}

#endif

}
