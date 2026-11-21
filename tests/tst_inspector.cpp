// SPDX-License-Identifier: MIT
#include "preview/Inspector.h"

#include <cstring>

#ifdef FUIN_HAVE_PARQUET
#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/writer.h>
#endif

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QtEndian>

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

// Value of a model header field by its label; invalid when absent.
QVariant field(const QVariantMap &model, const QString &label)
{
    for (const QVariant &f : model.value(u"fields"_s).toList())
        if (f.toList().value(0).toString() == label)
            return f.toList().value(1);
    return {};
}

template<typename T>
void put(QByteArray &b, T v)
{
    v = qToLittleEndian(v);
    b.append(reinterpret_cast<const char *>(&v), sizeof v);
}

struct Gguf {
    QByteArray kv, tensors;
    quint64 nKv = 0, nTensors = 0;

    static void str(QByteArray &b, const QByteArray &s)
    {
        put<quint64>(b, quint64(s.size()));
        b += s;
    }
    Gguf &string(const QByteArray &key, const QByteArray &value)
    {
        str(kv, key);
        put<quint32>(kv, 8);
        str(kv, value);
        ++nKv;
        return *this;
    }
    Gguf &u32(const QByteArray &key, quint32 value)
    {
        str(kv, key);
        put<quint32>(kv, 4);
        put<quint32>(kv, value);
        ++nKv;
        return *this;
    }
    Gguf &f32(const QByteArray &key, float value)
    {
        quint32 bits;
        std::memcpy(&bits, &value, 4);
        str(kv, key);
        put<quint32>(kv, 6);
        put<quint32>(kv, bits);
        ++nKv;
        return *this;
    }
    Gguf &strings(const QByteArray &key, const QList<QByteArray> &values)
    {
        str(kv, key);
        put<quint32>(kv, 9);
        put<quint32>(kv, 8);
        put<quint64>(kv, quint64(values.size()));
        for (const QByteArray &v : values)
            str(kv, v);
        ++nKv;
        return *this;
    }
    Gguf &i32s(const QByteArray &key, int n)
    {
        str(kv, key);
        put<quint32>(kv, 9);
        put<quint32>(kv, 5);
        put<quint64>(kv, quint64(n));
        for (int i = 0; i < n; ++i)
            put<qint32>(kv, i);
        ++nKv;
        return *this;
    }
    Gguf &tensor(const QByteArray &name, const QList<quint64> &dims, quint32 type)
    {
        str(tensors, name);
        put<quint32>(tensors, quint32(dims.size()));
        for (quint64 d : dims)
            put<quint64>(tensors, d);
        put<quint32>(tensors, type);
        put<quint64>(tensors, 0);
        ++nTensors;
        return *this;
    }
    QByteArray bytes(quint32 version) const
    {
        QByteArray b = "GGUF";
        put<quint32>(b, version);
        put<quint64>(b, nTensors);
        put<quint64>(b, nKv);
        return b + kv + tensors;
    }
};

