// SPDX-License-Identifier: MIT
#include "Inspector.h"
#include "../ai/SecretGuard.h"
#include "../fs/FileKind.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QtEndian>

#include <cstring>
#include <map>

#ifdef FUIN_HAVE_PARQUET
#include <arrow/api.h>
#include <arrow/io/file.h>
#include <arrow/util/compression.h>
#include <parquet/arrow/reader.h>
#include <parquet/file_reader.h>
#include <parquet/metadata.h>
#include <parquet/statistics.h>
#endif

using namespace Qt::StringLiterals;

namespace {

QVariantMap error(const QString &msg)
{
    return {{u"kind"_s, u"error"_s}, {u"msg"_s, msg}};
}

QVariantList pair(const QString &k, const QVariant &v)
{
    return {k, v};
}

// ------------------------------------------------------------------ column statistics (sampled)

struct ColStats {
    QString name;
    int nulls = 0;
    int ints = 0, floats = 0, bools = 0, dates = 0, strings = 0;
    bool hasNum = false;
    double minNum = 0, maxNum = 0;
    QString minStr, maxStr;

    void add(const QString &raw)
    {
        const QString v = raw.trimmed();
        if (v.isEmpty() || v == "null"_L1 || v == "NULL"_L1 || v == "NA"_L1 || v == "NaN"_L1 || v == "None"_L1) {
            ++nulls;
            return;
        }
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (ok) {
            (v.contains(u'.') || v.contains(u'e') || v.contains(u'E')) ? ++floats : ++ints;
            if (!hasNum || d < minNum)
                minNum = d;
            if (!hasNum || d > maxNum)
                maxNum = d;
            hasNum = true;
            return;
        }
        static const QRegularExpression dateRe(u"^\\d{4}-\\d{2}-\\d{2}([T ][\\d:.]+(Z|[+-]\\d{2}:?\\d{2})?)?$"_s);
        const QString low = v.toLower();
        if (low == "true"_L1 || low == "false"_L1)
            ++bools;
        else if (dateRe.match(v).hasMatch())
            ++dates;
        else
            ++strings;
        if (minStr.isEmpty() || v < minStr)
            minStr = v;
        if (maxStr.isEmpty() || v > maxStr)
            maxStr = v;
    }

    QVariantMap toMap() const
    {
        QString type = u"string"_s;
        const int nonNull = ints + floats + bools + dates + strings;
        if (nonNull == 0)
            type = u"null"_s;
        else if (strings == 0 && bools == 0 && dates == 0)
            type = floats > 0 ? u"float"_s : u"int"_s;
        else if (strings == 0 && ints == 0 && floats == 0 && dates == 0)
            type = u"bool"_s;
        else if (strings == 0 && ints == 0 && floats == 0 && bools == 0)
            type = u"date"_s;
        QVariantMap m{{u"name"_s, name}, {u"type"_s, type}, {u"nulls"_s, nulls}};
        if (type == "int"_L1 || type == "float"_L1) {
            m.insert(u"min"_s, QString::number(minNum, 'g', 12));
            m.insert(u"max"_s, QString::number(maxNum, 'g', 12));
        } else if (type == "date"_L1 || type == "string"_L1) {
            m.insert(u"min"_s, minStr.left(40));
            m.insert(u"max"_s, maxStr.left(40));
        }
        return m;
    }
};

qint64 countLines(const QString &path, qint64 cap)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > cap)
        return -1;
    qint64 n = 0;
    char buf[1 << 16];
    qint64 r;
    char last = '\n';
    while ((r = f.read(buf, sizeof buf)) > 0) {
        for (qint64 i = 0; i < r; ++i)
            if (buf[i] == '\n')
                ++n;
        last = buf[r - 1];
    }
    if (last != '\n')
        ++n;
    return n;
}

