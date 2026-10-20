// SPDX-License-Identifier: MIT
#include "McpServer.h"
#include "../app/Config.h"
#include "../ipc/Ipc.h"
#include "../preview/Inspector.h"
#include "ContextPacker.h"
#include "SecretGuard.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

#include <iostream>
#include <string>

using namespace Qt::StringLiterals;

namespace {
const QStringList kModern{u"2026-07-28"_s};
const QStringList kLegacy{u"2025-11-25"_s, u"2025-06-18"_s, u"2025-03-26"_s, u"2024-11-05"_s};
const QString kMetaVersion = u"io.modelcontextprotocol/protocolVersion"_s;

QJsonObject serverInfo()
{
    return {{u"name"_s, u"archivos"_s}, {u"title"_s, u"Archivos"_s}, {u"version"_s, QCoreApplication::applicationVersion()}};
}

QJsonObject capabilities()
{
    return {{u"tools"_s, QJsonObject{{u"listChanged"_s, false}}}};
}

const char *kInstructions =
    "Archivos is the user's file manager. Use these tools to look at the folder and selection the user has open "
    "(archivos_get_current_view), inspect files (schemas of CSV/Parquet/JSONL, ML model metadata), build context packs, and "
    "point the user at files (archivos_reveal). Writes are disabled unless the user enabled them and are confirmed in the UI.";

QJsonObject schema(const QJsonObject &props, const QStringList &required = {})
{
    QJsonObject s{{u"type"_s, u"object"_s}, {u"properties"_s, props}, {u"additionalProperties"_s, false}};
    if (!required.isEmpty())
        s.insert(u"required"_s, QJsonArray::fromStringList(required));
    return s;
}
QJsonObject str(const QString &d) { return {{u"type"_s, u"string"_s}, {u"description"_s, d}}; }
QJsonObject integer(const QString &d) { return {{u"type"_s, u"integer"_s}, {u"description"_s, d}}; }
QJsonObject boolean(const QString &d) { return {{u"type"_s, u"boolean"_s}, {u"description"_s, d}}; }
QJsonObject strArray(const QString &d) { return {{u"type"_s, u"array"_s}, {u"items"_s, QJsonObject{{u"type"_s, u"string"_s}}}, {u"description"_s, d}}; }

QJsonObject tool(const QString &name, const QString &title, const QString &desc, const QJsonObject &input, bool readOnly, bool destructive = false)
{
    return {{u"name"_s, name},
            {u"title"_s, title},
            {u"description"_s, desc},
            {u"inputSchema"_s, input},
            {u"annotations"_s, QJsonObject{{u"readOnlyHint"_s, readOnly}, {u"destructiveHint"_s, destructive}, {u"openWorldHint"_s, false}}}};
}

QJsonObject textResult(const QString &text, bool isError = false, const QJsonValue &structured = {})
{
    QJsonObject r{{u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, text}}}}, {u"isError"_s, isError}};
    if (structured.isObject())
        r.insert(u"structuredContent"_s, structured);
    return r;
}

QJsonObject jsonResult(const QJsonObject &o)
{
    return textResult(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented)), false, o);
}

QJsonObject rpcError(const QJsonValue &id, int code, const QString &msg, const QJsonValue &data = {})
{
    QJsonObject e{{u"code"_s, code}, {u"message"_s, msg}};
    if (!data.isUndefined() && !data.isNull())
        e.insert(u"data"_s, data);
    return {{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, id}, {u"error"_s, e}};
}

// Shown to the user in confirmation dialogs ("Claude Code solicita: ...").
QString g_clientName = u"Un agente MCP"_s;

QJsonObject gui(const QJsonObject &req, int timeoutMs = 3000)
{
    QJsonObject r = req;
    r.insert(u"client"_s, g_clientName);
    return IpcClient::request(r, timeoutMs);
}

bool ensureGui()
{
    if (IpcClient::isRunning())
        return true;
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
    for (int i = 0; i < 40; ++i) {
        QThread::msleep(150);
        if (IpcClient::isRunning())
            return true;
    }
    return false;
}
}

