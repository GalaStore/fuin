// SPDX-License-Identifier: MIT
// Usage: bench_foldermodel [count...]   (default: 1000 10000)
#include "fs/FolderModel.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLocale>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdio>
#include <numeric>
#include <random>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setQuitLockEnabled(false);
    QStandardPaths::setTestModeEnabled(true);
    // The dirN order check needs numeric collation, which QCollator ignores under the C locale.
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QList<int> sizes;
    for (int i = 1; i < argc; ++i)
        sizes << QByteArray(argv[i]).toInt();
    if (sizes.isEmpty())
        sizes = {1000, 10000};

    int failures = 0;
    for (int n : std::as_const(sizes)) {
        for (bool shuffled : {false, true}) {
            QTemporaryDir dir;
            if (!dir.isValid()) {
                std::fprintf(stderr, "cannot create a temporary dir: %s\n", qPrintable(dir.errorString()));
                return 1;
            }
            QList<int> order(n);
            std::iota(order.begin(), order.end(), 0);
            if (shuffled)
                std::shuffle(order.begin(), order.end(), std::mt19937(42));
            for (int i : std::as_const(order))
                QDir(dir.path()).mkdir(u"dir"_s + QString::number(i));

            FolderModel m;
            m.setShowHidden(false);
            m.setSortField(u"name"_s);
            m.setSortReversed(false);
            QEventLoop loop;
            QObject::connect(&m, &FolderModel::loadingChanged, &loop, [&] {
                if (!m.loading())
                    loop.quit();
            });
            QElapsedTimer t;
            t.start();
            m.navigate(dir.path());
            loop.exec();
            const qint64 ms = t.elapsed();

            bool ok = m.rowCount() == n;
            for (int i = 0; ok && i < n; ++i)
                ok = m.itemAt(i).text() == u"dir"_s + QString::number(i);
            failures += !ok;
            std::printf("%7d folders, %-14s %6lld ms%s\n", n, shuffled ? "shuffled" : "creation order", ms, ok ? "" : "  WRONG RESULT");
            std::fflush(stdout);
        }
    }
    return failures ? 1 : 0;
}
