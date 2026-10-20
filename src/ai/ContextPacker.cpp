// SPDX-License-Identifier: MIT
#include "ContextPacker.h"
#include "SecretGuard.h"
#include "../preview/Inspector.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QUrl>
#include <QtConcurrent>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace {
const QSet<QString> kSkipDirs{u".git"_s,  u"node_modules"_s, u"__pycache__"_s, u".venv"_s,  u"venv"_s,         u"target"_s, u"build"_s,
                              u"dist"_s,  u".mypy_cache"_s,  u".pytest_cache"_s, u".ruff_cache"_s, u".idea"_s,  u".next"_s,
                              u".cache"_s, u".tox"_s,        u".gradle"_s,    u"coverage"_s, u".terraform"_s, u".dvc"_s};
const QSet<QString> kLockFiles{u"package-lock.json"_s, u"yarn.lock"_s, u"pnpm-lock.yaml"_s, u"poetry.lock"_s, u"uv.lock"_s, u"cargo.lock"_s,
                               u"composer.lock"_s,     u"gemfile.lock"_s, u"bun.lockb"_s, u"pdm.lock"_s};
const QSet<QString> kBinaryExt{u"png"_s,  u"jpg"_s,  u"jpeg"_s, u"gif"_s,  u"webp"_s, u"ico"_s,  u"bmp"_s,     u"tiff"_s,   u"avif"_s,
                               u"mp4"_s,  u"mkv"_s,  u"webm"_s, u"mov"_s,  u"mp3"_s,  u"flac"_s, u"wav"_s,     u"ogg"_s,    u"pdf"_s,
                               u"zip"_s,  u"gz"_s,   u"tgz"_s,  u"xz"_s,   u"zst"_s,  u"7z"_s,   u"rar"_s,     u"tar"_s,    u"parquet"_s,
                               u"arrow"_s, u"feather"_s, u"avro"_s, u"orc"_s, u"safetensors"_s, u"gguf"_s, u"onnx"_s, u"pt"_s, u"pth"_s,
                               u"ckpt"_s, u"bin"_s,  u"pkl"_s,  u"npy"_s,  u"npz"_s,  u"h5"_s,   u"sqlite"_s,  u"db"_s,     u"duckdb"_s,
                               u"so"_s,   u"o"_s,    u"a"_s,    u"exe"_s,  u"dll"_s,  u"class"_s, u"jar"_s,    u"woff"_s,   u"woff2"_s,
                               u"ttf"_s,  u"otf"_s,  u"iso"_s,  u"xlsx"_s, u"docx"_s, u"pptx"_s};

// Data files and model weights are not pasted verbatim: an LLM gets far more from their schema.
const QSet<QString> kSchemaExt{u"parquet"_s, u"safetensors"_s, u"gguf"_s, u"onnx"_s};
const QSet<QString> kTableExt{u"csv"_s, u"tsv"_s, u"jsonl"_s, u"ndjson"_s, u"json"_s};

QString cell(const QVariant &v)
{
    QString s = v.isNull() ? u"∅"_s : v.toString();
    s.replace(u'|', u"\\|"_s).replace(u'\n', u' ');
    return s.size() > 60 ? s.left(57) + u"..."_s : s;
}

