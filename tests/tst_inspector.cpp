// SPDX-License-Identifier: MIT
#include "preview/Inspector.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
QVariantMap column(const QVariantMap &table, int i)
{
    return table.value(u"columns"_s).toList().value(i).toMap();
}

QVariantList row(const QVariantMap &table, int i)
{
    return table.value(u"rows"_s).toList().value(i).toList();
}
}

class TestInspector : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;

    QString write(const QString &name, const QByteArray &data)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size())
            qFatal("cannot write %s", qPrintable(path));
        return path;
    }

private Q_SLOTS:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void csvSniffsDelimiter_data();
    void csvSniffsDelimiter();
    void csvColumnStats();
    void csvQuotes();
    void csvCrlf();
    void csvMaxRows();
    void csvEmpty();
    void jsonArrayOfObjects();
    void jsonOtherDocument();
    void jsonInvalid();
    void jsonLines();
    void dispatch();
    void dispatchSecrets();
    void folder();
};

void TestInspector::csvSniffsDelimiter_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<QString>("delimiter");
    QTest::newRow("comma") << QByteArrayLiteral("a,b,c\n1,2,3\n") << u","_s;
    QTest::newRow("semicolon") << QByteArrayLiteral("a;b;c\n1,5;2;3\n") << u";"_s;
    QTest::newRow("tab") << QByteArrayLiteral("a\tb\tc\n1\t2\t3\n") << u"\t"_s;
    QTest::newRow("pipe") << QByteArrayLiteral("a|b|c\n1|2|3\n") << u"|"_s;
    QTest::newRow("single column") << QByteArrayLiteral("a\n1\n") << u","_s;
}

void TestInspector::csvSniffsDelimiter()
{
    QFETCH(QByteArray, data);
    QFETCH(QString, delimiter);
    const QVariantMap t = Inspector::inspectCsv(write(u"sniff.csv"_s, data), QChar(), {});
    QCOMPARE(t.value(u"kind"_s).toString(), u"table"_s);
    QCOMPARE(t.value(u"delimiter"_s).toString(), delimiter);
    QCOMPARE(column(t, 0).value(u"name"_s).toString(), u"a"_s);
    QCOMPARE(t.value(u"totalRows"_s).toDouble(), 1.0);
}

void TestInspector::csvColumnStats()
{
    const QByteArray data = "name,age,score,active,joined,note\n"
                            "Ana,31,9.5,true,2024-01-02,\n"
                            "Bea,,7,false,2023-05-06T10:00:00Z,NA\n"
                            "Cai,25,1e2,TRUE,2022-12-31,x\n";
    const QVariantMap t = Inspector::inspectCsv(write(u"stats.csv"_s, data), QChar(), {});
    QCOMPARE(t.value(u"format"_s).toString(), u"csv"_s);
    QCOMPARE(t.value(u"totalRows"_s).toDouble(), 3.0);
    QCOMPARE(t.value(u"statsSample"_s).toInt(), 3);

    const QVariantMap name = column(t, 0), age = column(t, 1), score = column(t, 2), active = column(t, 3), joined = column(t, 4),
                      note = column(t, 5);
    QCOMPARE(name.value(u"type"_s).toString(), u"string"_s);
    QCOMPARE(name.value(u"min"_s).toString(), u"Ana"_s);
    QCOMPARE(name.value(u"max"_s).toString(), u"Cai"_s);
    QCOMPARE(age.value(u"type"_s).toString(), u"int"_s);
    QCOMPARE(age.value(u"nulls"_s).toInt(), 1);
    QCOMPARE(age.value(u"min"_s).toString(), u"25"_s);
    QCOMPARE(age.value(u"max"_s).toString(), u"31"_s);
    QCOMPARE(score.value(u"type"_s).toString(), u"float"_s);
    QCOMPARE(score.value(u"min"_s).toString(), u"7"_s);
    QCOMPARE(score.value(u"max"_s).toString(), u"100"_s);
    QCOMPARE(active.value(u"type"_s).toString(), u"bool"_s);
    QCOMPARE(joined.value(u"type"_s).toString(), u"date"_s);
    QCOMPARE(note.value(u"type"_s).toString(), u"string"_s);
    QCOMPARE(note.value(u"nulls"_s).toInt(), 2);

    QCOMPARE(row(t, 0).value(0).toString(), u"Ana"_s);
    QVERIFY(row(t, 1).value(1).isNull());
}

