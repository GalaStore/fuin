// SPDX-License-Identifier: MIT
#include "FileOps.h"
#include "../app/Config.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KFileItem>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/EmptyTrashJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>
#include <KIO/MkdirJob>
#include <KIO/OpenUrlJob>
#include <KIO/Paste>
#include <KIO/RestoreJob>
#include <KIO/SimpleJob>
#include <KIO/StoredTransferJob>
#include <KService>
#include <KSharedConfig>
#include <KUrlMimeData>

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>

#include <memory>

FileOps::FileOps(QObject *parent)
    : QObject(parent)
{
    auto *undo = KIO::FileUndoManager::self();
    connect(undo, &KIO::FileUndoManager::undoAvailable, this, &FileOps::undoChanged);
    connect(undo, &KIO::FileUndoManager::undoTextChanged, this, &FileOps::undoChanged);
    connect(undo, &KIO::FileUndoManager::undoJobFinished, this, [this] {
        Q_EMIT notify(tr("Undone"), QStringLiteral("ok"), false);
    });
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &FileOps::clipboardChanged);
}

QUrl FileOps::toUrl(const QString &s)
{
    QUrl u(s);
    if (u.scheme().isEmpty())
        u = QUrl::fromLocalFile(Config::expand(s));
    return u;
}

QList<QUrl> FileOps::toUrls(const QStringList &urls)
{
    QList<QUrl> out;
    for (const QString &s : urls)
        if (!s.isEmpty())
            out << toUrl(s);
    return out;
}

bool FileOps::canUndo() const { return KIO::FileUndoManager::self()->isUndoAvailable(); }
QString FileOps::undoText() const { return KIO::FileUndoManager::self()->undoText(); }

void FileOps::undo()
{
    if (canUndo())
        KIO::FileUndoManager::self()->undo();
}

QString FileOps::humanSize(double bytes) const
{
    return KIO::convertSize(KIO::filesize_t(qMax(0.0, bytes)));
}

// ---------------------------------------------------------------- job tracking

void FileOps::track(KJob *job, const QString &label, const QString &doneText, bool undoable)
{
    job->setProperty("fuinLabel", label);
    m_jobs.insert(job);
    connect(job, &KJob::percentChanged, this, &FileOps::recompute);
    connect(job, &KJob::speed, this, [this](KJob *, unsigned long bps) {
        m_speed = bps ? KIO::convertSize(bps) + QStringLiteral("/s") : QString();
        Q_EMIT progressChanged();
    });
    if (auto *cj = qobject_cast<KIO::CopyJob *>(job)) {
        connect(cj, &KIO::CopyJob::copying, this, [this, cj, label](KIO::Job *, const QUrl &src, const QUrl &) {
            cj->setProperty("fuinLabel", label + QStringLiteral(" · ") + src.fileName());
            recompute();
        });
        connect(cj, &KIO::CopyJob::moving, this, [this, cj, label](KIO::Job *, const QUrl &src, const QUrl &) {
            cj->setProperty("fuinLabel", label + QStringLiteral(" · ") + src.fileName());
            recompute();
        });
    }
    connect(job, &KJob::result, this, [this, job, doneText, undoable] {
        m_jobs.remove(job);
        if (job->error() && job->error() != KJob::KilledJobError)
            Q_EMIT notify(job->errorString(), QStringLiteral("error"), false);
        else if (!job->error() && !doneText.isEmpty())
            Q_EMIT notify(doneText, QStringLiteral("ok"), undoable);
        recompute();
    });
    recompute();
}

void FileOps::recompute()
{
    if (m_jobs.isEmpty()) {
        m_label.clear();
        m_progress = 0;
        m_speed.clear();
    } else {
        qreal sum = 0;
        for (KJob *j : std::as_const(m_jobs))
            sum += j->percent();
        m_progress = sum / (100.0 * m_jobs.size());
        m_label = (*m_jobs.cbegin())->property("fuinLabel").toString();
    }
    Q_EMIT progressChanged();
}

void FileOps::cancelAll()
{
    const auto jobs = m_jobs;
    for (KJob *j : jobs)
        j->kill(KJob::EmitResult);
}

static QString countText(int n, const QString &one, const QString &many)
{
    return n == 1 ? one : many.arg(n);
}