QList<QStringList> parseDelimited(const QString &text, QChar delim, int maxRows, bool *truncated)
{
    QList<QStringList> rows;
    QStringList row;
    QString field;
    bool inQuotes = false;
    const int n = int(text.size());
    for (int i = 0; i < n; ++i) {
        const QChar c = text.at(i);
        if (inQuotes) {
            if (c == u'"') {
                if (i + 1 < n && text.at(i + 1) == u'"') {
                    field += u'"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
            continue;
        }
        if (c == u'"' && field.isEmpty()) {
            inQuotes = true;
        } else if (c == delim) {
            row << field;
            field.clear();
        } else if (c == u'\n' || c == u'\r') {
            if (c == u'\r' && i + 1 < n && text.at(i + 1) == u'\n')
                ++i;
            row << field;
            field.clear();
            rows << row;
            row.clear();
            if (rows.size() > maxRows) {
                *truncated = true;
                return rows;
            }
        } else {
            field += c;
        }
    }
    if (!field.isEmpty() || !row.isEmpty()) {
        row << field;
        rows << row;
    }
    return rows;
}

QChar sniffDelimiter(const QString &firstLine)
{
    const QChar cands[] = {u',', u';', u'\t', u'|'};
    QChar best = u',';
    int bestN = 0;
    for (QChar c : cands) {
        const int k = int(firstLine.count(c));
        if (k > bestN) {
            bestN = k;
            best = c;
        }
    }
    return best;
}

QString jsonCell(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        return {};
    case QJsonValue::Bool:
        return v.toBool() ? u"true"_s : u"false"_s;
    case QJsonValue::Double:
        return QString::number(v.toDouble(), 'g', 15);
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    }
    return {};
}

QVariantMap tableFromObjects(const QList<QJsonObject> &objs, int maxCols)
{
    QStringList cols;
    for (const QJsonObject &o : objs)
        for (auto it = o.begin(); it != o.end(); ++it)
            if (!cols.contains(it.key()) && cols.size() < maxCols)
                cols << it.key();
    QList<ColStats> stats(cols.size());
    for (int c = 0; c < cols.size(); ++c)
        stats[c].name = cols.at(c);
    QVariantList rows;
    for (const QJsonObject &o : objs) {
        QVariantList r;
        for (int c = 0; c < cols.size(); ++c) {
            const QJsonValue v = o.value(cols.at(c));
            const QString s = jsonCell(v);
            stats[c].add(v.isNull() || v.isUndefined() ? QString() : s);
            r << (v.isNull() || v.isUndefined() ? QVariant() : QVariant(s.left(300)));
        }
        rows << QVariant(r);
    }
    QVariantList columns;
    for (const ColStats &s : stats)
        columns << s.toMap();
    return {{u"kind"_s, u"table"_s}, {u"columns"_s, columns}, {u"rows"_s, rows}};
}

// ------------------------------------------------------------------ small binary reader for GGUF

struct Reader {
    QFile &f;
    bool ok = true;
    template<typename T>
    T read()
    {
        T v{};
        if (f.read(reinterpret_cast<char *>(&v), sizeof v) != qint64(sizeof v))
            ok = false;
        return qFromLittleEndian(v);
    }
    QString str()
    {
        const quint64 n = read<quint64>();
        if (!ok || n > (1u << 24)) {
            ok = false;
            return {};
        }
        return QString::fromUtf8(f.read(qint64(n)));
    }
    void skip(qint64 n)
    {
        if (!f.seek(f.pos() + n))
            ok = false;
    }
};

QVariant ggufValue(Reader &r, quint32 type, bool keep)
{
    switch (type) {
    case 0: return r.read<quint8>();
    case 1: return r.read<qint8>();
    case 2: return r.read<quint16>();
    case 3: return r.read<qint16>();
    case 4: return r.read<quint32>();
    case 5: return r.read<qint32>();
    case 6: { const quint32 bits = r.read<quint32>(); float fl; std::memcpy(&fl, &bits, 4); return double(fl); }
    case 7: return r.read<quint8>() != 0;
    case 8: return r.str();
    case 9: {
        const quint32 et = r.read<quint32>();
        const quint64 n = r.read<quint64>();
        static const int sizes[] = {1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8};
        if (et < 13 && sizes[et] > 0) {
            r.skip(qint64(n) * sizes[et]);
        } else {
            for (quint64 i = 0; i < n && r.ok; ++i)
                ggufValue(r, et, false);
        }
        return keep ? QVariant(u"[%1 elementos]"_s.arg(n)) : QVariant();
    }
    case 10: return r.read<quint64>();
    case 11: return r.read<qint64>();
    case 12: return r.read<double>();
    }
    r.ok = false;
    return {};
}

QString ggmlType(quint32 t)
{
    static const std::map<quint32, const char *> m{{0, "F32"},     {1, "F16"},    {2, "Q4_0"},   {3, "Q4_1"},    {6, "Q5_0"},   {7, "Q5_1"},
                                                   {8, "Q8_0"},    {9, "Q8_1"},   {10, "Q2_K"},  {11, "Q3_K"},   {12, "Q4_K"},  {13, "Q5_K"},
                                                   {14, "Q6_K"},   {15, "Q8_K"},  {16, "IQ2_XXS"}, {17, "IQ2_XS"}, {18, "IQ3_XXS"}, {19, "IQ1_S"},
                                                   {20, "IQ4_NL"}, {21, "IQ3_S"}, {22, "IQ2_S"}, {23, "IQ4_XS"}, {24, "I8"},    {25, "I16"},
                                                   {26, "I32"},    {27, "I64"},   {28, "F64"},   {29, "IQ1_M"},  {30, "BF16"},  {34, "TQ1_0"},
                                                   {35, "TQ2_0"},  {39, "MXFP4"}};
    const auto it = m.find(t);
    return it != m.end() ? QString::fromLatin1(it->second) : u"tipo %1"_s.arg(t);
}

QString llamaFileType(quint32 t)
{
    static const std::map<quint32, const char *> m{{0, "F32"},     {1, "F16"},     {2, "Q4_0"},    {3, "Q4_1"},    {7, "Q8_0"},   {8, "Q5_0"},
                                                   {9, "Q5_1"},    {10, "Q2_K"},   {11, "Q3_K_S"}, {12, "Q3_K_M"}, {13, "Q3_K_L"}, {14, "Q4_K_S"},
                                                   {15, "Q4_K_M"}, {16, "Q5_K_S"}, {17, "Q5_K_M"}, {18, "Q6_K"},   {19, "IQ2_XXS"}, {20, "IQ2_XS"},
                                                   {21, "Q2_K_S"}, {22, "IQ3_XS"}, {23, "IQ3_XXS"}, {24, "IQ1_S"}, {25, "IQ4_NL"}, {26, "IQ3_S"},
                                                   {27, "IQ3_M"},  {28, "IQ2_S"},  {29, "IQ2_M"},  {30, "IQ4_XS"}, {31, "IQ1_M"}, {32, "BF16"},
                                                   {36, "TQ1_0"},  {37, "TQ2_0"},  {38, "MXFP4_MOE"}};
    const auto it = m.find(t);
    return it != m.end() ? QString::fromLatin1(it->second) : QString::number(t);
}

// protobuf varint
quint64 varint(const QByteArray &b, int &i, bool &ok)
{
    quint64 v = 0;
    int shift = 0;
    while (i < b.size() && shift < 64) {
        const auto c = quint8(b.at(i++));
        v |= quint64(c & 0x7f) << shift;
        if (!(c & 0x80))
            return v;
        shift += 7;
    }
    ok = false;
    return v;
}

} // namespace

