// SPDX-License-Identifier: MIT
// File operations through KIO jobs (copy/move/trash/delete/mkdir/rename/restore), progress,
// undo (KIO::FileUndoManager), clipboard, drag & drop payloads and launching (open / open with /
// terminal / editor).
#pragma once

#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVariantList>

class KJob;

class FileOps : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY progressChanged)
    Q_PROPERTY(QString opLabel READ opLabel NOTIFY progressChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString speed READ speed NOTIFY progressChanged)
    Q_PROPERTY(int jobCount READ jobCount NOTIFY progressChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoChanged)
    Q_PROPERTY(int clipboardCount READ clipboardCount NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardCut READ clipboardCut NOTIFY clipboardChanged)
    Q_PROPERTY(QStringList clipboardUrls READ clipboardUrls NOTIFY clipboardChanged)

public:
    explicit FileOps(QObject *parent = nullptr);

    bool busy() const { return !m_jobs.isEmpty(); }
    QString opLabel() const { return m_label; }
    qreal progress() const { return m_progress; }
    QString speed() const { return m_speed; }
    int jobCount() const { return int(m_jobs.size()); }
    bool canUndo() const;
    QString undoText() const;
    int clipboardCount() const;
    bool clipboardCut() const;
    QStringList clipboardUrls() const;

    Q_INVOKABLE void copy(const QStringList &urls, const QString &destDir);
    Q_INVOKABLE void move(const QStringList &urls, const QString &destDir);
    Q_INVOKABLE void trash(const QStringList &urls);
    Q_INVOKABLE void deleteForever(const QStringList &urls);
    Q_INVOKABLE void restore(const QStringList &trashUrls);
    Q_INVOKABLE void emptyTrash();
    Q_INVOKABLE void mkdir(const QString &dirUrl, const QString &name);
    Q_INVOKABLE void newFile(const QString &dirUrl, const QString &name);
    Q_INVOKABLE void rename(const QString &url, const QString &newName);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void cancelAll();

    Q_INVOKABLE void setClipboard(const QStringList &urls, bool cut);
    Q_INVOKABLE void paste(const QString &destDir);
    Q_INVOKABLE void copyText(const QString &text);
    // Drop handler: action "copy" | "move" | "auto" (move inside the same device, copy otherwise)
    Q_INVOKABLE void drop(const QStringList &urls, const QString &destDir, const QString &action);

    Q_INVOKABLE void open(const QString &url);
    Q_INVOKABLE QVariantList openWithApps(const QString &mimeType) const;
    Q_INVOKABLE void openWith(const QString &storageId, const QStringList &urls);
    Q_INVOKABLE void openWithDialog(const QStringList &urls);
    Q_INVOKABLE void openTerminal(const QString &dir, const QString &command = QString());
    Q_INVOKABLE void openInEditor(const QString &path);
    Q_INVOKABLE void openInFileManagerWindow(const QString &url);
    Q_INVOKABLE bool hasProgram(const QString &name) const;
    Q_INVOKABLE QString humanSize(double bytes) const;

Q_SIGNALS:
    void progressChanged();
    void undoChanged();
    void clipboardChanged();
    void notify(const QString &text, const QString &kind, bool undoable);
    void created(const QString &url); // new folder / renamed item: views select it

private:
    void track(KJob *job, const QString &label, const QString &doneText, bool undoable);
    void recompute();
    static QList<QUrl> toUrls(const QStringList &urls);
    static QUrl toUrl(const QString &s);

    QSet<KJob *> m_jobs;
    QString m_label;
    qreal m_progress = 0;
    QString m_speed;
};