void TestInspector::csvQuotes()
{
    const QByteArray data = "a,b\n\"x, y\",\"he said \"\"hi\"\"\"\n\"multi\nline\",2\n";
    const QVariantMap t = Inspector::inspectCsv(write(u"quotes.csv"_s, data), QChar(), {});
    QCOMPARE(t.value(u"rows"_s).toList().size(), 2);
    QCOMPARE(row(t, 0).value(0).toString(), u"x, y"_s);
    QCOMPARE(row(t, 0).value(1).toString(), u"he said \"hi\""_s);
    QCOMPARE(row(t, 1).value(0).toString(), u"multi\nline"_s);
}

void TestInspector::csvCrlf()
{
    const QVariantMap t = Inspector::inspectCsv(write(u"crlf.csv"_s, "a,b\r\n1,2\r\n3,4\r\n"), QChar(), {});
    QCOMPARE(t.value(u"rows"_s).toList().size(), 2);
    QCOMPARE(row(t, 1).value(1).toString(), u"4"_s);
    QCOMPARE(column(t, 1).value(u"type"_s).toString(), u"int"_s);
}

void TestInspector::csvMaxRows()
{
    QByteArray data = "n\n";
    for (int i = 0; i < 10; ++i)
        data += QByteArray::number(i) + '\n';
    Inspector::Options opts;
    opts.maxRows = 3;
    const QVariantMap t = Inspector::inspectCsv(write(u"many.csv"_s, data), QChar(), opts);
    QCOMPARE(t.value(u"rows"_s).toList().size(), 3);
    QCOMPARE(t.value(u"statsSample"_s).toInt(), 3);
    QCOMPARE(t.value(u"totalRows"_s).toDouble(), 10.0);
    QCOMPARE(column(t, 0).value(u"max"_s).toString(), u"2"_s);
}

void TestInspector::csvEmpty()
{
    const QVariantMap t = Inspector::inspectCsv(write(u"empty.csv"_s, {}), QChar(), {});
    QCOMPARE(t.value(u"kind"_s).toString(), u"table"_s);
    QVERIFY(t.value(u"columns"_s).toList().isEmpty());
    QCOMPARE(t.value(u"totalRows"_s).toInt(), 0);
}

void TestInspector::jsonArrayOfObjects()
{
    const QVariantMap t = Inspector::inspectJson(write(u"array.json"_s, R"([{"a": 1, "b": "x"}, {"a": 2.5, "c": true}, {"a": null, "d": [1, 2]}])"), {});
    QCOMPARE(t.value(u"kind"_s).toString(), u"table"_s);
    QCOMPARE(t.value(u"format"_s).toString(), u"json"_s);
    QCOMPARE(t.value(u"totalRows"_s).toDouble(), 3.0);
    QCOMPARE(column(t, 0).value(u"name"_s).toString(), u"a"_s);
    QCOMPARE(column(t, 0).value(u"type"_s).toString(), u"float"_s);
    QCOMPARE(column(t, 0).value(u"nulls"_s).toInt(), 1);
    QCOMPARE(column(t, 1).value(u"nulls"_s).toInt(), 2);
    QCOMPARE(column(t, 2).value(u"type"_s).toString(), u"bool"_s);
    QCOMPARE(row(t, 2).value(3).toString(), u"[1,2]"_s);
}

void TestInspector::jsonOtherDocument()
{
    const QVariantMap obj = Inspector::inspectJson(write(u"object.json"_s, R"({"k": [1, 2, 3]})"), {});
    QCOMPARE(obj.value(u"kind"_s).toString(), u"text"_s);
    QCOMPARE(obj.value(u"language"_s).toString(), u"JSON"_s);
    QVERIFY(obj.value(u"text"_s).toString().contains(u"\"k\""_s));

    const QVariantMap mixed = Inspector::inspectJson(write(u"mixed.json"_s, R"([{"a": 1}, 2])"), {});
    QCOMPARE(mixed.value(u"kind"_s).toString(), u"text"_s);
}

void TestInspector::jsonInvalid()
{
    const QVariantMap t = Inspector::inspectJson(write(u"broken.json"_s, R"({"a": )"), {});
    QCOMPARE(t.value(u"kind"_s).toString(), u"text"_s);
    QVERIFY(t.value(u"warning"_s).toString().startsWith(u"Invalid JSON"_s));
    QCOMPARE(t.value(u"text"_s).toString(), uR"({"a": )"_s);
}

