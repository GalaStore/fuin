// SPDX-License-Identifier: MIT
// "Contexto para IA": turns files/folders into a single Markdown or XML document (tree + file
// contents) ready to paste into an LLM chat. Respects .gitignore (git ls-files), skips binaries,
// lockfiles, huge files and anything that looks like a secret, and estimates the token count.
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace ContextPacker
{
struct Limits {
    qint64 maxFileBytes = 256 * 1024;
    qint64 maxTotalBytes = 2 * 1024 * 1024;
    int maxFiles = 400;
};
struct Result {
    QString text;
    QStringList included;
    QVariantList skipped; // [{path, reason}]
    qint64 bytes = 0;
    int tokens = 0;
    int secretsBlocked = 0;
    int summarized = 0; // data/model files included as schema summaries
    QString root;
    QVariantMap summary() const;
};
// format: "markdown" | "xml"
Result build(const QStringList &paths, const QString &format, const Limits &limits = {});
int estimateTokens(const QString &text);
}

// QML wrapper: async build + copy to clipboard.
class ContextPack : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantMap last READ last NOTIFY lastChanged)
public:
    explicit ContextPack(QObject *parent = nullptr);
    bool busy() const { return m_busy; }
    QVariantMap last() const { return m_last; }
    // Builds the pack in a worker thread; copies it to the clipboard when `copy` is true.
    Q_INVOKABLE void build(const QStringList &pathsOrUrls, const QString &format, bool copy);
    Q_INVOKABLE void copyLast();
    Q_INVOKABLE QString lastText() const { return m_text; }

Q_SIGNALS:
    void busyChanged();
    void lastChanged();
    void copied(const QVariantMap &summary);

private:
    bool m_busy = false;
    QVariantMap m_last;
    QString m_text;
};