// ====================================================================== public

QString Inspector::humanCount(double n)
{
    if (n >= 1e12)
        return QString::number(n / 1e12, 'f', 2) + u" T"_s;
    if (n >= 1e9)
        return QString::number(n / 1e9, 'f', 2) + u" B"_s;
    if (n >= 1e6)
        return QString::number(n / 1e6, 'f', 1) + u" M"_s;
    if (n >= 1e3)
        return QString::number(n / 1e3, 'f', 1) + u" K"_s;
    return QString::number(qint64(n));
}

bool Inspector::hasParquet()
{
#ifdef FUIN_HAVE_PARQUET
    return true;
#else
    return false;
#endif
}

QVariantMap Inspector::inspect(const QString &path, const Options &opts)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return error(u"No existe"_s);
    if (fi.isDir())
        return inspectFolder(path);
    if (!fi.isReadable())
        return error(u"Sin permiso de lectura"_s);

    const QString name = fi.fileName();
    const QString ext = FileKind::extension(name);
    const QString mime = QMimeDatabase().mimeTypeForFile(fi).name();
    QVariantMap out;

    if (ext == "csv"_L1 || ext == "tsv"_L1 || ext == "psv"_L1) {
        out = inspectCsv(path, ext == "tsv"_L1 ? u'\t' : ext == "psv"_L1 ? u'|' : QChar(), opts);
    } else if (ext == "parquet"_L1) {
        out = inspectParquet(path, opts);
    } else if (ext == "jsonl"_L1 || ext == "ndjson"_L1) {
        out = inspectJsonLines(path, opts);
    } else if (ext == "json"_L1 || ext == "geojson"_L1) {
        out = inspectJson(path, opts);
    } else if (ext == "safetensors"_L1) {
        out = inspectSafetensors(path);
    } else if (ext == "gguf"_L1) {
        out = inspectGguf(path);
    } else if (ext == "onnx"_L1) {
        out = inspectOnnx(path);
    } else if (ext == "pt"_L1 || ext == "pth"_L1 || ext == "ckpt"_L1 || ext == "pkl"_L1) {
        out = {{u"kind"_s, u"model"_s}, {u"format"_s, u"pickle"_s},
               {u"fields"_s, QVariantList{pair(u"Formato"_s, u"PyTorch / pickle"_s), pair(u"Nota"_s, u"No se deserializa: un pickle puede ejecutar código. Prefiere .safetensors."_s)}}};
    } else if (mime.startsWith("image/"_L1)) {
        QImageReader r(path);
        const QSize s = r.size();
        out = {{u"kind"_s, u"image"_s}, {u"width"_s, s.width()}, {u"height"_s, s.height()}, {u"format"_s, QString::fromLatin1(r.format()).toUpper()}};
    } else if (mime.startsWith("video/"_L1) || mime.startsWith("audio/"_L1) || mime == "application/pdf"_L1) {
        out = {{u"kind"_s, mime.startsWith("video/"_L1) ? u"video"_s : mime.startsWith("audio/"_L1) ? u"audio"_s : u"pdf"_s}};
    } else {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return error(f.errorString());
        const QByteArray data = f.read(opts.maxTextBytes);
        if (SecretGuard::looksBinary(data)) {
            out = {{u"kind"_s, u"binary"_s}};
        } else {
            const bool md = ext == "md"_L1 || ext == "markdown"_L1 || mime == "text/markdown"_L1;
            out = {{u"kind"_s, md ? u"markdown"_s : u"text"_s}, {u"text"_s, QString::fromUtf8(data)}, {u"truncated"_s, fi.size() > data.size()}};
        }
        if (out.value(u"kind"_s) != "binary"_L1) {
            const QString reason = SecretGuard::scanContent(data);
            if (!reason.isEmpty())
                out.insert(u"secret"_s, reason);
        }
    }
    if (SecretGuard::isSecretName(name) && !out.contains(u"secret"_s))
        out.insert(u"secret"_s, u"nombre de archivo sensible"_s);
    out.insert(u"mime"_s, mime);
    out.insert(u"size"_s, double(fi.size()));
    out.insert(u"path"_s, fi.absoluteFilePath());
    return out;
}