QJsonArray McpServer::toolDefinitions()
{
    return {
        tool(u"archivos_get_current_view"_s, u"Vista actual"_s,
             u"Returns what the user is looking at in Archivos right now: the active folder, selected files, open tabs and the "
             "previewed file. Use it to resolve 'this folder' or 'these files'."_s,
             schema({}), true),
        tool(u"archivos_list_directory"_s, u"Listar carpeta"_s,
             u"Lists a directory (name, type, size, modified time, git branch for repos). Paths may use ~."_s,
             schema({{u"path"_s, str(u"Directory path"_s)}, {u"show_hidden"_s, boolean(u"Include dotfiles (default false)"_s)},
                     {u"limit"_s, integer(u"Max entries (default 500)"_s)}},
                    {u"path"_s}),
             true),
        tool(u"archivos_read_file"_s, u"Leer archivo"_s,
             u"Reads a UTF-8 text file. Refuses binaries and files that look like secrets (.env, keys, tokens)."_s,
             schema({{u"path"_s, str(u"File path"_s)}, {u"offset"_s, integer(u"Byte offset (default 0)"_s)},
                     {u"max_bytes"_s, integer(u"Max bytes to return (default 65536, max 1048576)"_s)}},
                    {u"path"_s}),
             true),
        tool(u"archivos_search"_s, u"Buscar"_s,
             u"Finds files by name (fd) or, with content=true, by text inside files (ripgrep). Respects .gitignore."_s,
             schema({{u"query"_s, str(u"Text to search for"_s)}, {u"path"_s, str(u"Folder to search in (default: current view or ~)"_s)},
                     {u"content"_s, boolean(u"Search file contents instead of names"_s)}, {u"limit"_s, integer(u"Max results (default 100)"_s)}},
                    {u"query"_s}),
             true),
        tool(u"archivos_inspect"_s, u"Inspeccionar"_s,
             u"Structured metadata for a file: schema, row count, null counts and min/max for CSV/TSV/JSON/JSONL/Parquet; "
             "tensors, parameter count, dtype and quantization for .safetensors/.gguf/.onnx; dimensions for images; project type "
             "and README for folders."_s,
             schema({{u"path"_s, str(u"File or folder path"_s)}, {u"sample_rows"_s, integer(u"Rows to include for tables (default 10)"_s)}},
                    {u"path"_s}),
             true),
        tool(u"archivos_context_pack"_s, u"Contexto para IA"_s,
             u"Bundles files/folders into one Markdown or XML document (tree + contents). Respects .gitignore and skips binaries, "
             "lockfiles and secrets. Returns the document and a token estimate."_s,
             schema({{u"paths"_s, strArray(u"Files or folders"_s)}, {u"format"_s, QJsonObject{{u"type"_s, u"string"_s}, {u"enum"_s, QJsonArray{u"markdown"_s, u"xml"_s}}}}},
                    {u"paths"_s}),
             true),
        tool(u"archivos_reveal"_s, u"Mostrar en Archivos"_s,
             u"Opens Archivos at a file or folder and selects it so the user can see it (starts Archivos if needed)."_s,
             schema({{u"path"_s, str(u"File or folder path"_s)}, {u"preview"_s, boolean(u"Also open the preview pane"_s)}}, {u"path"_s}), true),
        tool(u"archivos_create_folder"_s, u"Crear carpeta"_s,
             u"Creates a folder. Requires 'Permitir escritura a agentes' in Archivos; the user must approve it in the UI. Undoable."_s,
             schema({{u"path"_s, str(u"New folder path"_s)}}, {u"path"_s}), false),
        tool(u"archivos_move_to_trash"_s, u"Mover a la papelera"_s,
             u"Moves files to the trash (recoverable, undoable). Requires write permission and user approval in the UI."_s,
             schema({{u"paths"_s, strArray(u"Paths to trash"_s)}}, {u"paths"_s}), false, true),
        tool(u"archivos_copy"_s, u"Copiar"_s, u"Copies files into a folder. Requires write permission and user approval in the UI. Undoable."_s,
             schema({{u"sources"_s, strArray(u"Files/folders to copy"_s)}, {u"destination"_s, str(u"Destination folder"_s)}}, {u"sources"_s, u"destination"_s}),
             false),
        tool(u"archivos_move"_s, u"Mover"_s, u"Moves files into a folder. Requires write permission and user approval in the UI. Undoable."_s,
             schema({{u"sources"_s, strArray(u"Files/folders to move"_s)}, {u"destination"_s, str(u"Destination folder"_s)}}, {u"sources"_s, u"destination"_s}),
             false, true),
    };
}

