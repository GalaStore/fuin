// SPDX-License-Identifier: MIT
// Synchronous, GUI-free file inspection used by the preview pane (in a worker thread) and by the
// MCP server: text/markdown, tabular data (CSV/TSV/JSON/JSONL/Parquet) with schema + stats, ML
// model headers (safetensors, GGUF, ONNX), images and folders.
#pragma once

#include <QString>
#include <QVariantMap>

namespace Inspector
{
struct Options {
    int maxRows = 200;
    int maxColumns = 80;
    qint64 maxTextBytes = 256 * 1024;
};

QVariantMap inspect(const QString &path, const Options &opts = {});

// Individual inspectors (exposed for tests / MCP).
QVariantMap inspectCsv(const QString &path, QChar delimiter, const Options &opts);
QVariantMap inspectJson(const QString &path, const Options &opts);
QVariantMap inspectJsonLines(const QString &path, const Options &opts);
QVariantMap inspectParquet(const QString &path, const Options &opts);
QVariantMap inspectSafetensors(const QString &path);
QVariantMap inspectGguf(const QString &path);
QVariantMap inspectOnnx(const QString &path);
QVariantMap inspectFolder(const QString &path);

bool hasParquet();
QString humanCount(double n); // 7.24 B, 13.1 M ...
}
