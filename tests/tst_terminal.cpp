// SPDX-License-Identifier: MIT
#include "platform/Terminal.h"

#include <QTest>

using namespace Qt::StringLiterals;

class TestTerminal : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directoryArguments_data()
    {
        QTest::addColumn<QString>("terminal");
        QTest::addColumn<QStringList>("args");
        QTest::newRow("kitty") << u"/usr/bin/kitty"_s << QStringList{u"--directory"_s, u"/work"_s};
        QTest::newRow("foot") << u"foot"_s << QStringList{u"--working-directory"_s, u"/work"_s};
        QTest::newRow("alacritty") << u"alacritty"_s << QStringList{u"--working-directory"_s, u"/work"_s};
        QTest::newRow("konsole") << u"konsole"_s << QStringList{u"--workdir"_s, u"/work"_s};
        QTest::newRow("wezterm") << u"wezterm"_s << QStringList{u"start"_s, u"--cwd"_s, u"/work"_s};
        QTest::newRow("gnome-terminal") << u"gnome-terminal"_s << QStringList{u"--working-directory=/work"_s};
        QTest::newRow("xdg-terminal-exec") << u"xdg-terminal-exec"_s << QStringList{};
        QTest::newRow("unknown") << u"mytty"_s << QStringList{};
    }

    void directoryArguments()
    {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        QSKIP("Linux launch rules");
#else
        QFETCH(QString, terminal);
        QFETCH(QStringList, args);
        const Terminal::Launch l = Terminal::launch(terminal, u"/work"_s, QStringList());
        QCOMPARE(l.program, terminal);
        QCOMPARE(l.args, args);
        QCOMPARE(l.workingDirectory, u"/work"_s);
#endif
    }

    void commandArguments_data()
    {
        QTest::addColumn<QString>("terminal");
        QTest::addColumn<QStringList>("tail");
        QTest::newRow("kitty") << u"kitty"_s << QStringList{u"-e"_s, u"ls"_s, u"-la"_s, u"a b"_s};
        QTest::newRow("konsole") << u"konsole"_s << QStringList{u"-e"_s, u"ls"_s, u"-la"_s, u"a b"_s};
        QTest::newRow("gnome-terminal") << u"gnome-terminal"_s << QStringList{u"--"_s, u"ls"_s, u"-la"_s, u"a b"_s};
        QTest::newRow("wezterm") << u"wezterm"_s << QStringList{u"ls"_s, u"-la"_s, u"a b"_s};
        QTest::newRow("xdg-terminal-exec") << u"xdg-terminal-exec"_s << QStringList{u"ls"_s, u"-la"_s, u"a b"_s};
    }

    void commandArguments()
    {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        QSKIP("Linux launch rules");
#else
        QFETCH(QString, terminal);
        QFETCH(QStringList, tail);
        const QStringList args = Terminal::launch(terminal, u"/work"_s, {u"ls"_s, u"-la"_s, u"a b"_s}).args;
        QVERIFY(args.size() >= tail.size());
        QCOMPARE(args.mid(args.size() - tail.size()), tail);
#endif
    }
};

QTEST_GUILESS_MAIN(TestTerminal)
#include "tst_terminal.moc"
