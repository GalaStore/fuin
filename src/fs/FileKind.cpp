// SPDX-License-Identifier: MIT
#include "FileKind.h"

#include <QSet>

QString FileKind::extension(const QString &fileName)
{
    const int dot = fileName.lastIndexOf(QLatin1Char('.'));
    return dot > 0 ? fileName.mid(dot + 1).toLower() : QString();
}

QString FileKind::classify(const QString &fileName, const QString &mime, bool isDir)
{
    if (isDir)
        return QStringLiteral("folder");
    static const QSet<QString> data{QStringLiteral("csv"), QStringLiteral("tsv"), QStringLiteral("parquet"), QStringLiteral("jsonl"),
                                    QStringLiteral("ndjson"), QStringLiteral("arrow"), QStringLiteral("feather"), QStringLiteral("avro"),
                                    QStringLiteral("orc"), QStringLiteral("xlsx"), QStringLiteral("sqlite"), QStringLiteral("sqlite3"),
                                    QStringLiteral("db"), QStringLiteral("duckdb"), QStringLiteral("json")};
    static const QSet<QString> model{QStringLiteral("safetensors"), QStringLiteral("gguf"), QStringLiteral("onnx"), QStringLiteral("pt"),
                                     QStringLiteral("pth"), QStringLiteral("ckpt"), QStringLiteral("h5"), QStringLiteral("keras"),
                                     QStringLiteral("pkl"), QStringLiteral("tflite"), QStringLiteral("mlmodel"), QStringLiteral("ggml")};
    static const QSet<QString> code{
        QStringLiteral("py"),   QStringLiteral("js"),   QStringLiteral("ts"),    QStringLiteral("tsx"),  QStringLiteral("jsx"),
        QStringLiteral("rs"),   QStringLiteral("go"),   QStringLiteral("c"),     QStringLiteral("h"),    QStringLiteral("cpp"),
        QStringLiteral("hpp"),  QStringLiteral("cc"),   QStringLiteral("java"),  QStringLiteral("kt"),   QStringLiteral("rb"),
        QStringLiteral("php"),  QStringLiteral("sh"),   QStringLiteral("bash"),  QStringLiteral("zsh"),  QStringLiteral("fish"),
        QStringLiteral("lua"),  QStringLiteral("qml"),  QStringLiteral("sql"),   QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("toml"), QStringLiteral("ini"),  QStringLiteral("conf"),  QStringLiteral("xml"),  QStringLiteral("html"),
        QStringLiteral("css"),  QStringLiteral("scss"), QStringLiteral("vue"),   QStringLiteral("svelte"), QStringLiteral("cmake"),
        QStringLiteral("ipynb"), QStringLiteral("r"),   QStringLiteral("scala"), QStringLiteral("swift"), QStringLiteral("dart"),
        QStringLiteral("zig"),  QStringLiteral("nix"),  QStringLiteral("tf"),    QStringLiteral("hcl"),  QStringLiteral("dockerfile")};
    static const QSet<QString> archive{QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("tgz"), QStringLiteral("xz"),
                                       QStringLiteral("zst"), QStringLiteral("bz2"), QStringLiteral("7z"), QStringLiteral("rar"), QStringLiteral("deb"),
                                       QStringLiteral("rpm"), QStringLiteral("iso")};
    const QString ext = extension(fileName);
    const QString lower = fileName.toLower();
    if (ext == QLatin1String("pdf") || mime == QLatin1String("application/pdf"))
        return QStringLiteral("pdf");
    if (model.contains(ext))
        return QStringLiteral("model");
    if (data.contains(ext))
        return QStringLiteral("data");
    if (ext == QLatin1String("md") || ext == QLatin1String("markdown") || mime == QLatin1String("text/markdown"))
        return QStringLiteral("markdown");
    if (mime.startsWith(QLatin1String("image/")))
        return QStringLiteral("image");
    if (mime.startsWith(QLatin1String("video/")))
        return QStringLiteral("video");
    if (mime.startsWith(QLatin1String("audio/")))
        return QStringLiteral("audio");
    if (archive.contains(ext))
        return QStringLiteral("archive");
    if (code.contains(ext) || lower == QLatin1String("makefile") || lower == QLatin1String("dockerfile") || lower == QLatin1String("cmakelists.txt")
        || mime.startsWith(QLatin1String("text/x-")) || mime == QLatin1String("application/x-shellscript") || mime == QLatin1String("application/javascript"))
        return QStringLiteral("code");
    if (mime.startsWith(QLatin1String("text/")) || ext == QLatin1String("txt") || ext == QLatin1String("log") || lower.startsWith(QLatin1String(".env")))
        return QStringLiteral("text");
    return QStringLiteral("other");
}