QVariantMap Inspector::inspectCsv(const QString &path, QChar delimiter, const Options &opts)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return error(f.errorString());
    const QString text = QString::fromUtf8(f.read(2 * 1024 * 1024));
    if (delimiter.isNull())
        delimiter = sniffDelimiter(text.section(u'\n', 0, 0));
    bool truncated = false;
    QList<QStringList> rows = parseDelimited(text, delimiter, opts.maxRows, &truncated);
    if (rows.isEmpty())
        return {{u"kind"_s, u"table"_s}, {u"columns"_s, QVariantList()}, {u"rows"_s, QVariantList()}, {u"totalRows"_s, 0}};
    const QStringList header = rows.takeFirst();
    if (rows.size() > opts.maxRows)
        rows = rows.mid(0, opts.maxRows);
    const int ncol = qMin(int(header.size()), opts.maxColumns);
    QList<ColStats> stats(ncol);
    for (int c = 0; c < ncol; ++c)
        stats[c].name = header.at(c).trimmed();
    QVariantList outRows;
    for (const QStringList &r : std::as_const(rows)) {
        QVariantList vr;
        for (int c = 0; c < ncol; ++c) {
            const QString v = c < r.size() ? r.at(c) : QString();
            stats[c].add(v);
            vr << (v.isEmpty() ? QVariant() : QVariant(v.left(300)));
        }
        outRows << QVariant(vr);
    }
    QVariantList columns;
    for (const ColStats &s : stats)
        columns << s.toMap();
    const qint64 lines = countLines(path, 1024LL * 1024 * 1024);
    const QString delimName = delimiter == u'\t' ? u"tabulador"_s : QString(delimiter);
    return {{u"kind"_s, u"table"_s},
            {u"format"_s, u"csv"_s},
            {u"engine"_s, u"lector CSV · separador «%1»"_s.arg(delimName)},
            {u"columns"_s, columns},
            {u"rows"_s, outRows},
            {u"totalRows"_s, lines >= 0 ? QVariant(double(qMax<qint64>(0, lines - 1))) : QVariant()},
            {u"statsSample"_s, int(rows.size())},
            {u"delimiter"_s, QString(delimiter)}};
}