static void withUi(KJob *job)
{
    // Default KIO UI delegate: conflict ("already exists") dialogs, auth prompts, error handling.
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoWarningHandlingEnabled, nullptr));
}

// ---------------------------------------------------------------- operations

void FileOps::copy(const QStringList &urls, const QString &destDir)
{
    const QList<QUrl> src = toUrls(urls);
    if (src.isEmpty())
        return;
    auto *job = KIO::copy(src, toUrl(destDir));
    withUi(job);
    KIO::FileUndoManager::self()->recordCopyJob(job);
    track(job, tr("Copying"), countText(int(src.size()), tr("Copied"), tr("%1 items copied")), true);
}

void FileOps::move(const QStringList &urls, const QString &destDir)
{
    const QList<QUrl> src = toUrls(urls);
    if (src.isEmpty())
        return;
    auto *job = KIO::move(src, toUrl(destDir));
    withUi(job);
    KIO::FileUndoManager::self()->recordCopyJob(job);
    track(job, tr("Moving"), countText(int(src.size()), tr("Moved"), tr("%1 items moved")), true);
}

void FileOps::trash(const QStringList &urls)
{
    const QList<QUrl> src = toUrls(urls);
    if (src.isEmpty())
        return;
    auto *job = KIO::trash(src);
    withUi(job);
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Trash, src, QUrl(QStringLiteral("trash:/")), job);
    track(job, tr("Moving to the trash"), countText(int(src.size()), tr("Moved to the trash"), tr("%1 items moved to the trash")), true);
}

void FileOps::deleteForever(const QStringList &urls)
{
    const QList<QUrl> src = toUrls(urls);
    if (src.isEmpty())
        return;
    auto *job = KIO::del(src);
    withUi(job);
    track(job, tr("Deleting"), countText(int(src.size()), tr("Deleted"), tr("%1 items deleted")), false);
}

void FileOps::restore(const QStringList &trashUrls)
{
    const QList<QUrl> src = toUrls(trashUrls);
    if (src.isEmpty())
        return;
    auto *job = KIO::restoreFromTrash(src);
    withUi(job);
    track(job, tr("Restoring"), countText(int(src.size()), tr("Restored"), tr("%1 items restored")), false);
}

void FileOps::emptyTrash()
{
    auto *job = KIO::emptyTrash();
    withUi(job);
    track(job, tr("Emptying the trash"), tr("Trash emptied"), false);
}

void FileOps::mkdir(const QString &dirUrl, const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || n.contains(QLatin1Char('/')))
        return;
    QUrl u = toUrl(dirUrl);
    u.setPath(u.path() + (u.path().endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + n);
    auto *job = KIO::mkdir(u);
    withUi(job);
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Mkdir, {}, u, job);
    connect(job, &KJob::result, this, [this, job, u] {
        if (!job->error())
            Q_EMIT created(u.toString());
    });
    track(job, tr("Creating folder"), tr("Folder created"), true);
}

void FileOps::newFile(const QString &dirUrl, const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || n.contains(QLatin1Char('/')))
        return;
    QUrl u = toUrl(dirUrl);
    u.setPath(u.path() + (u.path().endsWith(QLatin1Char('/')) ? QString() : QStringLiteral("/")) + n);
    auto *job = KIO::storedPut(QByteArray(), u, -1, KIO::HideProgressInfo);
    withUi(job);
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Put, {}, u, job);
    connect(job, &KJob::result, this, [this, job, u] {
        if (!job->error())
            Q_EMIT created(u.toString());
    });
    track(job, tr("Creating file"), tr("File created"), true);
}

void FileOps::rename(const QString &url, const QString &newName)
{
    const QString n = newName.trimmed();
    const QUrl src = toUrl(url);
    if (n.isEmpty() || n.contains(QLatin1Char('/')) || n == src.fileName())
        return;
    QUrl dst = src.adjusted(QUrl::RemoveFilename);
    dst.setPath(dst.path() + n);
    auto *job = KIO::moveAs(src, dst, KIO::HideProgressInfo);
    withUi(job);
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Rename, {src}, dst, job);
    connect(job, &KJob::result, this, [this, job, dst] {
        if (!job->error())
            Q_EMIT created(dst.toString());
    });
    track(job, tr("Renaming"), tr("Renamed"), true);
}

// ---------------------------------------------------------------- clipboard & dnd