void TestInspector::jsonLines()
{
    const QVariantMap t = Inspector::inspectJsonLines(write(u"rows.jsonl"_s, "{\"a\": 1}\n\n[1, 2]\n{\"a\": 2, \"b\": \"y\"}\n"), {});
    QCOMPARE(t.value(u"kind"_s).toString(), u"table"_s);
    QCOMPARE(t.value(u"format"_s).toString(), u"jsonl"_s);
    QCOMPARE(t.value(u"rows"_s).toList().size(), 2);
    QCOMPARE(t.value(u"statsSample"_s).toInt(), 2);
    QCOMPARE(t.value(u"warning"_s).toString(), u"1 lines are not JSON objects"_s);
    QCOMPARE(column(t, 0).value(u"max"_s).toString(), u"2"_s);
    QCOMPARE(column(t, 1).value(u"nulls"_s).toInt(), 1);
}

void TestInspector::dispatch()
{
    QCOMPARE(Inspector::inspect(write(u"d.tsv"_s, "a,b\tc\n1,2\t3\n")).value(u"delimiter"_s).toString(), u"\t"_s);
    QCOMPARE(Inspector::inspect(write(u"d.psv"_s, "a,b|c\n1,2|3\n")).value(u"delimiter"_s).toString(), u"|"_s);
    QCOMPARE(Inspector::inspect(write(u"d.ndjson"_s, "{\"a\": 1}\n")).value(u"format"_s).toString(), u"jsonl"_s);
    QCOMPARE(Inspector::inspect(write(u"d.json"_s, "[{\"a\": 1}]")).value(u"format"_s).toString(), u"json"_s);
    QCOMPARE(Inspector::inspect(write(u"d.md"_s, "# Title\n")).value(u"kind"_s).toString(), u"markdown"_s);
    QCOMPARE(Inspector::inspect(write(u"d.pt"_s, "not a pickle")).value(u"format"_s).toString(), u"pickle"_s);
    QCOMPARE(Inspector::inspect(write(u"d.bin"_s, QByteArray("\x01\x00\x02", 3))).value(u"kind"_s).toString(), u"binary"_s);

    const QVariantMap text = Inspector::inspect(write(u"d.txt"_s, "hello\n"));
    QCOMPARE(text.value(u"kind"_s).toString(), u"text"_s);
    QCOMPARE(text.value(u"text"_s).toString(), u"hello\n"_s);
    QCOMPARE(text.value(u"size"_s).toDouble(), 6.0);
    QCOMPARE(text.value(u"path"_s).toString(), m_dir.filePath(u"d.txt"_s));

    Inspector::Options opts;
    opts.maxTextBytes = 4;
    const QVariantMap cut = Inspector::inspect(m_dir.filePath(u"d.txt"_s), opts);
    QCOMPARE(cut.value(u"text"_s).toString(), u"hell"_s);
    QVERIFY(cut.value(u"truncated"_s).toBool());

    QCOMPARE(Inspector::inspect(m_dir.filePath(u"missing.txt"_s)).value(u"kind"_s).toString(), u"error"_s);
}

void TestInspector::dispatchSecrets()
{
    const QVariantMap env = Inspector::inspect(write(u".env"_s, "DEBUG=1\n"));
    QCOMPARE(env.value(u"secret"_s).toString(), u"sensitive file name"_s);

    const QVariantMap token = Inspector::inspect(write(u"notes.txt"_s, "token: " + QByteArray("hf_") + QByteArray(30, 'z') + '\n'));
    QCOMPARE(token.value(u"secret"_s).toString(), u"Hugging Face token"_s);

    const QVariantMap example = Inspector::inspect(write(u".env.example"_s, "DEBUG=1\n"));
    QVERIFY(!example.contains(u"secret"_s));

    const QVariantMap binary = Inspector::inspect(write(u"blob.bin"_s, QByteArray(1, '\0') + "hf_" + QByteArray(30, 'z')));
    QCOMPARE(binary.value(u"kind"_s).toString(), u"binary"_s);
    QVERIFY(!binary.contains(u"secret"_s));
}

void TestInspector::folder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(u"src"_s));
    for (const auto &[name, data] : {std::pair{"CMakeLists.txt", "project(x)\n"}, {"pyproject.toml", "[project]\n"}, {"README.md", "# Hi\n"}}) {
        QFile f(dir.filePath(QString::fromLatin1(name)));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }
    const QVariantMap t = Inspector::inspect(dir.path());
    QCOMPARE(t.value(u"kind"_s).toString(), u"folder"_s);
    QCOMPARE(t.value(u"dirs"_s).toInt(), 1);
    QCOMPARE(t.value(u"files"_s).toInt(), 3);
    QCOMPARE(t.value(u"project"_s).toStringList(), (QStringList{u"Python"_s, u"CMake/C++"_s}));
    QCOMPARE(t.value(u"readme"_s).toString(), u"# Hi\n"_s);
}

QTEST_GUILESS_MAIN(TestInspector)
#include "tst_inspector.moc"