QString schemaSummary(const QString &path, int sampleRows)
{
    Inspector::Options o;
    o.maxRows = sampleRows;
    const QVariantMap r = Inspector::inspect(path, o);
    const QString kind = r.value(u"kind"_s).toString();
    QString out;
    if (kind == "table"_L1) {
        const QVariantList cols = r.value(u"columns"_s).toList();
        out += u"Resumen generado por Fūin (no es el contenido literal).\n"_s;
        if (r.contains(u"totalRows"_s))
            out += u"Filas: %1 · "_s.arg(r.value(u"totalRows"_s).toLongLong());
        out += u"Columnas: %1 · Motor: %2\n\n| columna | tipo | nulos | min | max |\n|---|---|---|---|---|\n"_s.arg(cols.size()).arg(
            r.value(u"engine"_s).toString());
        for (const QVariant &cv : cols) {
            const QVariantMap c = cv.toMap();
            out += u"| %1 | %2 | %3 | %4 | %5 |\n"_s.arg(cell(c.value(u"name"_s)), cell(c.value(u"type"_s)), c.value(u"nulls"_s).toString(),
                                                     cell(c.value(u"min"_s)), cell(c.value(u"max"_s)));
        }
        const QVariantList rows = r.value(u"rows"_s).toList();
        if (!rows.isEmpty() && !cols.isEmpty()) {
            out += u"\nPrimeras %1 filas:\n\n|"_s.arg(rows.size());
            for (const QVariant &cv : cols)
                out += u" %1 |"_s.arg(cell(cv.toMap().value(u"name"_s)));
            out += u"\n|"_s + u"---|"_s.repeated(cols.size()) + u"\n"_s;
            for (const QVariant &rv : rows) {
                out += u"|"_s;
                for (const QVariant &v : rv.toList())
                    out += u" %1 |"_s.arg(cell(v));
                out += u"\n"_s;
            }
        }
    } else if (kind == "model"_L1) {
        out += u"Cabecera del modelo (pesos omitidos):\n\n"_s;
        for (const QVariant &fv : r.value(u"fields"_s).toList()) {
            const QVariantList f = fv.toList();
            out += u"- %1: %2\n"_s.arg(f.value(0).toString(), f.value(1).toString());
        }
        const QVariantList tensors = r.value(u"tensors"_s).toList();
        if (!tensors.isEmpty())
            out += u"\nTensores (%1 primeros):\n"_s.arg(qMin<qsizetype>(tensors.size(), 40));
        for (const QVariant &tv : tensors.mid(0, 40)) {
            const QVariantMap t = tv.toMap();
            out += u"- %1 %2 %3\n"_s.arg(t.value(u"name"_s).toString(), t.value(u"dtype"_s).toString(), t.value(u"shape"_s).toString());
        }
    }
    return out;
}


QString fenceLang(const QString &name)
{
    static const QMap<QString, QString> m{{u"py"_s, u"python"_s}, {u"js"_s, u"javascript"_s}, {u"ts"_s, u"typescript"_s}, {u"tsx"_s, u"tsx"_s},
                                          {u"rs"_s, u"rust"_s},   {u"go"_s, u"go"_s},         {u"cpp"_s, u"cpp"_s},        {u"h"_s, u"cpp"_s},
                                          {u"hpp"_s, u"cpp"_s},   {u"c"_s, u"c"_s},           {u"sh"_s, u"bash"_s},        {u"qml"_s, u"qml"_s},
                                          {u"sql"_s, u"sql"_s},   {u"yml"_s, u"yaml"_s},      {u"yaml"_s, u"yaml"_s},      {u"toml"_s, u"toml"_s},
                                          {u"json"_s, u"json"_s}, {u"md"_s, u"markdown"_s},   {u"html"_s, u"html"_s},      {u"css"_s, u"css"_s},
                                          {u"java"_s, u"java"_s}, {u"kt"_s, u"kotlin"_s},     {u"rb"_s, u"ruby"_s},        {u"lua"_s, u"lua"_s},
                                          {u"tf"_s, u"hcl"_s},    {u"cmake"_s, u"cmake"_s},   {u"csv"_s, u"csv"_s},        {u"xml"_s, u"xml"_s}};
    const int dot = name.lastIndexOf(u'.');
    const QString ext = dot > 0 ? name.mid(dot + 1).toLower() : QString();
    if (name == "Dockerfile"_L1)
        return u"dockerfile"_s;
    if (name == "CMakeLists.txt"_L1)
        return u"cmake"_s;
    return m.value(ext, ext);
}

QStringList gitFiles(const QString &dir, bool *isRepo)
{
    QProcess p;
    p.start(u"git"_s, {u"-C"_s, dir, u"ls-files"_s, u"-co"_s, u"--exclude-standard"_s, u"-z"_s});
    if (!p.waitForFinished(15000) || p.exitCode() != 0) {
        *isRepo = false;
        return {};
    }
    *isRepo = true;
    QStringList out;
    for (const QByteArray &rel : p.readAllStandardOutput().split('\0'))
        if (!rel.isEmpty())
            out << dir + u'/' + QString::fromUtf8(rel);
    return out;
}