QVariantMap Inspector::inspectJson(const QString &path, const Options &opts)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return error(f.errorString());
    if (f.size() > 32 * 1024 * 1024) {
        const QByteArray head = f.read(opts.maxTextBytes);
        return {{u"kind"_s, u"text"_s}, {u"text"_s, QString::fromUtf8(head)}, {u"truncated"_s, true}};
    }
    const QByteArray all = f.readAll();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(all, &err);
    if (err.error != QJsonParseError::NoError)
        return {{u"kind"_s, u"text"_s}, {u"text"_s, QString::fromUtf8(all.left(opts.maxTextBytes))}, {u"truncated"_s, all.size() > opts.maxTextBytes},
                {u"warning"_s, u"JSON no válido: "_s + err.errorString()}};
    if (doc.isArray()) {
        const QJsonArray arr = doc.array();
        QList<QJsonObject> objs;
        bool allObjects = !arr.isEmpty();
        for (const QJsonValue &v : arr) {
            if (!v.isObject()) {
                allObjects = false;
                break;
            }
            if (objs.size() < opts.maxRows)
                objs << v.toObject();
        }
        if (allObjects) {
            QVariantMap t = tableFromObjects(objs, opts.maxColumns);
            t.insert(u"format"_s, u"json"_s);
            t.insert(u"engine"_s, u"JSON (array de objetos)"_s);
            t.insert(u"totalRows"_s, double(arr.size()));
            t.insert(u"statsSample"_s, int(objs.size()));
            return t;
        }
    }
    const QByteArray pretty = doc.toJson(QJsonDocument::Indented);
    return {{u"kind"_s, u"text"_s}, {u"text"_s, QString::fromUtf8(pretty.left(opts.maxTextBytes))}, {u"truncated"_s, pretty.size() > opts.maxTextBytes},
            {u"language"_s, u"JSON"_s}};
}

QVariantMap Inspector::inspectJsonLines(const QString &path, const Options &opts)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return error(f.errorString());
    QList<QJsonObject> objs;
    int bad = 0;
    while (!f.atEnd() && objs.size() < opts.maxRows) {
        const QByteArray line = f.readLine(4 * 1024 * 1024).trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument d = QJsonDocument::fromJson(line);
        if (d.isObject())
            objs << d.object();
        else
            ++bad;
    }
    QVariantMap t = tableFromObjects(objs, opts.maxColumns);
    t.insert(u"format"_s, u"jsonl"_s);
    t.insert(u"engine"_s, u"JSON Lines"_s);
    const qint64 lines = countLines(path, 1024LL * 1024 * 1024);
    if (lines >= 0)
        t.insert(u"totalRows"_s, double(lines));
    t.insert(u"statsSample"_s, int(objs.size()));
    if (bad)
        t.insert(u"warning"_s, u"%1 líneas no son objetos JSON"_s.arg(bad));
    return t;
}

