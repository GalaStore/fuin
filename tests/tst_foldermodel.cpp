// SPDX-License-Identifier: MIT
#include "fs/FolderModel.h"

#include <KIO/SimpleJob>

#include <QAbstractItemModelTester>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <numeric>
#include <random>
#include <utime.h>

using namespace Qt::StringLiterals;

namespace {
constexpr int kTimeout = 10000;

class Model : public FolderModel
{
public:
    Model()
    {
        setShowHidden(false);
        setSortField(u"name"_s);
        setSortReversed(false);
    }

    bool list(const QString &dir)
    {
        return waitListed([&] { navigate(dir); });
    }

    bool relist()
    {
        return waitListed([&] { refresh(); });
    }

    template<typename Start>
    bool waitListed(Start start)
    {
        bool done = false;
        const auto c = connect(this, &FolderModel::loadingChanged, this, [&] { done |= !loading(); });
        start();
        const bool ok = QTest::qWaitFor([&] { return done; }, kTimeout);
        disconnect(c);
        return ok;
    }

    QStringList names() const
    {
        QStringList out;
        for (int i = 0; i < rowCount(); ++i)
            out << itemAt(i).text();
        return out;
    }

    QString currentName() const { return itemAt(currentIndex()).text(); }

    QSet<QString> selectedNames() const
    {
        QSet<QString> out;
        for (const QString &p : selectedPaths())
            out << QFileInfo(p).fileName();
        return out;
    }

    void select(const QStringList &names, const QString &current)
    {
        QList<int> rows;
        for (const QString &n : names)
            rows << indexOfUrl(path(n));
        setSelection(rows, false);
        setCurrentIndex(indexOfUrl(path(current)));
    }

    QString path(const QString &name) const { return localPath() + u'/' + name; }

    QAbstractItemModelTester tester{this, QAbstractItemModelTester::FailureReportingMode::Fatal};
};

void createFile(const QString &path)
{
    if (!QFile(path).open(QIODevice::WriteOnly))
        qFatal("cannot create %s", qPrintable(path));
}

QString entryName(int i)
{
    return (i % 5 == 0 ? (i % 2 ? u"Dir"_s : u"dir"_s) : (i % 2 ? u"File"_s : u"file"_s)) + QString::number(i);
}

// Returns the visible names in the model's order: folders first, then numeric case-insensitive names.
QStringList populate(const QString &dir, int n, bool shuffled)
{
    QList<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    if (shuffled)
        std::shuffle(order.begin(), order.end(), std::mt19937(42));
    for (int i : std::as_const(order)) {
        if (i % 5 == 0) {
            QDir(dir).mkdir(entryName(i));
        } else {
            createFile(dir + u'/' + entryName(i));
            if (i % 7 == 0)
                createFile(dir + u"/.hidden"_s + QString::number(i));
        }
    }
    QStringList dirs, files;
    for (int i = 0; i < n; ++i)
        (i % 5 == 0 ? dirs : files) << entryName(i);
    return dirs + files;
}

void touch(const QString &path, qint64 secs)
{
    const utimbuf t{secs, secs};
    utime(QFile::encodeName(path).constData(), &t);
}

bool kioRename(const QString &from, const QString &to)
{
    return KIO::rename(QUrl::fromLocalFile(from), QUrl::fromLocalFile(to), KIO::HideProgressInfo)->exec();
}
}

class TestFolderModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // FolderModel's collator follows the default locale, and under C it ignores numeric and case-insensitive mode.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        QCoreApplication::instance()->setQuitLockEnabled(false);
    }

    void listing_data()
    {
        QTest::addColumn<bool>("shuffled");
        QTest::newRow("creation order") << false;
        QTest::newRow("shuffled") << true;
    }

    void listing()
    {
        QFETCH(bool, shuffled);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList expected = populate(dir.path(), 3000, shuffled);
        Model m;
        QVERIFY(m.list(dir.path()));
        QCOMPARE(m.names(), expected);
    }

    void hiddenAndFilter()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList expected = populate(dir.path(), 300, true);
        Model m;
        QVERIFY(m.list(dir.path()));
        QCOMPARE(m.names(), expected);

        m.setShowHidden(true);
        QStringList all = m.names();
        QStringList want = expected + QDir(dir.path()).entryList(QDir::Hidden | QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        want.removeDuplicates();
        std::sort(all.begin(), all.end());
        std::sort(want.begin(), want.end());
        QCOMPARE(all, want);
        QVERIFY(all.size() > expected.size());

        m.setShowHidden(false);
        QCOMPARE(m.names(), expected);

        m.setFilter(u"FILE1"_s);
        QStringList filtered;
        for (const QString &n : expected)
            if (n.contains(u"file1"_s, Qt::CaseInsensitive))
                filtered << n;
        QVERIFY(!filtered.isEmpty());
        QCOMPARE(m.names(), filtered);
        m.setFilter({});
        QCOMPARE(m.names(), expected);
    }

    void refreshMovesByTime_data()
    {
        QTest::addColumn<int>("moved");
        QTest::newRow("few") << 3;
        QTest::newRow("many") << 60;
    }

    void refreshMovesByTime()
    {
        QFETCH(int, moved);
        constexpr int n = 500;
        constexpr qint64 base = 1600000000;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QList<qint64> mtime(n);
        for (int i = 0; i < n; ++i) {
            const QString p = dir.filePath(u"f"_s + QString::number(i));
            createFile(p);
            touch(p, mtime[i] = base + i);
        }
        const auto expected = [&] {
            QList<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::sort(idx.begin(), idx.end(), [&](int a, int b) { return mtime.at(a) > mtime.at(b); });
            QStringList out;
            for (int i : idx)
                out << u"f"_s + QString::number(i);
            return out;
        };

        Model m;
        m.setSortField(u"time"_s);
        QVERIFY(m.list(dir.path()));
        QCOMPARE(m.names(), expected());

        m.select({u"f7"_s, u"f100"_s, u"f101"_s}, u"f7"_s);
        for (int t = 0; t < moved; ++t) {
            const int i = 7 * t;
            touch(dir.filePath(u"f"_s + QString::number(i)), mtime[i] = t % 2 ? base + 10000 + t : base - 10000 - t);
        }
        QVERIFY(m.relist());
        QCOMPARE(m.names(), expected());
        QCOMPARE(m.selectedNames(), (QSet<QString>{u"f7"_s, u"f100"_s, u"f101"_s}));
        QCOMPARE(m.currentName(), u"f7"_s);
    }

    void renameLeavesAndEnters()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("KIO rename notifications need a D-Bus session bus (run under dbus-run-session)");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList expected = populate(dir.path(), 100, false);
        Model m;
        QVERIFY(m.list(dir.path()));
        m.select({u"file2"_s, u"file4"_s}, u"file4"_s);

        QVERIFY(kioRename(m.path(u"file2"_s), m.path(u".file2"_s)));
        QTRY_COMPARE_WITH_TIMEOUT(m.rowCount(), int(expected.size()) - 1, kTimeout);
        QCOMPARE(m.indexOfUrl(m.path(u"file2"_s)), -1);
        QCOMPARE(m.selectedNames(), QSet<QString>{u"file4"_s});
        QCOMPARE(m.currentName(), u"file4"_s);

        QVERIFY(kioRename(m.path(u".file2"_s), m.path(u"file2"_s)));
        QTRY_COMPARE_WITH_TIMEOUT(m.rowCount(), int(expected.size()), kTimeout);
        QCOMPARE(m.names(), expected);

        m.setFilter(u"file"_s);
        m.setCurrentIndex(m.indexOfUrl(m.path(u"file4"_s)));
        const int shown = m.rowCount();
        QVERIFY(kioRename(m.path(u"file6"_s), m.path(u"other6"_s)));
        QTRY_COMPARE_WITH_TIMEOUT(m.rowCount(), shown - 1, kTimeout);
        QCOMPARE(m.indexOfUrl(m.path(u"other6"_s)), -1);
        QCOMPARE(m.currentName(), u"file4"_s);
    }

    void massDelete()
    {
        constexpr int n = 2000;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QStringList survivors;
        for (int i = 0; i < n; ++i) {
            createFile(dir.filePath(u"f"_s + QString::number(i)));
            if (i % 2)
                survivors << u"f"_s + QString::number(i);
        }
        Model m;
        QVERIFY(m.list(dir.path()));
        QCOMPARE(m.rowCount(), n);

        m.select({u"f10"_s, u"f11"_s, u"f1500"_s, u"f1999"_s}, u"f1501"_s);
        for (int i = 0; i < n; i += 2)
            QVERIFY(QFile::remove(dir.filePath(u"f"_s + QString::number(i))));
        QVERIFY(m.relist());
        QCOMPARE(m.names(), survivors);
        QCOMPARE(m.selectedNames(), (QSet<QString>{u"f11"_s, u"f1999"_s}));
        QCOMPARE(m.currentName(), u"f1501"_s);
    }

    void selectionSurvivesListing_data() { listing_data(); }

    void selectionSurvivesListing()
    {
        QFETCH(bool, shuffled);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList expected = populate(dir.path(), 5000, shuffled);
        Model m;
        int merges = 0;
        QSet<QString> selected;
        QString current;
        // Like a QML handler reacting to the first rows, while later batches are still being merged.
        connect(&m, &FolderModel::countChanged, &m, [&] {
            if (!m.loading() || m.rowCount() == 0)
                return;
            if (merges++ > 0)
                return;
            const int last = m.rowCount() - 1;
            m.setSelection({0, last / 2, last}, false);
            m.setCurrentIndex(last / 2);
            selected = m.selectedNames();
            current = m.currentName();
        });
        QVERIFY(m.list(dir.path()));
        QCOMPARE(m.names(), expected);
        QVERIFY2(merges >= 2, "the listing arrived in a single merge, so nothing was merged after selecting");
        QCOMPARE(selected.size(), 3);
        QCOMPARE(m.selectedNames(), selected);
        QCOMPARE(m.currentName(), current);
    }
};

QTEST_GUILESS_MAIN(TestFolderModel)
#include "tst_foldermodel.moc"