void FileOps::setClipboard(const QStringList &urls, bool cut)
{
    const QList<QUrl> list = toUrls(urls);
    if (list.isEmpty())
        return;
    auto *md = new QMimeData;
    KUrlMimeData::setUrls(list, list, md);
    KIO::setClipboardDataCut(md, cut);
    // Also plain text paths, handy for terminals/editors.
    QStringList paths;
    for (const QUrl &u : list)
        paths << (u.isLocalFile() ? u.toLocalFile() : u.toString());
    md->setText(paths.join(QLatin1Char('\n')));
    QApplication::clipboard()->setMimeData(md);
    Q_EMIT notify(cut ? countText(int(list.size()), tr("Cut"), tr("%1 items cut"))
                      : countText(int(list.size()), tr("Copied to the clipboard"), tr("%1 items copied")),
                  QStringLiteral("info"), false);
}

QStringList FileOps::clipboardUrls() const
{
    QStringList out;
    const QMimeData *md = QApplication::clipboard()->mimeData();
    if (!md)
        return out;
    for (const QUrl &u : KUrlMimeData::urlsFromMimeData(md))
        out << u.toString();
    return out;
}

int FileOps::clipboardCount() const { return int(clipboardUrls().size()); }

bool FileOps::clipboardCut() const
{
    const QMimeData *md = QApplication::clipboard()->mimeData();
    return md && KIO::isClipboardDataCut(md);
}

void FileOps::paste(const QString &destDir)
{
    const QMimeData *md = QApplication::clipboard()->mimeData();
    if (!md)
        return;
    const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(md);
    if (urls.isEmpty()) {
        Q_EMIT notify(tr("There are no files on the clipboard"), QStringLiteral("info"), false);
        return;
    }
    QStringList s;
    for (const QUrl &u : urls)
        s << u.toString();
    if (KIO::isClipboardDataCut(md)) {
        move(s, destDir);
        QApplication::clipboard()->clear();
    } else {
        copy(s, destDir);
    }
}

void FileOps::copyText(const QString &text)
{
    QApplication::clipboard()->setText(text);
    Q_EMIT notify(tr("Copied to the clipboard"), QStringLiteral("info"), false);
}

void FileOps::drop(const QStringList &urls, const QString &destDir, const QString &action)
{
    QStringList src;
    const QUrl dest = toUrl(destDir);
    for (const QString &s : urls) {
        const QUrl u = toUrl(s);
        // ignore drops onto itself / into its own folder
        if (u == dest || u.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == dest.adjusted(QUrl::StripTrailingSlash))
            continue;
        src << u.toString();
    }
    if (src.isEmpty())
        return;
    QString act = action;
    if (act == QLatin1String("auto")) {
        const QUrl first = toUrl(src.first());
        act = QStringLiteral("copy");
        if (first.isLocalFile() && dest.isLocalFile()
            && QStorageInfo(first.toLocalFile()).rootPath() == QStorageInfo(dest.toLocalFile()).rootPath())
            act = QStringLiteral("move");
    }
    if (dest.scheme() == QLatin1String("trash"))
        trash(src);
    else if (act == QLatin1String("move"))
        move(src, destDir);
    else
        copy(src, destDir);
}

// ---------------------------------------------------------------- launching

// Human-readable reason for a failed open: names the file and the application, and spells out
// the common "Terminal=true app but no usable terminal" case instead of KIO's generic text.
static QString openErrorText(const QUrl &url, const QString &mimeType, const QString &kioError)
{
    const QString file = url.fileName().isEmpty() ? url.toDisplayString(QUrl::PreferLocalFile) : url.fileName();
    const KService::Ptr app = mimeType.isEmpty() ? KService::Ptr() : KApplicationTrader::preferredService(mimeType);
    if (!app)
        return FileOps::tr("Could not open «%1»: %2").arg(file, kioError);
    if (app->terminal()) {
        const KConfigGroup general(KSharedConfig::openConfig(), QStringLiteral("General"));
        const QString terminal = QProcess::splitCommand(general.readPathEntry("TerminalApplication", QStringLiteral("konsole"))).value(0);
        if (terminal.isEmpty() || QStandardPaths::findExecutable(terminal).isEmpty())
            return FileOps::tr("Could not open «%1» with %2: it runs in a terminal and «%3» is not installed. "
                               "Set TerminalApplication in ~/.config/fuinrc or choose another default application.")
                .arg(file, app->name(), terminal.isEmpty() ? FileOps::tr("(none)") : terminal);
    }
    return FileOps::tr("Could not open «%1» with %2: %3").arg(file, app->name(), kioError);
}