// Protocol Buffers wire format, enough for an ONNX ModelProto header.
QByteArray varint(quint64 v)
{
    QByteArray b;
    do {
        char c = char(v & 0x7f);
        v >>= 7;
        if (v)
            c |= char(0x80);
        b += c;
    } while (v);
    return b;
}
QByteArray pbInt(int field, quint64 v)
{
    return varint(quint64(field) << 3) + varint(v);
}
QByteArray pbBytes(int field, const QByteArray &v)
{
    return varint((quint64(field) << 3) | 2) + varint(quint64(v.size())) + v;
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
    void safetensors();
    void safetensorsInvalid();
    void gguf_data();
    void gguf();
    void ggufInvalid();
    void onnx();
    void parquet();
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

void TestInspector::safetensors()
{
    const QJsonObject header{
        {u"__metadata__"_s, QJsonObject{{u"format"_s, u"pt"_s}}},
        {u"w"_s, QJsonObject{{u"dtype"_s, u"F16"_s}, {u"shape"_s, QJsonArray{2, 3}}, {u"data_offsets"_s, QJsonArray{0, 12}}}},
        {u"b"_s, QJsonObject{{u"dtype"_s, u"F32"_s}, {u"shape"_s, QJsonArray{3}}, {u"data_offsets"_s, QJsonArray{12, 24}}}},
    };
    const QByteArray json = QJsonDocument(header).toJson(QJsonDocument::Compact);
    QByteArray file;
    put<quint64>(file, quint64(json.size()));
    file += json + QByteArray(24, '\0');

    const QVariantMap m = Inspector::inspect(write(u"tiny.safetensors"_s, file));
    QCOMPARE(m.value(u"kind"_s).toString(), u"model"_s);
    QCOMPARE(m.value(u"format"_s).toString(), u"safetensors"_s);
    QCOMPARE(m.value(u"params"_s).toDouble(), 9.0);
    QCOMPARE(field(m, u"Tensors"_s).toInt(), 2);
    QCOMPARE(field(m, u"Parameters"_s).toString(), u"9"_s);
    QCOMPARE(field(m, u"Types"_s).toString(), u"F16 67% · F32 33%"_s);
    QCOMPARE(field(m, u"format"_s).toString(), u"pt"_s);
    const QVariantList tensors = m.value(u"tensors"_s).toList();
    QCOMPARE(tensors.size(), 2);
    QCOMPARE(tensors.at(1).toMap().value(u"shape"_s).toString(), u"[2, 3]"_s);
}

void TestInspector::safetensorsInvalid()
{
    QCOMPARE(Inspector::inspectSafetensors(write(u"short.safetensors"_s, "abc")).value(u"msg"_s).toString(), u"Incomplete header"_s);
    QByteArray zero;
    put<quint64>(zero, 0);
    QCOMPARE(Inspector::inspectSafetensors(write(u"zero.safetensors"_s, zero)).value(u"msg"_s).toString(), u"Invalid safetensors header"_s);
}

void TestInspector::gguf_data()
{
    QTest::addColumn<quint32>("version");
    QTest::newRow("v2") << 2u;
    QTest::newRow("v3") << 3u;
}

void TestInspector::gguf()
{
    QFETCH(quint32, version);
    Gguf g;
    g.string("general.architecture", "llama")
        .string("general.name", "Tiny")
        .u32("general.file_type", 15)
        .u32("llama.context_length", 2048)
        .u32("llama.block_count", 2)
        .f32("llama.rope.freq_base", 10000.0f)
        .strings("tokenizer.ggml.tokens", {"<s>", "</s>"})
        .i32s("tokenizer.ggml.token_type", 3)
        .string("tokenizer.ggml.model", "gpt2")
        .tensor("token_embd.weight", {4, 2}, 0)
        .tensor("blk.0.attn_q.weight", {8}, 12);

    const QVariantMap m = Inspector::inspect(write(u"tiny.gguf"_s, g.bytes(version)));
    QCOMPARE(m.value(u"kind"_s).toString(), u"model"_s);
    QCOMPARE(m.value(u"format"_s).toString(), u"gguf"_s);
    QCOMPARE(field(m, u"Format"_s).toString(), u"GGUF v%1"_s.arg(version));
    QCOMPARE(field(m, u"Name"_s).toString(), u"Tiny"_s);
    QCOMPARE(field(m, u"Architecture"_s).toString(), u"llama"_s);
    QCOMPARE(field(m, u"Quantization"_s).toString(), u"Q4_K_M"_s);
    QCOMPARE(field(m, u"Context"_s).toUInt(), 2048u);
    QCOMPARE(field(m, u"Layers"_s).toUInt(), 2u);
    QCOMPARE(field(m, u"Tokenizer"_s).toString(), u"gpt2"_s);
    QCOMPARE(field(m, u"Tensors"_s).toDouble(), 2.0);
    QCOMPARE(field(m, u"Parameters"_s).toString(), u"16"_s);
    QCOMPARE(field(m, u"Tensor types"_s).toString(), u"F32 50% · Q4_K 50%"_s);
    QCOMPARE(m.value(u"params"_s).toDouble(), 16.0);
    const QVariantList tensors = m.value(u"tensors"_s).toList();
    QCOMPARE(tensors.size(), 2);
    QCOMPARE(tensors.at(0).toMap().value(u"shape"_s).toString(), u"[4, 2]"_s);
    QCOMPARE(tensors.at(1).toMap().value(u"dtype"_s).toString(), u"Q4_K"_s);
}

void TestInspector::ggufInvalid()
{
    QCOMPARE(Inspector::inspectGguf(write(u"magic.gguf"_s, "GGML\x03\0\0\0")).value(u"msg"_s).toString(), u"Not a GGUF file"_s);
    QCOMPARE(Inspector::inspectGguf(write(u"v1.gguf"_s, Gguf().bytes(1))).value(u"msg"_s).toString(), u"GGUF v1 is not supported"_s);

    // A file cut inside the tensor table still shows the metadata but no parameter count.
    Gguf g;
    g.string("general.name", "Cut").tensor("t", {4}, 0);
    const QVariantMap cut = Inspector::inspectGguf(write(u"cut.gguf"_s, g.bytes(3).chopped(6)));
    QCOMPARE(cut.value(u"kind"_s).toString(), u"model"_s);
    QCOMPARE(field(cut, u"Name"_s).toString(), u"Cut"_s);
    QVERIFY(!field(cut, u"Parameters"_s).isValid());
}

void TestInspector::onnx()
{
    const QByteArray model = pbInt(1, 8) + pbBytes(2, "pytorch") + pbBytes(3, "2.4.0") + pbBytes(4, "ai.test") + pbInt(5, 3)
                             + pbBytes(6, "tiny test model") + pbBytes(7, QByteArray(5000, '\x01')) + pbBytes(8, pbInt(2, 17))
                             + pbBytes(8, pbBytes(1, "com.microsoft") + pbInt(2, 1));
    const QVariantMap m = Inspector::inspect(write(u"tiny.onnx"_s, model));
    QCOMPARE(m.value(u"kind"_s).toString(), u"model"_s);
    QCOMPARE(m.value(u"format"_s).toString(), u"onnx"_s);
    QCOMPARE(field(m, u"IR version"_s).toDouble(), 8.0);
    QCOMPARE(field(m, u"Producer"_s).toString(), u"pytorch"_s);
    QCOMPARE(field(m, u"Producer version"_s).toString(), u"2.4.0"_s);
    QCOMPARE(field(m, u"Domain"_s).toString(), u"ai.test"_s);
    QCOMPARE(field(m, u"Model version"_s).toDouble(), 3.0);
    QCOMPARE(field(m, u"Description"_s).toString(), u"tiny test model"_s);
    QCOMPARE(field(m, u"Opsets"_s).toString(), u"ai.onnx 17, com.microsoft 1"_s);
}

void TestInspector::parquet()
{
    const QString path = m_dir.filePath(u"tiny.parquet"_s);
#ifndef FUIN_HAVE_PARQUET
    QVERIFY(!Inspector::hasParquet());
    write(u"tiny.parquet"_s, "PAR1");
    const QVariantMap off = Inspector::inspect(path);
    QCOMPARE(off.value(u"kind"_s).toString(), u"error"_s);
    QCOMPARE(off.value(u"msg"_s).toString(), u"Built without Parquet support (Apache Arrow)."_s);
#else
    QVERIFY(Inspector::hasParquet());
    arrow::Int64Builder ids;
    arrow::StringBuilder names;
    QVERIFY(ids.Append(3).ok() && ids.AppendNull().ok() && ids.Append(7).ok());
    QVERIFY(names.Append("b").ok() && names.Append("a").ok() && names.Append("c").ok());
    std::shared_ptr<arrow::Array> idArray, nameArray;
    QVERIFY(ids.Finish(&idArray).ok() && names.Finish(&nameArray).ok());
    const auto table = arrow::Table::Make(arrow::schema({arrow::field("id", arrow::int64()), arrow::field("name", arrow::utf8())}),
                                          {idArray, nameArray});
    auto out = arrow::io::FileOutputStream::Open(path.toStdString());
    QVERIFY(out.ok());
    QVERIFY(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), *out, 2).ok());
    QVERIFY((*out)->Close().ok());

    const QVariantMap t = Inspector::inspect(path);
    QCOMPARE(t.value(u"kind"_s).toString(), u"table"_s);
    QCOMPARE(t.value(u"format"_s).toString(), u"parquet"_s);
    QCOMPARE(t.value(u"totalRows"_s).toDouble(), 3.0);
    QVERIFY(t.value(u"exactStats"_s).toBool());
    QCOMPARE(field(t, u"Row groups"_s).toInt(), 2);

    const QVariantMap id = column(t, 0), name = column(t, 1);
    QCOMPARE(id.value(u"name"_s).toString(), u"id"_s);
    QCOMPARE(id.value(u"type"_s).toString(), u"int64"_s);
    QCOMPARE(id.value(u"nulls"_s).toDouble(), 1.0);
    QCOMPARE(id.value(u"min"_s).toString(), u"3"_s);
    QCOMPARE(id.value(u"max"_s).toString(), u"7"_s);
    QCOMPARE(name.value(u"type"_s).toString(), u"string"_s);
    QCOMPARE(name.value(u"min"_s).toString(), u"a"_s);
    QCOMPARE(name.value(u"max"_s).toString(), u"c"_s);

    QCOMPARE(t.value(u"rows"_s).toList().size(), 3);
    QCOMPARE(row(t, 0).value(1).toString(), u"b"_s);
    QVERIFY(row(t, 1).value(0).isNull());
    QCOMPARE(row(t, 2).value(0).toString(), u"7"_s);
#endif
}

QTEST_GUILESS_MAIN(TestInspector)
#include "tst_inspector.moc"