int McpServer::run()
{
    std::ios::sync_with_stdio(false);
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty())
            continue;
        QJsonParseError perr;
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(line), &perr);
        QJsonObject resp;
        if (perr.error != QJsonParseError::NoError || !doc.isObject())
            resp = rpcError(QJsonValue::Null, -32700, u"Parse error"_s);
        else
            resp = handle(doc.object());
        if (!resp.isEmpty()) {
            std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << '\n';
            std::cout.flush();
        }
    }
    return 0;
}

QJsonObject McpServer::handle(const QJsonObject &msg)
{
    const QJsonValue id = msg.value(u"id"_s);
    const QString method = msg.value(u"method"_s).toString();
    const QJsonObject params = msg.value(u"params"_s).toObject();
    const bool isNotification = !msg.contains(u"id"_s);
    if (isNotification)
        return {};

    // Modern (stateless) requests carry the protocol version in _meta.
    const QJsonObject meta = params.value(u"_meta"_s).toObject();
    const bool modern = meta.contains(kMetaVersion);
    if (modern && !kModern.contains(meta.value(kMetaVersion).toString()))
        return rpcError(id, -32022, u"Unsupported protocol version"_s, QJsonObject{{u"supported"_s, QJsonArray::fromStringList(kModern + kLegacy)}});

    auto ok = [&](QJsonObject result, bool cacheable = false) {
        result.insert(u"resultType"_s, u"complete"_s);
        if (cacheable) {
            result.insert(u"ttlMs"_s, 300000);
            result.insert(u"cacheScope"_s, u"private"_s);
        }
        if (modern)
            result.insert(u"_meta"_s, QJsonObject{{u"io.modelcontextprotocol/serverInfo"_s, serverInfo()}});
        return QJsonObject{{u"jsonrpc"_s, u"2.0"_s}, {u"id"_s, id}, {u"result"_s, result}};
    };

    if (method == "initialize"_L1) {
        const QString wanted = params.value(u"protocolVersion"_s).toString();
        const QJsonObject ci = params.value(u"clientInfo"_s).toObject();
        const QString who = ci.value(u"title"_s).toString(ci.value(u"name"_s).toString());
        if (!who.isEmpty())
            g_clientName = who.left(60);
        m_legacy = true;
        m_version = kLegacy.contains(wanted) ? wanted : kLegacy.first();
        return ok({{u"protocolVersion"_s, m_version}, {u"capabilities"_s, capabilities()}, {u"serverInfo"_s, serverInfo()},
                   {u"instructions"_s, QString::fromUtf8(kInstructions)}});
    }
    if (method == "server/discover"_L1)
        return ok({{u"supportedVersions"_s, QJsonArray::fromStringList(kModern + kLegacy)}, {u"capabilities"_s, capabilities()},
                   {u"serverInfo"_s, serverInfo()}, {u"instructions"_s, QString::fromUtf8(kInstructions)}},
                  true);
    if (method == "ping"_L1)
        return ok({});
    if (method == "tools/list"_L1)
        return ok({{u"tools"_s, toolDefinitions()}}, true);
    if (method == "tools/call"_L1) {
        const QString name = params.value(u"name"_s).toString();
        bool known = false;
        for (const QJsonValue &t : toolDefinitions())
            known |= t.toObject().value(u"name"_s).toString() == name;
        if (!known)
            return rpcError(id, -32602, u"Unknown tool: "_s + name);
        return ok(callTool(name, params.value(u"arguments"_s).toObject()));
    }
    if (method == "resources/list"_L1)
        return ok({{u"resources"_s, QJsonArray()}}, true);
    if (method == "prompts/list"_L1)
        return ok({{u"prompts"_s, QJsonArray()}}, true);
    return rpcError(id, -32601, u"Method not found: "_s + method);
}