void FileOps::open(const QString &url)
{
    const QUrl target = toUrl(url);
    auto *job = new KIO::OpenUrlJob(target);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->setRunExecutables(false);
    job->setShowOpenOrExecuteDialog(true);
    auto mimeType = std::make_shared<QString>();
    connect(job, &KIO::OpenUrlJob::mimeTypeFound, this, [mimeType](const QString &mime) {
        *mimeType = mime;
        Config::instance()->noteOpenedMimeType(mime);
    });
    connect(job, &KJob::result, this, [this, job, target, mimeType] {
        if (job->error() && job->error() != KJob::KilledJobError)
            Q_EMIT notify(openErrorText(target, *mimeType, job->errorString()), QStringLiteral("error"), false);
    });
    job->start();
}

QVariantList FileOps::openWithApps(const QString &mimeType) const
{
    QVariantList out;
    const KService::List apps = KApplicationTrader::queryByMimeType(mimeType);
    for (const KService::Ptr &s : apps) {
        if (out.size() >= 12)
            break;
        out << QVariantMap{{QStringLiteral("id"), s->storageId()}, {QStringLiteral("name"), s->name()}, {QStringLiteral("icon"), s->icon()}};
    }
    return out;
}

void FileOps::openWith(const QString &storageId, const QStringList &urls)
{
    const KService::Ptr service = KService::serviceByStorageId(storageId);
    if (!service)
        return;
    auto *job = new KIO::ApplicationLauncherJob(service);
    job->setUrls(toUrls(urls));
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->start();
}

void FileOps::openWithDialog(const QStringList &urls)
{
    // An ApplicationLauncherJob without a service shows KIO's "Open With" dialog.
    auto *job = new KIO::ApplicationLauncherJob();
    job->setUrls(toUrls(urls));
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->start();
}

bool FileOps::hasProgram(const QString &name) const
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

void FileOps::openTerminal(const QString &dir, const QString &command)
{
    QString d = dir;
    const QUrl u = toUrl(dir);
    if (u.isLocalFile())
        d = u.toLocalFile();
    if (!QFileInfo(d).isDir())
        d = QFileInfo(d).absolutePath();
    const QString term = Config::instance()->terminal();
    if (term.isEmpty()) {
        Q_EMIT notify(tr("No terminal found"), QStringLiteral("error"), false);
        return;
    }
    const QString bin = QFileInfo(term).fileName();
    QStringList args;
    if (bin == QLatin1String("kitty"))
        args << QStringLiteral("--directory") << d;
    else if (bin == QLatin1String("foot") || bin == QLatin1String("alacritty"))
        args << QStringLiteral("--working-directory") << d;
    else if (bin == QLatin1String("konsole"))
        args << QStringLiteral("--workdir") << d;
    else if (bin == QLatin1String("wezterm"))
        args << QStringLiteral("start") << QStringLiteral("--cwd") << d;
    else if (bin == QLatin1String("gnome-terminal"))
        args << QStringLiteral("--working-directory=") + d;
    // xdg-terminal-exec: the working directory is inherited and the command follows directly.
    if (!command.isEmpty()) {
        if (bin == QLatin1String("gnome-terminal"))
            args << QStringLiteral("--");
        else if (bin != QLatin1String("wezterm") && bin != QLatin1String("xdg-terminal-exec"))
            args << QStringLiteral("-e");
        args << QStringLiteral("sh") << QStringLiteral("-c") << command;
    }
    if (!QProcess::startDetached(term, args, d))
        Q_EMIT notify(tr("Could not open %1").arg(term), QStringLiteral("error"), false);
}

void FileOps::openInEditor(const QString &path)
{
    const QUrl u = toUrl(path);
    const QString p = u.isLocalFile() ? u.toLocalFile() : path;
    const QString ed = Config::instance()->editor();
    if (ed.isEmpty() || !QProcess::startDetached(ed, {p}, QFileInfo(p).isDir() ? p : QFileInfo(p).absolutePath()))
        Q_EMIT notify(tr("Editor not found"), QStringLiteral("error"), false);
}

void FileOps::openInFileManagerWindow(const QString &url)
{
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--new-window"), url});
}