QVariantMap Inspector::inspectParquet(const QString &path, const Options &opts)
{
#ifndef FUIN_HAVE_PARQUET
    Q_UNUSED(path)
    Q_UNUSED(opts)
    return error(u"Compilado sin soporte Parquet (Apache Arrow)."_s);
#else
    try {
        auto infile = arrow::io::ReadableFile::Open(path.toStdString());
        if (!infile.ok())
            return error(QString::fromStdString(infile.status().ToString()));
        std::unique_ptr<parquet::ParquetFileReader> pf = parquet::ParquetFileReader::Open(*infile);
        std::shared_ptr<parquet::FileMetaData> md = pf->metadata();
        const int ncols = md->num_columns();
        const int nrg = md->num_row_groups();

        parquet::ArrowReaderProperties props;
        props.set_batch_size(opts.maxRows);
        auto made = parquet::arrow::FileReader::Make(arrow::default_memory_pool(), std::move(pf), props);
        if (!made.ok())
            return error(QString::fromStdString(made.status().ToString()));
        std::unique_ptr<parquet::arrow::FileReader> reader = std::move(*made);
        std::shared_ptr<arrow::Schema> schema;
        if (auto st = reader->GetSchema(&schema); !st.ok())
            return error(QString::fromStdString(st.ToString()));

        // Column statistics from the footer (exact, no data scan). Arrow fields map 1:1 to leaf
        // columns only for flat schemas; nested columns just show their type.
        const bool flat = schema->num_fields() == ncols;
        QVariantList columns;
        for (int c = 0; c < schema->num_fields() && c < opts.maxColumns; ++c) {
            const auto &field = schema->field(c);
            QVariantMap col{{u"name"_s, QString::fromStdString(field->name())},
                            {u"type"_s, QString::fromStdString(field->type()->ToString())},
                            {u"nullable"_s, field->nullable()}};
            if (flat) {
                qint64 nulls = 0;
                bool haveNulls = false, numeric = arrow::is_numeric(field->type()->id());
                QString minS, maxS;
                double minD = 0, maxD = 0;
                bool haveMinMax = false;
                for (int rg = 0; rg < nrg; ++rg) {
                    auto chunk = md->RowGroup(rg)->ColumnChunk(c);
                    if (!chunk->is_stats_set())
                        continue;
                    auto stats = chunk->statistics();
                    if (!stats)
                        continue;
                    if (stats->HasNullCount()) {
                        nulls += stats->null_count();
                        haveNulls = true;
                    }
                    if (!stats->HasMinMax())
                        continue;
                    std::shared_ptr<arrow::Scalar> mn, mx;
                    if (!parquet::arrow::StatisticsAsScalars(*stats, &mn, &mx).ok() || !mn || !mx)
                        continue;
                    const QString a = QString::fromStdString(mn->ToString()), b = QString::fromStdString(mx->ToString());
                    if (numeric) {
                        const double da = a.toDouble(), db = b.toDouble();
                        if (!haveMinMax || da < minD) {
                            minD = da;
                            minS = a;
                        }
                        if (!haveMinMax || db > maxD) {
                            maxD = db;
                            maxS = b;
                        }
                    } else {
                        if (!haveMinMax || a < minS)
                            minS = a;
                        if (!haveMinMax || b > maxS)
                            maxS = b;
                    }
                    haveMinMax = true;
                }
                if (haveNulls)
                    col.insert(u"nulls"_s, double(nulls));
                if (haveMinMax) {
                    col.insert(u"min"_s, minS.left(40));
                    col.insert(u"max"_s, maxS.left(40));
                }
            }
            columns << col;
        }

        QVariantList rows;
        auto rbr = reader->GetRecordBatchReader();
        if (rbr.ok()) {
            std::shared_ptr<arrow::RecordBatch> batch;
            if ((*rbr)->ReadNext(&batch).ok() && batch) {
                const int nc = qMin(batch->num_columns(), opts.maxColumns);
                for (int64_t r = 0; r < batch->num_rows() && r < opts.maxRows; ++r) {
                    QVariantList vr;
                    for (int c = 0; c < nc; ++c) {
                        auto sc = batch->column(c)->GetScalar(r);
                        if (!sc.ok() || !(*sc)->is_valid)
                            vr << QVariant();
                        else
                            vr << QString::fromStdString((*sc)->ToString()).left(300);
                    }
                    rows << QVariant(vr);
                }
            }
        }

        QString compression;
        if (nrg > 0 && ncols > 0)
            compression = QString::fromStdString(arrow::util::Codec::GetCodecAsString(md->RowGroup(0)->ColumnChunk(0)->compression()));
        return {{u"kind"_s, u"table"_s},
                {u"format"_s, u"parquet"_s},
                {u"engine"_s, u"Apache Arrow %1"_s.arg(QString::fromLatin1(ARROW_VERSION_STRING))},
                {u"columns"_s, columns},
                {u"rows"_s, rows},
                {u"totalRows"_s, double(md->num_rows())},
                {u"exactStats"_s, true},
                {u"fields"_s, QVariantList{pair(u"Grupos de filas"_s, nrg), pair(u"Compresión"_s, compression),
                                           pair(u"Creado por"_s, QString::fromStdString(md->created_by())),
                                           pair(u"Versión"_s, QString::fromStdString(md->version() == parquet::ParquetVersion::PARQUET_1_0 ? "1.0" : "2.x"))}}};
    } catch (const std::exception &e) {
        return error(QString::fromUtf8(e.what()));
    }
#endif
}

QVariantMap Inspector::inspectSafetensors(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return error(f.errorString());
    quint64 n = 0;
    if (f.read(reinterpret_cast<char *>(&n), 8) != 8)
        return error(u"Cabecera incompleta"_s);
    n = qFromLittleEndian(n);
    if (n == 0 || n > 100 * 1024 * 1024)
        return error(u"Cabecera safetensors no válida"_s);
    const QJsonObject hdr = QJsonDocument::fromJson(f.read(qint64(n))).object();
    double params = 0;
    QMap<QString, double> dtypes;
    QVariantList tensors;
    QStringList names = hdr.keys();
    names.removeAll(u"__metadata__"_s);
    for (const QString &k : std::as_const(names)) {
        const QJsonObject t = hdr.value(k).toObject();
        double count = 1;
        QStringList shape;
        for (const QJsonValue &d : t.value(u"shape"_s).toArray()) {
            count *= d.toDouble();
            shape << QString::number(qint64(d.toDouble()));
        }
        params += count;
        dtypes[t.value(u"dtype"_s).toString()] += count;
        if (tensors.size() < 60)
            tensors << QVariantMap{{u"name"_s, k}, {u"dtype"_s, t.value(u"dtype"_s).toString()}, {u"shape"_s, u"["_s + shape.join(u", "_s) + u"]"_s}};
    }
    QStringList dt;
    for (auto it = dtypes.cbegin(); it != dtypes.cend(); ++it)
        dt << u"%1 %2%"_s.arg(it.key()).arg(qRound(100 * it.value() / qMax(1.0, params)));
    QVariantList fields{pair(u"Formato"_s, u"safetensors"_s), pair(u"Tensores"_s, int(names.size())), pair(u"Parámetros"_s, humanCount(params)),
                        pair(u"Tipos"_s, dt.join(u" · "_s))};
    const QJsonObject meta = hdr.value(u"__metadata__"_s).toObject();
    for (auto it = meta.begin(); it != meta.end() && fields.size() < 14; ++it)
        fields << QVariant(pair(it.key(), it.value().toString().left(120)));
    return {{u"kind"_s, u"model"_s}, {u"format"_s, u"safetensors"_s}, {u"fields"_s, fields}, {u"tensors"_s, tensors}, {u"params"_s, params}};
}