QString McpServer::resolvePath(const QString &p, bool mustExist, QString *err) const
{
    QString path = Config::expand(p);
    const QUrl u(path);
    if (u.isLocalFile())
        path = u.toLocalFile();
    if (QDir::isRelativePath(path))
        path = QDir::home().absoluteFilePath(path);
    QFileInfo fi(path);
    QString canon = fi.exists() ? fi.canonicalFilePath() : QFileInfo(fi.absolutePath()).canonicalFilePath() + u'/' + fi.fileName();
    if (canon.isEmpty() || (mustExist && !fi.exists())) {
        *err = u"Not found: "_s + p;
        return {};
    }
    for (const QString &root : Config::instance()->mcpRoots()) {
        const QString r = QFileInfo(root).canonicalFilePath();
        if (!r.isEmpty() && (canon == r || canon.startsWith(r + u'/')))
            return canon;
    }
    *err = u"Path is outside the allowed roots (mcp/roots in %1)"_s.arg(Config::instance()->configFile());
    return {};
}

QJsonObject McpServer::callTool(const QString &name, const QJsonObject &args)
{
    QString err;
    if (name == "archivos_get_current_view"_L1) {
        const QJsonObject st = gui({{u"cmd"_s, u"state"_s}});
        if (st.isEmpty())
            return textResult(u"Archivos is not open. Call archivos_reveal to open it."_s, true);
        QJsonObject o = st;
        o.remove(u"ok"_s);
        return jsonResult(o);
    }
    if (name == "archivos_list_directory"_L1) {
        const QString dir = resolvePath(args.value(u"path"_s).toString(), true, &err);
        if (dir.isEmpty())
            return textResult(err, true);
        QDir::Filters f = QDir::AllEntries | QDir::NoDotAndDotDot;
        if (args.value(u"show_hidden"_s).toBool())
            f |= QDir::Hidden;
        const int limit = qBound(1, args.value(u"limit"_s).toInt(500), 5000);
        QJsonArray entries;
        const QFileInfoList list = QDir(dir).entryInfoList(f, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &e : list) {
            if (entries.size() >= limit)
                break;
            QJsonObject o{{u"name"_s, e.fileName()}, {u"type"_s, e.isDir() ? u"dir"_s : e.isSymLink() ? u"link"_s : u"file"_s},
                          {u"modified"_s, e.lastModified().toString(Qt::ISODate)}};
            if (!e.isDir())
                o.insert(u"size"_s, double(e.size()));
            if (e.isDir() && QFileInfo::exists(e.absoluteFilePath() + u"/.git"_s))
                o.insert(u"git"_s, true);
            if (!e.isDir() && SecretGuard::isSecretName(e.fileName()))
                o.insert(u"secret"_s, true);
            entries << o;
        }
        return jsonResult({{u"path"_s, dir}, {u"total"_s, int(list.size())}, {u"entries"_s, entries}});
    }
    if (name == "archivos_read_file"_L1) {
        const QString path = resolvePath(args.value(u"path"_s).toString(), true, &err);
        if (path.isEmpty())
            return textResult(err, true);
        const QFileInfo fi(path);
        if (fi.isDir())
            return textResult(u"This is a folder; use archivos_list_directory."_s, true);
        if (SecretGuard::isSecretName(fi.fileName()))
            return textResult(u"Blocked: the file name looks like a secret (keys, tokens, .env)."_s, true);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return textResult(f.errorString(), true);
        const qint64 offset = qMax<qint64>(0, args.value(u"offset"_s).toInteger(0));
        const qint64 max = qBound<qint64>(1, args.value(u"max_bytes"_s).toInteger(65536), qint64(1024 * 1024));
        const QByteArray probe = f.read(8192);
        if (SecretGuard::looksBinary(probe))
            return textResult(u"Binary file; use archivos_inspect to read its metadata/schema."_s, true);
        f.seek(offset);
        const QByteArray data = f.read(max);
        const QString why = SecretGuard::scanContent(data);
        if (!why.isEmpty())
            return textResult(u"Blocked: the file contains %1."_s.arg(why), true);
        QString text = QString::fromUtf8(data);
        if (offset + data.size() < fi.size())
            text += u"\n\n[truncated: %1 of %2 bytes, continue with offset=%3]"_s.arg(offset + data.size()).arg(fi.size()).arg(offset + data.size());
        return textResult(text);
    }
    if (name == "archivos_search"_L1) {
        QString base = args.value(u"path"_s).toString();
        if (base.isEmpty()) {
            const QJsonObject st = gui({{u"cmd"_s, u"state"_s}}, 800);
            base = st.value(u"currentFolder"_s).toString(QDir::homePath());
        }
        const QString dir = resolvePath(base, true, &err);
        if (dir.isEmpty())
            return textResult(err, true);
        const QString q = args.value(u"query"_s).toString();
        const int limit = qBound(1, args.value(u"limit"_s).toInt(100), 1000);
        const bool content = args.value(u"content"_s).toBool();
        QProcess p;
        if (content) {
            if (QStandardPaths::findExecutable(u"rg"_s).isEmpty())
                p.start(u"grep"_s, {u"-rIn"_s, u"--max-count=3"_s, u"-F"_s, u"--"_s, q, dir});
            else
                p.start(u"rg"_s, {u"-n"_s, u"--max-count=3"_s, u"-F"_s, u"--color=never"_s, u"--"_s, q, dir});
        } else if (!QStandardPaths::findExecutable(u"fd"_s).isEmpty()) {
            p.start(u"fd"_s, {u"--absolute-path"_s, u"-i"_s, u"-F"_s, u"--max-results"_s, QString::number(limit), u"--"_s, q, dir});
        } else {
            p.start(u"find"_s, {dir, u"-iname"_s, u"*"_s + q + u"*"_s});
        }
        p.waitForFinished(20000);
        QStringList lines = QString::fromUtf8(p.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
        lines.erase(std::remove_if(lines.begin(), lines.end(), [](const QString &l) { return SecretGuard::isSecretName(QFileInfo(l.section(u':', 0, 0)).fileName()); }),
                    lines.end());
        const bool more = lines.size() > limit;
        lines = lines.mid(0, limit);
        return jsonResult({{u"root"_s, dir}, {u"mode"_s, content ? u"content"_s : u"name"_s}, {u"results"_s, QJsonArray::fromStringList(lines)},
                           {u"truncated"_s, more}});
    }
    if (name == "archivos_inspect"_L1) {
        const QString path = resolvePath(args.value(u"path"_s).toString(), true, &err);
        if (path.isEmpty())
            return textResult(err, true);
        Inspector::Options opts;
        opts.maxRows = qBound(0, args.value(u"sample_rows"_s).toInt(10), 100);
        QVariantMap m = Inspector::inspect(path, opts);
        if (m.contains(u"secret"_s)) {
            m.remove(u"text"_s);
            m.remove(u"rows"_s);
        }
        if (m.value(u"kind"_s) == "text"_L1 || m.value(u"kind"_s) == "markdown"_L1) {
            const QString t = m.value(u"text"_s).toString();
            m.insert(u"text"_s, t.left(4000));
            m.insert(u"lines"_s, int(t.count(u'\n')) + 1);
        }
        if (m.value(u"kind"_s) == "folder"_L1)
            m.insert(u"readme"_s, m.value(u"readme"_s).toString().left(4000));
        return jsonResult(QJsonObject::fromVariantMap(m));
    }
    if (name == "archivos_context_pack"_L1) {
        QStringList paths;
        for (const QJsonValue &v : args.value(u"paths"_s).toArray()) {
            const QString p = resolvePath(v.toString(), true, &err);
            if (p.isEmpty())
                return textResult(err, true);
            paths << p;
        }
        if (paths.isEmpty())
            return textResult(u"paths is empty"_s, true);
        const auto r = ContextPacker::build(paths, args.value(u"format"_s).toString(u"markdown"_s));
        QJsonObject summary = QJsonObject::fromVariantMap(r.summary());
        summary.remove(u"included"_s);
        return {{u"content"_s, QJsonArray{QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, r.text}},
                                          QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Compact))}}}},
                {u"isError"_s, false}};
    }
    if (name == "archivos_reveal"_L1) {
        const QString path = resolvePath(args.value(u"path"_s).toString(), true, &err);
        if (path.isEmpty())
            return textResult(err, true);
        if (!ensureGui())
            return textResult(u"Could not start Archivos"_s, true);
        gui({{u"cmd"_s, args.value(u"preview"_s).toBool() ? u"preview"_s : u"reveal"_s}, {u"path"_s, path}});
        return textResult(u"Shown in Archivos: "_s + path);
    }

    // ---- writes: opt-in + explicit approval in the GUI
    if (!Config::instance()->mcpAllowWrite())
        return textResult(u"Writes are disabled. Ask the user to enable them in Archivos → IA panel → «Permitir escritura a agentes»."_s, true);
    if (!IpcClient::isRunning())
        return textResult(u"Archivos must be open so the user can approve changes. Call archivos_reveal first."_s, true);
    QJsonObject req{{u"cmd"_s, u"confirm"_s}};
    if (name == "archivos_create_folder"_L1) {
        const QString path = resolvePath(args.value(u"path"_s).toString(), false, &err);
        if (path.isEmpty())
            return textResult(err, true);
        req.insert(u"action"_s, u"mkdir"_s);
        req.insert(u"title"_s, u"Crear carpeta"_s);
        req.insert(u"detail"_s, path);
        req.insert(u"args"_s, QJsonObject{{u"path"_s, path}});
    } else {
        const bool trash = name == "archivos_move_to_trash"_L1;
        QStringList srcs;
        for (const QJsonValue &v : args.value(trash ? u"paths"_s : u"sources"_s).toArray()) {
            const QString p = resolvePath(v.toString(), true, &err);
            if (p.isEmpty())
                return textResult(err, true);
            srcs << p;
        }
        if (srcs.isEmpty())
            return textResult(u"Nothing to do"_s, true);
        if (trash) {
            req.insert(u"action"_s, u"trash"_s);
            req.insert(u"title"_s, u"Mover a la papelera"_s);
            req.insert(u"args"_s, QJsonObject{{u"paths"_s, QJsonArray::fromStringList(srcs)}});
            req.insert(u"detail"_s, srcs.join(u'\n'));
        } else {
            const QString dest = resolvePath(args.value(u"destination"_s).toString(), true, &err);
            if (dest.isEmpty())
                return textResult(err, true);
            const bool move = name == "archivos_move"_L1;
            req.insert(u"action"_s, move ? u"move"_s : u"copy"_s);
            req.insert(u"title"_s, move ? u"Mover archivos"_s : u"Copiar archivos"_s);
            req.insert(u"args"_s, QJsonObject{{u"sources"_s, QJsonArray::fromStringList(srcs)}, {u"destination"_s, dest}});
            req.insert(u"detail"_s, srcs.join(u'\n') + u"\n→ "_s + dest);
        }
    }
    const QJsonObject r = gui(req, 120000);
    if (r.isEmpty())
        return textResult(u"No answer from the user (timed out)."_s, true);
    if (!r.value(u"approved"_s).toBool())
        return textResult(u"The user rejected this operation in Archivos."_s, true);
    return textResult(u"Approved and running (the user can undo it with Ctrl+Z in Archivos)."_s);
}