QStringList walk(const QString &dir, int cap)
{
    QStringList out;
    QStringList stack{dir};
    while (!stack.isEmpty() && out.size() < cap) {
        const QString d = stack.takeLast();
        const QFileInfoList entries = QDir(d).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
        for (const QFileInfo &e : entries) {
            if (e.isSymLink())
                continue;
            if (e.isDir()) {
                if (!kSkipDirs.contains(e.fileName()))
                    stack << e.absoluteFilePath();
            } else {
                out << e.absoluteFilePath();
            }
        }
    }
    return out;
}

QString commonRoot(const QStringList &paths)
{
    if (paths.isEmpty())
        return {};
    QString root = QFileInfo(paths.first()).isDir() && paths.size() == 1 ? paths.first() : QFileInfo(paths.first()).absolutePath();
    for (const QString &p : paths) {
        while (!root.isEmpty() && root != "/"_L1 && !(p == root || p.startsWith(root + u'/')))
            root = QFileInfo(root).absolutePath();
    }
    return root;
}

QString renderTree(const QStringList &rel)
{
    // rel paths are sorted; print as an indented tree
    QString out;
    QStringList prev;
    for (const QString &r : rel) {
        const QStringList parts = r.split(u'/');
        int common = 0;
        while (common < prev.size() - 1 && common < parts.size() - 1 && prev.at(common) == parts.at(common))
            ++common;
        for (int i = common; i < parts.size(); ++i) {
            out += QString(i * 2, u' ') + parts.at(i) + (i < parts.size() - 1 ? u"/"_s : QString()) + u'\n';
        }
        prev = parts;
    }
    return out;
}

QString xmlEscape(QString s)
{
    // CDATA keeps content readable; only the terminator needs splitting.
    return s.replace(u"]]>"_s, u"]]]]><![CDATA[>"_s);
}
}

int ContextPacker::estimateTokens(const QString &text)
{
    // ~3.6 chars/token for mixed code+prose with current BPE tokenizers.
    return int(double(text.size()) / 3.6 + 0.5);
}

QVariantMap ContextPacker::Result::summary() const
{
    return {{u"files"_s, int(included.size())}, {u"included"_s, included}, {u"skipped"_s, skipped}, {u"bytes"_s, double(bytes)},
            {u"tokens"_s, tokens}, {u"secretsBlocked"_s, secretsBlocked}, {u"summarized"_s, summarized}, {u"root"_s, root}};
}