QVariantMap Inspector::inspectGguf(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return error(f.errorString());
    Reader r{f};
    if (f.read(4) != "GGUF")
        return error(u"No es un archivo GGUF"_s);
    const quint32 version = r.read<quint32>();
    if (version < 2)
        return error(u"GGUF v1 no soportado"_s);
    const quint64 nTensors = r.read<quint64>();
    const quint64 nKv = r.read<quint64>();
    QMap<QString, QVariant> kv;
    for (quint64 i = 0; i < nKv && r.ok; ++i) {
        const QString key = r.str();
        const quint32 type = r.read<quint32>();
        kv.insert(key, ggufValue(r, type, true));
    }
    double params = 0;
    QMap<QString, double> types;
    QVariantList tensors;
    for (quint64 i = 0; i < nTensors && r.ok; ++i) {
        const QString name = r.str();
        const quint32 nd = r.read<quint32>();
        double count = 1;
        QStringList shape;
        for (quint32 d = 0; d < nd && d < 8 && r.ok; ++d) {
            const quint64 dim = r.read<quint64>();
            count *= double(dim);
            shape << QString::number(dim);
        }
        const QString t = ggmlType(r.read<quint32>());
        r.read<quint64>(); // offset
        params += count;
        types[t] += count;
        if (tensors.size() < 60)
            tensors << QVariantMap{{u"name"_s, name}, {u"dtype"_s, t}, {u"shape"_s, u"["_s + shape.join(u", "_s) + u"]"_s}};
    }
    const QString arch = kv.value(u"general.architecture"_s).toString();
    QVariantList fields{pair(u"Formato"_s, u"GGUF v%1"_s.arg(version))};
    auto add = [&](const QString &label, const QString &key) {
        if (kv.contains(key))
            fields << QVariant(pair(label, kv.value(key)));
    };
    add(u"Nombre"_s, u"general.name"_s);
    add(u"Arquitectura"_s, u"general.architecture"_s);
    add(u"Tamaño"_s, u"general.size_label"_s);
    if (kv.contains(u"general.file_type"_s))
        fields << QVariant(pair(u"Cuantización"_s, llamaFileType(kv.value(u"general.file_type"_s).toUInt())));
    add(u"Contexto"_s, arch + u".context_length"_s);
    add(u"Embedding"_s, arch + u".embedding_length"_s);
    add(u"Capas"_s, arch + u".block_count"_s);
    add(u"Cabezas atención"_s, arch + u".attention.head_count"_s);
    add(u"Expertos"_s, arch + u".expert_count"_s);
    add(u"Tokenizador"_s, u"tokenizer.ggml.model"_s);
    add(u"Licencia"_s, u"general.license"_s);
    fields << QVariant(pair(u"Tensores"_s, double(nTensors)));
    if (r.ok)
        fields << QVariant(pair(u"Parámetros"_s, humanCount(params)));
    QStringList ts;
    for (auto it = types.cbegin(); it != types.cend(); ++it)
        ts << u"%1 %2%"_s.arg(it.key()).arg(qRound(100 * it.value() / qMax(1.0, params)));
    if (!ts.isEmpty())
        fields << QVariant(pair(u"Tipos de tensor"_s, ts.join(u" · "_s)));
    return {{u"kind"_s, u"model"_s}, {u"format"_s, u"gguf"_s}, {u"fields"_s, fields}, {u"tensors"_s, tensors}, {u"params"_s, params}};
}

QVariantMap Inspector::inspectOnnx(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return error(f.errorString());
    QVariantList fields{pair(u"Formato"_s, u"ONNX"_s)};
    QStringList opsets;
    // Walk top-level ModelProto fields, seeking over the (large) graph.
    for (int guard = 0; guard < 64 && !f.atEnd(); ++guard) {
        QByteArray head = f.peek(20);
        int i = 0;
        bool ok = true;
        const quint64 key = varint(head, i, ok);
        if (!ok)
            break;
        const int field = int(key >> 3), wire = int(key & 7);
        if (wire == 0) {
            const quint64 v = varint(head, i, ok);
            f.seek(f.pos() + i);
            if (field == 1)
                fields << QVariant(pair(u"IR version"_s, double(v)));
            else if (field == 5)
                fields << QVariant(pair(u"Versión del modelo"_s, double(v)));
        } else if (wire == 2) {
            const quint64 len = varint(head, i, ok);
            if (!ok)
                break;
            f.seek(f.pos() + i);
            if (field == 7 || len > 4 * 1024 * 1024) { // graph or big blob: skip
                if (!f.seek(f.pos() + qint64(len)))
                    break;
                continue;
            }
            const QByteArray val = f.read(qint64(len));
            if (field == 2)
                fields << QVariant(pair(u"Productor"_s, QString::fromUtf8(val)));
            else if (field == 3)
                fields << QVariant(pair(u"Versión productor"_s, QString::fromUtf8(val)));
            else if (field == 4)
                fields << QVariant(pair(u"Dominio"_s, QString::fromUtf8(val)));
            else if (field == 6 && !val.isEmpty())
                fields << QVariant(pair(u"Descripción"_s, QString::fromUtf8(val).left(200)));
            else if (field == 8) { // OperatorSetIdProto { domain = 1; version = 2 }
                QString dom = u"ai.onnx"_s;
                quint64 ver = 0;
                int j = 0;
                bool ok2 = true;
                while (j < val.size() && ok2) {
                    const quint64 k = varint(val, j, ok2);
                    if ((k & 7) == 2) {
                        const quint64 l = varint(val, j, ok2);
                        if (!ok2 || j + int(l) > val.size())
                            break;
                        if ((k >> 3) == 1 && l > 0)
                            dom = QString::fromUtf8(val.mid(j, int(l)));
                        j += int(l);
                    } else if ((k & 7) == 0) {
                        const quint64 v = varint(val, j, ok2);
                        if ((k >> 3) == 2)
                            ver = v;
                    } else {
                        break;
                    }
                }
                opsets << u"%1 %2"_s.arg(dom).arg(ver);
            }
        } else if (wire == 1) {
            f.seek(f.pos() + i + 8);
        } else if (wire == 5) {
            f.seek(f.pos() + i + 4);
        } else {
            break;
        }
    }
    if (!opsets.isEmpty())
        fields << QVariant(pair(u"Opsets"_s, opsets.join(u", "_s)));
    return {{u"kind"_s, u"model"_s}, {u"format"_s, u"onnx"_s}, {u"fields"_s, fields}};
}

QVariantMap Inspector::inspectFolder(const QString &path)
{
    QDir d(path);
    const QFileInfoList entries = d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    int dirs = 0, files = 0;
    qint64 bytes = 0;
    QString readme;
    for (const QFileInfo &e : entries) {
        if (e.isDir()) {
            ++dirs;
        } else {
            ++files;
            bytes += e.size();
            if (readme.isEmpty() && e.fileName().compare(u"readme.md"_s, Qt::CaseInsensitive) == 0)
                readme = e.absoluteFilePath();
        }
    }
    // Project detection (handy for AI/data repos)
    struct Marker {
        const char *file;
        const char *label;
    };
    static const Marker markers[] = {{"pyproject.toml", "Python"},       {"requirements.txt", "Python"}, {"package.json", "Node.js"},
                                     {"Cargo.toml", "Rust"},             {"go.mod", "Go"},               {"CMakeLists.txt", "CMake/C++"},
                                     {"dbt_project.yml", "dbt"},         {"docker-compose.yml", "Docker Compose"}, {"compose.yaml", "Docker Compose"},
                                     {"Dockerfile", "Docker"},           {"main.tf", "Terraform"},       {"airflow.cfg", "Airflow"},
                                     {"dvc.yaml", "DVC"},                {"MLproject", "MLflow"},        {"uv.lock", "uv"},
                                     {"CLAUDE.md", "Claude Code"},       {"AGENTS.md", "Agentes IA"},    {".cursor", "Cursor"}};
    QStringList kinds;
    for (const Marker &m : markers)
        if (QFileInfo::exists(path + u'/' + QString::fromLatin1(m.file)) && !kinds.contains(QString::fromUtf8(m.label)))
            kinds << QString::fromUtf8(m.label);
    QVariantMap out{{u"kind"_s, u"folder"_s}, {u"dirs"_s, dirs}, {u"files"_s, files}, {u"bytes"_s, double(bytes)}, {u"project"_s, kinds}};
    if (!readme.isEmpty()) {
        QFile f(readme);
        if (f.open(QIODevice::ReadOnly))
            out.insert(u"readme"_s, QString::fromUtf8(f.read(64 * 1024)));
    }
    return out;
}