ContextPacker::Result ContextPacker::build(const QStringList &inputPaths, const QString &format, const Limits &limits)
{
    Result res;
    QStringList inputs;
    for (const QString &p : inputPaths) {
        const QUrl u(p);
        inputs << QFileInfo(u.isLocalFile() ? u.toLocalFile() : p).absoluteFilePath();
    }
    res.root = commonRoot(inputs);

    // 1. gather candidates
    QStringList candidates;
    for (const QString &p : std::as_const(inputs)) {
        const QFileInfo fi(p);
        if (fi.isDir()) {
            bool repo = false;
            QStringList files = gitFiles(p, &repo);
            if (!repo)
                files = walk(p, 5000);
            candidates += files;
        } else if (fi.isFile()) {
            candidates << fi.absoluteFilePath();
        }
    }
    candidates.removeDuplicates();
    std::sort(candidates.begin(), candidates.end());

    auto rel = [&res](const QString &p) {
        return res.root.isEmpty() || res.root == "/"_L1 ? p : (p == res.root ? QFileInfo(p).fileName() : p.mid(res.root.size() + 1));
    };
    auto skip = [&res, &rel](const QString &p, const QString &why) {
        res.skipped << QVariantMap{{u"path"_s, rel(p)}, {u"reason"_s, why}};
    };

    // 2. read & filter
    struct Doc {
        QString rel, content;
    };
    QList<Doc> docs;
    for (const QString &p : std::as_const(candidates)) {
        const QFileInfo fi(p);
        const QString name = fi.fileName();
        const int dot = name.lastIndexOf(u'.');
        const QString ext = dot > 0 ? name.mid(dot + 1).toLower() : QString();
        if (!fi.isFile())
            continue;
        if (SecretGuard::isSecretName(name)) {
            ++res.secretsBlocked;
            skip(p, u"posible secreto (nombre)"_s);
            continue;
        }
        if (kLockFiles.contains(name.toLower())) {
            skip(p, u"lockfile"_s);
            continue;
        }
        if (kSchemaExt.contains(ext) || (kTableExt.contains(ext) && fi.size() > 32 * 1024)) {
            // Big tables and binary data/model files: include a schema summary instead.
            const QString summary = schemaSummary(p, kSchemaExt.contains(ext) ? 5 : 10);
            if (!summary.isEmpty() && docs.size() < limits.maxFiles) {
                res.bytes += summary.toUtf8().size();
                docs << Doc{rel(p) + u" (esquema)"_s, summary};
                res.included << rel(p);
                ++res.summarized;
                continue;
            }
        }
        if (kBinaryExt.contains(ext)) {
            skip(p, u"binario"_s);
            continue;
        }
        if (fi.size() > limits.maxFileBytes) {
            skip(p, u"demasiado grande (%1 KB)"_s.arg(fi.size() / 1024));
            continue;
        }
        if (docs.size() >= limits.maxFiles) {
            skip(p, u"límite de ficheros"_s);
            continue;
        }
        if (res.bytes + fi.size() > limits.maxTotalBytes) {
            skip(p, u"límite total"_s);
            continue;
        }
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) {
            skip(p, u"sin permiso"_s);
            continue;
        }
        const QByteArray data = f.readAll();
        if (SecretGuard::looksBinary(data)) {
            skip(p, u"binario"_s);
            continue;
        }
        const QString why = SecretGuard::scanContent(data);
        if (!why.isEmpty()) {
            ++res.secretsBlocked;
            skip(p, u"contiene %1"_s.arg(why));
            continue;
        }
        res.bytes += data.size();
        docs << Doc{rel(p), QString::fromUtf8(data)};
        res.included << rel(p);
    }

    // 3. render
    const QString title = res.root.isEmpty() ? u"ficheros"_s : QFileInfo(res.root).fileName();
    const QString tree = renderTree(res.included);
    QString out;
    if (format == "xml"_L1) {
        out += u"<context source=\"Fūin\" root=\"%1\">\n<tree><![CDATA[\n%2]]></tree>\n<documents>\n"_s.arg(title.toHtmlEscaped(), tree);
        int i = 1;
        for (const Doc &d : std::as_const(docs))
            out += u"<document index=\"%1\">\n<source>%2</source>\n<document_content><![CDATA[\n%3\n]]></document_content>\n</document>\n"_s.arg(i++).arg(
                d.rel.toHtmlEscaped(), xmlEscape(d.content));
        out += u"</documents>\n</context>\n"_s;
    } else {
        out += u"# Contexto: %1\n\n%2 ficheros · generado con Fūin\n\n## Estructura\n\n```\n%3```\n"_s.arg(title).arg(docs.size()).arg(tree);
        for (const Doc &d : std::as_const(docs)) {
            QString fence = u"```"_s;
            while (d.content.contains(fence))
                fence += u'`';
            out += u"\n## %1\n\n%2%3\n%4%5%2\n"_s.arg(d.rel, fence, fenceLang(QFileInfo(d.rel).fileName()), d.content,
                                                     d.content.endsWith(u'\n') ? QString() : u"\n"_s);
        }
    }
    res.text = out;
    res.tokens = estimateTokens(out);
    return res;
}

// ---------------------------------------------------------------- QML wrapper

ContextPack::ContextPack(QObject *parent)
    : QObject(parent)
{
}

void ContextPack::build(const QStringList &pathsOrUrls, const QString &format, bool copy)
{
    if (m_busy || pathsOrUrls.isEmpty())
        return;
    m_busy = true;
    Q_EMIT busyChanged();
    auto *w = new QFutureWatcher<ContextPacker::Result>(this);
    connect(w, &QFutureWatcher<ContextPacker::Result>::finished, this, [this, w, copy] {
        w->deleteLater();
        const ContextPacker::Result r = w->result();
        m_text = r.text;
        m_last = r.summary();
        m_busy = false;
        Q_EMIT busyChanged();
        Q_EMIT lastChanged();
        if (copy)
            copyLast();
    });
    w->setFuture(QtConcurrent::run([pathsOrUrls, format] { return ContextPacker::build(pathsOrUrls, format); }));
}

void ContextPack::copyLast()
{
    if (m_text.isEmpty())
        return;
    QApplication::clipboard()->setText(m_text);
    Q_EMIT copied(m_last);
}
