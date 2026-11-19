// SPDX-License-Identifier: MIT
#include "FolderModel.h"
#include "../ai/SecretGuard.h"
#include "../app/Config.h"
#include "FileKind.h"

#include <KCoreDirLister>
#include <KIO/Job>
#include <KIO/UDSEntry>

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QtConcurrent>

#include <algorithm>
#include <numeric>
#include <sys/stat.h>

Q_LOGGING_CATEGORY(lcFolderTiming, "fuin.folder.timing", QtWarningMsg)

namespace {
constexpr int kMaxResults = 2000;
constexpr int kMaxDirtyChecks = 40;
constexpr qsizetype kMaxInsertRuns = 8;
constexpr qsizetype kMaxInsertRows = 256;

KFileItem itemForLocalPath(const QString &path)
{
    const QFileInfo fi(path);
    KIO::UDSEntry e;
    e.fastInsert(KIO::UDSEntry::UDS_NAME, fi.fileName());
    e.fastInsert(KIO::UDSEntry::UDS_LOCAL_PATH, fi.absoluteFilePath());
    e.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, fi.isDir() ? S_IFDIR : S_IFREG);
    e.fastInsert(KIO::UDSEntry::UDS_SIZE, fi.isDir() ? 0 : fi.size());
    e.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, fi.lastModified().toSecsSinceEpoch());
    e.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0644);
    if (fi.isSymLink())
        e.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, fi.symLinkTarget());
    return KFileItem(e, QUrl::fromLocalFile(fi.absoluteFilePath()));
}

QString readGitHead(const QString &dir)
{
    QString gitPath = dir + QStringLiteral("/.git");
    QFileInfo gi(gitPath);
    if (gi.isFile()) { // worktree / submodule: "gitdir: <path>"
        QFile f(gitPath);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.startsWith(QLatin1String("gitdir:")))
            return {};
        gitPath = QDir(dir).absoluteFilePath(line.mid(7).trimmed());
    } else if (!gi.isDir()) {
        return {};
    }
    QFile head(gitPath + QStringLiteral("/HEAD"));
    if (!head.open(QIODevice::ReadOnly))
        return {};
    const QString h = QString::fromUtf8(head.readAll()).trimmed();
    if (h.startsWith(QLatin1String("ref: refs/heads/")))
        return h.mid(16);
    return h.left(7);
}
}

FolderEntry::FolderEntry(const KFileItem &it, const QCollator &collator)
    : KFileItem(it)
    , key(collator.sortKey(it.text()))
{
}

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_lister(new KCoreDirLister(this))
{
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_lister->setAutoErrorHandlingEnabled(false);
    m_lister->setDelayedMimeTypes(true);
    m_showHidden = Config::instance()->showHidden();
    m_sortField = Config::instance()->sortField();
    m_sortReversed = Config::instance()->sortReversed();
    // The lister always reports hidden items; accepts() filters them so Ctrl+H is instant.
    m_lister->setShowHiddenFiles(true);

    connect(m_lister, &KCoreDirLister::started, this, [this] {
        m_loading = true;
        m_error.clear();
        Q_EMIT loadingChanged();
    });
    connect(m_lister, &KCoreDirLister::completed, this, &FolderModel::onCompleted);
    connect(m_lister, &KCoreDirLister::canceled, this, [this] {
        m_loading = false;
        Q_EMIT loadingChanged();
    });
    connect(m_lister, &KCoreDirLister::clear, this, [this] {
        if (searching())
            return;
        m_source.clear();
        rebuild();
    });
    connect(m_lister, &KCoreDirLister::newItems, this, &FolderModel::onNewItems);
    connect(m_lister, &KCoreDirLister::itemsDeleted, this, &FolderModel::onDeleted);
    connect(m_lister, &KCoreDirLister::refreshItems, this, &FolderModel::onRefresh);
    connect(m_lister, &KCoreDirLister::redirection, this, [this](const QUrl &, const QUrl &newUrl) {
        m_url = newUrl;
        if (m_histIdx >= 0 && m_histIdx < m_history.size())
            m_history[m_histIdx] = newUrl;
        Q_EMIT locationChanged();
    });
    connect(m_lister, &KCoreDirLister::jobError, this, [this](KIO::Job *job) {
        m_error = job->errorString();
        m_loading = false;
        Q_EMIT loadingChanged();
    });

    m_mergeTimer.setSingleShot(true);
    m_mergeTimer.setInterval(100);
    connect(&m_mergeTimer, &QTimer::timeout, this, &FolderModel::mergePending);
    m_searchDebounce.setSingleShot(true);
    m_searchDebounce.setInterval(220);
    connect(&m_searchDebounce, &QTimer::timeout, this, &FolderModel::runSearch);
}

FolderModel::~FolderModel()
{
    if (m_search)
        m_search->kill();
    if (m_dirtyProc)
        m_dirtyProc->kill();
    if (m_repoProc)
        m_repoProc->kill();
}

// ---------------------------------------------------------------- model basics

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_view.size());
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {
        {NameRole, "name"},       {UrlRole, "url"},         {PathRole, "path"},         {IsDirRole, "isDir"},       {SizeRole, "size"},
        {MTimeRole, "mtime"},     {MimeRole, "mimeType"},   {MimeCommentRole, "mimeComment"}, {IconNameRole, "iconName"}, {HiddenRole, "isHidden"},
        {LinkRole, "isLink"},     {SecretRole, "isSecret"}, {KindRole, "kind"},         {ExtRole, "ext"},           {GitBranchRole, "gitBranch"},
        {GitDirtyRole, "gitDirty"}, {SelectedRole, "isSelected"}, {LocationRole, "location"}, {ThumbRole, "thumb"},
    };
}

QVariant FolderModel::roleValue(const KFileItem &it, int role) const
{
    switch (role) {
    case NameRole:
        return it.text();
    case UrlRole:
        return it.url().toString();
    case PathRole:
        return it.localPath().isEmpty() ? it.url().toDisplayString(QUrl::PreferLocalFile) : it.localPath();
    case IsDirRole:
        return it.isDir();
    case SizeRole:
        return double(it.size());
    case MTimeRole:
        return it.time(KFileItem::ModificationTime).toMSecsSinceEpoch();
    case MimeRole:
        return it.currentMimeType().name();
    case MimeCommentRole:
        return it.mimeComment();
    case IconNameRole:
        return it.iconName();
    case HiddenRole:
        return it.isHidden();
    case LinkRole:
        return it.isLink();
    case SecretRole:
        return !it.isDir() && SecretGuard::isSecretName(it.text());
    case KindRole:
        return FileKind::classify(it.text(), it.currentMimeType().name(), it.isDir());
    case ExtRole:
        return FileKind::extension(it.text());
    case GitBranchRole:
        return it.isDir() ? m_gitBranch.value(it.text()) : QString();
    case GitDirtyRole:
        return it.isDir() ? m_gitDirty.value(it.text(), -1) : -1;
    case SelectedRole:
        return m_sel.contains(it.url());
    case LocationRole: {
        if (!searching() || !it.url().isLocalFile())
            return QString();
        const QString dir = QFileInfo(it.localPath()).absolutePath();
        const QString base = m_url.toLocalFile();
        if (dir == base)
            return QStringLiteral(".");
        return dir.startsWith(base + QLatin1Char('/')) ? dir.mid(base.size() + 1) : dir;
    }
    case ThumbRole: {
        const QString kind = FileKind::classify(it.text(), it.currentMimeType().name(), it.isDir());
        if (kind != QLatin1String("image") && kind != QLatin1String("video") && kind != QLatin1String("pdf"))
            return QString();
        return QStringLiteral("image://thumb/") + QString::fromLatin1(QUrl::toPercentEncoding(it.url().toString())) + QLatin1Char('@')
            + QString::number(it.time(KFileItem::ModificationTime).toSecsSinceEpoch());
    }
    }
    return {};
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_view.size())
        return {};
    return roleValue(m_view.at(index.row()), role);
}

KFileItem FolderModel::itemAt(int row) const
{
    return row >= 0 && row < m_view.size() ? KFileItem(m_view.at(row)) : KFileItem();
}

QVariantMap FolderModel::get(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= m_view.size())
        return m;
    const KFileItem &it = m_view.at(row);
    const auto names = roleNames();
    for (auto r = names.cbegin(); r != names.cend(); ++r)
        m.insert(QString::fromLatin1(r.value()), roleValue(it, r.key()));
    m.insert(QStringLiteral("index"), row);
    return m;
}

void FolderModel::ensureRowIndex() const
{
    if (!m_rowsDirty)
        return;
    m_rowCache.clear();
    for (int i = 0; i < m_view.size(); ++i)
        m_rowCache.insert(m_view.at(i).url(), i);
    m_rowsDirty = false;
}

int FolderModel::indexOfUrl(const QString &url) const
{
    ensureRowIndex();
    QUrl u(url);
    if (u.scheme().isEmpty())
        u = QUrl::fromLocalFile(url);
    return m_rowCache.value(u, -1);
}

// ---------------------------------------------------------------- location

QString FolderModel::displayPath() const
{
    if (m_url.isLocalFile()) {
        const QString p = m_url.toLocalFile();
        const QString home = QDir::homePath();
        if (p == home)
            return QStringLiteral("~");
        if (p.startsWith(home + QLatin1Char('/')))
            return QLatin1Char('~') + p.mid(home.size());
        return p;
    }
    return m_url.toDisplayString();
}

QString FolderModel::title() const
{
    if (isTrash())
        return tr("Trash");
    if (m_url.scheme() == QLatin1String("remote"))
        return tr("Network");
    if (m_url.scheme() == QLatin1String("recentlyused"))
        return tr("Recent");
    if (m_url.isLocalFile()) {
        const QString p = m_url.toLocalFile();
        if (p == QDir::homePath())
            return tr("Home");
        if (p == QLatin1String("/"))
            return tr("System");
    }
    const QString name = m_url.fileName();
    return name.isEmpty() ? m_url.toDisplayString() : name;
}

bool FolderModel::canUp() const
{
    return m_url.isValid() && m_url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) != m_url && m_url.path() != QLatin1String("/")
        && !m_url.path().isEmpty();
}

QString FolderModel::parentUrl() const
{
    return KIO::upUrl(m_url).toString();
}

QVariantList FolderModel::crumbs() const
{
    QVariantList out;
    auto add = [&out](const QString &label, const QUrl &u, const QString &icon) {
        out << QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("url"), u.toString()}, {QStringLiteral("icon"), icon}};
    };
    if (!m_url.isLocalFile()) {
        QUrl root = m_url;
        root.setPath(QStringLiteral("/"));
        const QString scheme = m_url.scheme();
        const QString rootLabel = scheme == QLatin1String("trash") ? tr("Trash")
            : scheme == QLatin1String("remote")                    ? tr("Network")
                                                                   : (m_url.host().isEmpty() ? scheme : m_url.host());
        add(rootLabel, root, scheme == QLatin1String("trash") ? QStringLiteral("delete") : QStringLiteral("lan"));
        QString acc;
        for (const QString &part : m_url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
            acc += QLatin1Char('/') + part;
            QUrl u = m_url;
            u.setPath(acc);
            add(part, u, QString());
        }
        return out;
    }
    const QString p = m_url.toLocalFile();
    const QString home = QDir::homePath();
    QString acc, rest;
    if (p == home || p.startsWith(home + QLatin1Char('/'))) {
        add(tr("Home"), QUrl::fromLocalFile(home), QStringLiteral("home"));
        acc = home;
        rest = p.mid(home.size());
    } else {
        add(QStringLiteral("/"), QUrl::fromLocalFile(QStringLiteral("/")), QStringLiteral("hard_drive"));
        rest = p;
    }
    for (const QString &part : rest.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        acc += QLatin1Char('/') + part;
        add(part, QUrl::fromLocalFile(acc), QString());
    }
    return out;
}

void FolderModel::navigate(const QString &input)
{
    QString s = input.trimmed();
    if (s.isEmpty())
        return;
    s = Config::expand(s);
    QUrl u = QUrl::fromUserInput(s, QDir::homePath(), QUrl::AssumeLocalFile);
    if (!u.isValid())
        return;
    if (u.isLocalFile()) {
        QString lp = QDir::cleanPath(u.toLocalFile());
        QFileInfo fi(lp);
        if (fi.exists() && !fi.isDir()) { // a file: open its folder and select it
            open(QUrl::fromLocalFile(fi.absolutePath()), true);
            selectUrl(QUrl::fromLocalFile(lp).toString());
            return;
        }
        u = QUrl::fromLocalFile(lp);
    }
    u = u.adjusted(QUrl::StripTrailingSlash);
    if (u.path().isEmpty())
        u.setPath(QStringLiteral("/"));
    if (u == m_url && !searching())
        return;
    open(u, true);
}

void FolderModel::open(const QUrl &url, bool pushHistory)
{
    if (pushHistory) {
        m_history = m_history.mid(0, m_histIdx + 1);
        m_history.append(url);
        m_histIdx = int(m_history.size()) - 1;
    }
    m_url = url;
    m_filter.clear();
    m_query.clear();
    m_results.clear();
    if (m_search)
        m_search->kill();
    m_searchBusy = false;
    m_sel.clear();
    m_anchor = -1;
    m_current = -1;
    m_gitBranch.clear();
    m_gitDirty.clear();
    m_dirtyQueue.clear();
    m_error.clear();
    m_source.clear();
    rebuild();
    m_listTimer.start();
    Q_EMIT filterChanged();
    Q_EMIT searchChanged();
    Q_EMIT selectionChanged();
    Q_EMIT currentIndexChanged();
    Q_EMIT locationChanged();
    m_lister->openUrl(url);
    updateRepo();
}

void FolderModel::back()
{
    if (!canBack())
        return;
    const QUrl from = m_url;
    --m_histIdx;
    open(m_history.at(m_histIdx), false);
    if (from.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash) == m_url)
        selectUrl(from.toString());
}

void FolderModel::forward()
{
    if (!canForward())
        return;
    ++m_histIdx;
    open(m_history.at(m_histIdx), false);
}

void FolderModel::up()
{
    if (!canUp())
        return;
    const QUrl child = m_url;
    open(KIO::upUrl(m_url).adjusted(QUrl::StripTrailingSlash), true);
    selectUrl(child.toString());
}

void FolderModel::refresh()
{
    if (searching()) {
        runSearch();
        return;
    }
    m_listTimer.start();
    m_lister->updateDirectory(m_url);
    updateRepo();
}

// ---------------------------------------------------------------- listing

bool FolderModel::accepts(const KFileItem &it) const
{
    if (!m_showHidden && it.isHidden())
        return false;
    if (m_dirsOnly && !it.isDir())
        return false;
    if (!m_filter.isEmpty() && !it.text().contains(m_filter, Qt::CaseInsensitive))
        return false;
    if ((!m_nameRx.isEmpty() || !m_mimeRx.isEmpty()) && !it.isDir()) {
        const QString name = it.name();
        if (std::ranges::any_of(m_nameRx, [&](const QRegularExpression &rx) { return rx.match(name).hasMatch(); }))
            return true;
        if (m_mimeRx.isEmpty())
            return false;
        // By name only: sniffing contents for every file would stall large folders.
        static const QMimeDatabase db;
        const QMimeType mime = db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
        QStringList names = mime.allAncestors();
        names.prepend(mime.name());
        return std::ranges::any_of(m_mimeRx, [&](const QRegularExpression &rx) {
            return std::ranges::any_of(names, [&](const QString &n) { return rx.match(n).hasMatch(); });
        });
    }
    return true;
}

bool FolderModel::lessThan(const FolderEntry &a, const FolderEntry &b) const
{
    if (a.isDir() != b.isDir())
        return a.isDir();
    int c = 0;
    if (m_sortField == QLatin1String("time")) {
        // time() builds a QDateTime per call, which made sorting by date ~40x slower than reading the UDS field.
        const auto ta = a.entry().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME), tb = b.entry().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME);
        c = ta < tb ? 1 : ta > tb ? -1 : 0; // newest first
    } else if (m_sortField == QLatin1String("size")) {
        c = a.size() < b.size() ? 1 : a.size() > b.size() ? -1 : 0; // biggest first
    } else if (m_sortField == QLatin1String("type")) {
        c = QString::compare(FileKind::extension(a.text()), FileKind::extension(b.text()), Qt::CaseInsensitive);
    }
    if (c == 0)
        c = a.key.compare(b.key);
    if (m_sortReversed)
        c = -c;
    return c < 0;
}

void FolderModel::rebuild()
{
    QElapsedTimer t;
    t.start();
    beginResetModel();
    m_view.clear();
    const QList<FolderEntry> &src = searching() ? m_results : m_source;
    for (const FolderEntry &it : src)
        if (searching() || accepts(it))
            m_view.append(it);
    const qint64 sortStart = t.nsecsElapsed();
    if (!searching())
        std::stable_sort(m_view.begin(), m_view.end(), [this](const FolderEntry &a, const FolderEntry &b) {
            return lessThan(a, b);
        });
    const qint64 sortNs = t.nsecsElapsed() - sortStart;
    m_merged = m_source.size();
    m_rowsDirty = true;
    endResetModel();
    qCDebug(lcFolderTiming).nospace() << "rebuild view=" << m_view.size() << " sort=" << sortNs / 1000 << "us total="
                                      << t.nsecsElapsed() / 1000 << "us";
    if (m_current >= m_view.size()) {
        m_current = int(m_view.size()) - 1;
        Q_EMIT currentIndexChanged();
    }
    Q_EMIT countChanged();
}

void FolderModel::onNewItems(const KFileItemList &items)
{
    for (const KFileItem &it : items)
        m_source.append(FolderEntry(it, m_collator));
    // Each view update costs QML work and a frame, so while listing merge KIO's ~200-item batches in bigger chunks.
    if (m_loading && !m_view.isEmpty() && !searching()) {
        if (!m_mergeTimer.isActive())
            m_mergeTimer.start();
        return;
    }
    mergePending();
}

void FolderModel::mergePending()
{
    m_mergeTimer.stop();
    if (searching())
        return;
    QElapsedTimer t;
    t.start();
    // KCoreDirLister never re-sends listed items (changes come via refreshItems), so no dedupe.
    QList<FolderEntry> batch;
    for (auto it = m_source.cbegin() + m_merged; it != m_source.cend(); ++it)
        if (accepts(*it))
            batch.append(*it);
    m_merged = m_source.size();
    if (batch.isEmpty())
        return;
    const int oldCurrent = m_current;
    insertSorted(batch);
    qCDebug(lcFolderTiming).nospace() << "insert batch=" << batch.size() << " view=" << m_view.size()
                                      << " total=" << t.nsecsElapsed() / 1000 << "us";
    if (m_current != oldCurrent)
        Q_EMIT currentIndexChanged();
    Q_EMIT countChanged();
    // applyPendingSelect() rebuilds the row index, so only pay for it when this batch can resolve it.
    if (!m_pendingSelect.isEmpty()
        && std::any_of(batch.cbegin(), batch.cend(), [this](const KFileItem &it) { return it.url() == m_pendingSelect; }))
        applyPendingSelect();
}

void FolderModel::insertSorted(QList<FolderEntry> &batch)
{
    m_rowsDirty = true;
    const auto less = [this](const FolderEntry &a, const FolderEntry &b) { return lessThan(a, b); };
    std::stable_sort(batch.begin(), batch.end(), less);

    // upper_bound keeps new items after existing equal ones, matching a stable sort of m_source.
    QList<qsizetype> pos(batch.size());
    qsizetype runs = 0;
    for (qsizetype i = 0, from = 0; i < batch.size(); ++i) {
        if (i > 0 && (from == m_view.size() || lessThan(batch.at(i), m_view.at(from)))) {
            pos[i] = from;
            continue;
        }
        pos[i] = std::upper_bound(m_view.cbegin() + from, m_view.cend(), batch.at(i), less) - m_view.cbegin();
        if (i == 0 || pos[i] != pos[i - 1])
            ++runs;
        from = pos[i];
    }

    // A big insert above existing rows displaces every delegate and QML rebuilds a screenful; a merge keeps them.
    const bool append = pos.front() == m_view.size();
    if (append || (runs <= kMaxInsertRuns && batch.size() <= kMaxInsertRows)) {
        for (qsizetype i = 0, shift = 0; i < batch.size();) {
            qsizetype j = i + 1;
            while (j < batch.size() && pos[j] == pos[i])
                ++j;
            const qsizetype at = pos[i] + shift, len = j - i;
            beginInsertRows({}, int(at), int(at + len - 1));
            m_view.insert(at, len, batch.at(i));
            std::copy(batch.cbegin() + i, batch.cbegin() + j, m_view.begin() + at);
            endInsertRows();
            if (m_current >= at)
                m_current += int(len);
            if (m_anchor >= at)
                m_anchor += int(len);
            shift += len;
            i = j;
        }
    } else {
        // One insert per run costs an O(n) memmove and a view update each, so append and merge once instead.
        const qsizetype n = m_view.size(), k = batch.size();
        beginInsertRows({}, int(n), int(n + k - 1));
        m_view.append(batch);
        endInsertRows();
        Q_EMIT layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
        for (qsizetype i = k - 1, end = n; i >= 0; --i) {
            std::move_backward(m_view.begin() + pos[i], m_view.begin() + end, m_view.begin() + end + i + 1);
            m_view[pos[i] + i] = batch.at(i);
            end = pos[i];
        }
        const auto remap = [&](int r) {
            if (r < 0)
                return r;
            if (r >= n)
                return int(pos[r - n] + r - n);
            return r + int(std::upper_bound(pos.cbegin(), pos.cend(), r) - pos.cbegin());
        };
        const QModelIndexList from = persistentIndexList();
        QModelIndexList to;
        to.reserve(from.size());
        for (const QModelIndex &idx : from)
            to.append(index(remap(idx.row())));
        changePersistentIndexList(from, to);
        m_current = remap(m_current);
        m_anchor = remap(m_anchor);
        Q_EMIT layoutChanged({}, QAbstractItemModel::VerticalSortHint);
    }
}

// order[i] is the current row that must end up at row i.
void FolderModel::reorder(const QList<int> &order)
{
    Q_EMIT layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    QList<int> to(order.size());
    QList<FolderEntry> view;
    view.reserve(order.size());
    for (int i = 0; i < order.size(); ++i) {
        to[order.at(i)] = i;
        view.append(std::move(m_view[order.at(i)]));
    }
    m_view = std::move(view);
    const QModelIndexList from = persistentIndexList();
    QModelIndexList moved;
    moved.reserve(from.size());
    for (const QModelIndex &idx : from)
        moved.append(index(to.at(idx.row())));
    changePersistentIndexList(from, moved);
    const auto remap = [&](int r) { return r >= 0 && r < to.size() ? to.at(r) : r; };
    m_current = remap(m_current);
    m_anchor = remap(m_anchor);
    m_rowsDirty = true;
    Q_EMIT layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

// rows must be sorted and unique. A removed current/anchor row goes to the item that slides into its place.
void FolderModel::removeViewRows(const QList<int> &rows)
{
    if (rows.isEmpty())
        return;
    const auto survivorsBefore = [&](int r) { return r < 0 ? r : r - int(std::lower_bound(rows.cbegin(), rows.cend(), r) - rows.cbegin()); };
    const int current = survivorsBefore(m_current), anchor = survivorsBefore(m_anchor);
    qsizetype ranges = 1;
    for (qsizetype i = 1; i < rows.size(); ++i)
        if (rows.at(i) != rows.at(i - 1) + 1)
            ++ranges;
    if (ranges <= kMaxInsertRuns) {
        for (qsizetype j = rows.size(); j > 0;) {
            qsizetype i = j - 1;
            while (i > 0 && rows.at(i - 1) == rows.at(i) - 1)
                --i;
            beginRemoveRows({}, rows.at(i), rows.at(j - 1));
            m_view.remove(rows.at(i), j - i);
            endRemoveRows();
            j = i;
        }
    } else {
        // Each scattered range would memmove the tail, so gather the removed rows at the end and drop them at once.
        QList<int> order;
        order.reserve(m_view.size());
        for (int r = 0, j = 0; r < m_view.size(); ++r) {
            if (j < rows.size() && rows.at(j) == r)
                ++j;
            else
                order.append(r);
        }
        order.append(rows);
        reorder(order);
        const int n = int(m_view.size()), k = int(rows.size());
        beginRemoveRows({}, n - k, n - 1);
        m_view.remove(n - k, k);
        endRemoveRows();
    }
    m_current = qMin(current, int(m_view.size()) - 1);
    m_anchor = qMin(anchor, int(m_view.size()) - 1);
    m_rowsDirty = true;
}

// rows hold the items whose sort key may have changed; the rest of m_view must be sorted.
void FolderModel::resortRows(QList<int> rows)
{
    const auto less = [this](int a, int b) { return lessThan(m_view.at(a), m_view.at(b)); };
    const int n = int(m_view.size());
    if (std::all_of(rows.cbegin(), rows.cend(), [&](int r) { return (r == 0 || !less(r, r - 1)) && (r + 1 == n || !less(r + 1, r)); }))
        return;
    QList<bool> changed(n, false);
    for (int r : std::as_const(rows))
        changed[r] = true;
    QList<int> rest;
    rest.reserve(n - rows.size());
    for (int r = 0; r < n; ++r)
        if (!changed.at(r))
            rest.append(r);
    std::stable_sort(rows.begin(), rows.end(), less);
    QList<int> order;
    order.reserve(n);
    std::merge(rest.cbegin(), rest.cend(), rows.cbegin(), rows.cend(), std::back_inserter(order), less);
    if (rows.size() > kMaxInsertRuns) {
        reorder(order);
        return;
    }
    // Placing each changed item right after its final predecessor, in final order, converges on order; single moves animate.
    QList<int> cur(n);
    std::iota(cur.begin(), cur.end(), 0);
    for (int f = 0; f < n; ++f) {
        if (!changed.at(order.at(f)))
            continue;
        const int from = int(cur.indexOf(order.at(f)));
        const int dest = f == 0 ? 0 : int(cur.indexOf(order.at(f - 1))) + 1;
        if (dest == from)
            continue;
        const int to = dest > from ? dest - 1 : dest;
        const auto remap = [&](int r) {
            if (r == from)
                return to;
            if (to < from && r >= to && r < from)
                return r + 1;
            if (from < to && r > from && r <= to)
                return r - 1;
            return r;
        };
        beginMoveRows({}, from, from, {}, dest);
        m_view.move(from, to);
        cur.move(from, to);
        endMoveRows();
        m_current = remap(m_current);
        m_anchor = remap(m_anchor);
    }
    m_rowsDirty = true;
}

void FolderModel::onDeleted(const KFileItemList &items)
{
    mergePending();
    QSet<QUrl> gone;
    gone.reserve(items.size());
    bool selChanged = false;
    for (const KFileItem &it : items) {
        gone.insert(it.url());
        if (m_sel.remove(it.url()))
            selChanged = true;
    }
    m_source.removeIf([&](const KFileItem &x) { return gone.contains(x.url()); });
    m_merged = qMin(m_merged, m_source.size());
    const int oldCurrent = m_current;
    if (!searching()) {
        ensureRowIndex();
        QList<int> rows;
        for (const QUrl &u : std::as_const(gone))
            if (const int row = m_rowCache.value(u, -1); row >= 0)
                rows.append(row);
        std::sort(rows.begin(), rows.end());
        removeViewRows(rows);
    }
    if (m_current >= m_view.size())
        m_current = int(m_view.size()) - 1;
    if (m_current != oldCurrent)
        Q_EMIT currentIndexChanged();
    Q_EMIT countChanged();
    if (selChanged)
        Q_EMIT selectionChanged();
}

void FolderModel::onRefresh(const QList<QPair<KFileItem, KFileItem>> &items)
{
    QHash<QUrl, qsizetype> pending;
    pending.reserve(items.size());
    for (qsizetype i = 0; i < items.size(); ++i)
        pending.insert(items.at(i).first.url(), i);
    QList<qsizetype> src(items.size(), -1);
    for (qsizetype s = 0, found = 0; s < m_source.size() && found < pending.size(); ++s) {
        const auto it = pending.constFind(m_source.at(s).url());
        if (it == pending.cend())
            continue;
        src[*it] = s;
        m_source[s] = FolderEntry(items.at(*it).second, m_collator);
        ++found;
    }
    if (searching())
        return;

    ensureRowIndex();
    const int oldCurrent = m_current;
    bool renamed = false, selChanged = false;
    QList<int> changed, removed;
    QList<FolderEntry> added;
    for (qsizetype i = 0; i < items.size(); ++i) {
        // Items past m_merged are not in m_view yet; mergePending() takes their new state.
        if (src.at(i) < 0 || src.at(i) >= m_merged)
            continue;
        const QUrl &before = items.at(i).first.url(), &after = items.at(i).second.url();
        const FolderEntry &e = m_source.at(src.at(i));
        const int row = m_rowCache.value(before, -1);
        const bool keep = accepts(e);
        if (row >= 0 && keep) {
            m_view[row] = e;
            changed.append(row);
        } else if (row >= 0) {
            removed.append(row);
        } else if (keep) {
            added.append(e);
        }
        renamed |= before != after;
        if (m_sel.remove(before)) {
            if (row >= 0 && !keep)
                selChanged = true;
            else
                m_sel.insert(after);
        }
    }
    if (changed.size() > kMaxInsertRuns) {
        const auto [lo, hi] = std::minmax_element(changed.cbegin(), changed.cend());
        Q_EMIT dataChanged(index(*lo), index(*hi));
    } else {
        for (int row : std::as_const(changed))
            Q_EMIT dataChanged(index(row), index(row));
    }
    std::sort(removed.begin(), removed.end());
    removeViewRows(removed);
    for (int &row : changed)
        row -= int(std::lower_bound(removed.cbegin(), removed.cend(), row) - removed.cbegin());
    // insertSorted() binary-searches m_view, so rows whose sort key changed must move.
    resortRows(changed);
    if (!added.isEmpty())
        insertSorted(added);
    if (renamed)
        m_rowsDirty = true;
    if (m_current != oldCurrent)
        Q_EMIT currentIndexChanged();
    if (!removed.isEmpty() || !added.isEmpty())
        Q_EMIT countChanged();
    if (selChanged)
        Q_EMIT selectionChanged();
}

void FolderModel::onCompleted()
{
    mergePending();
    m_loading = false;
    Q_EMIT loadingChanged();
    qCDebug(lcFolderTiming).nospace() << "listing complete view=" << m_view.size()
                                      << " elapsed=" << m_listTimer.elapsed() << "ms";
    applyPendingSelect();
    scanGit();
}

void FolderModel::applyPendingSelect()
{
    if (m_pendingSelect.isEmpty())
        return;
    const int row = indexOfUrl(m_pendingSelect.toString());
    if (row < 0)
        return;
    m_pendingSelect.clear();
    selectOnly(row);
    Q_EMIT revealRow(row);
}

void FolderModel::setFilter(const QString &f)
{
    if (f == m_filter)
        return;
    m_filter = f;
    Q_EMIT filterChanged();
    rebuild();
}

void FolderModel::setShowHidden(bool v)
{
    if (v == m_showHidden)
        return;
    m_showHidden = v;
    Q_EMIT optionsChanged();
    rebuild();
    if (searching())
        m_searchDebounce.start();
}

void FolderModel::setSortField(const QString &f)
{
    if (f == m_sortField)
        return;
    m_sortField = f;
    Q_EMIT optionsChanged();
    rebuild();
}

void FolderModel::setSortReversed(bool v)
{
    if (v == m_sortReversed)
        return;
    m_sortReversed = v;
    Q_EMIT optionsChanged();
    rebuild();
}

void FolderModel::setDirsOnly(bool v)
{
    if (v == m_dirsOnly)
        return;
    m_dirsOnly = v;
    Q_EMIT optionsChanged();
    rebuild();
}

void FolderModel::setTypeFilter(const QStringList &patterns)
{
    if (patterns == m_typeFilter)
        return;
    m_typeFilter = patterns;
    m_nameRx.clear();
    m_mimeRx.clear();
    for (const QString &p : patterns) {
        (p.contains(u'/') ? m_mimeRx : m_nameRx) << QRegularExpression::fromWildcard(p, Qt::CaseInsensitive);
    }
    Q_EMIT optionsChanged();
    rebuild();
}

// ---------------------------------------------------------------- search (fd, fallback QDirIterator)

void FolderModel::setSearchQuery(const QString &q)
{
    if (q == m_query)
        return;
    const bool was = searching();
    m_query = q;
    if (!searching()) {
        if (m_search)
            m_search->kill();
        m_results.clear();
        m_searchBusy = false;
        Q_EMIT searchChanged();
        if (was)
            rebuild();
        return;
    }
    Q_EMIT searchChanged();
    m_searchDebounce.start();
}

void FolderModel::runSearch()
{
    if (!searching() || !m_url.isLocalFile())
        return;
    if (m_search) {
        m_search->disconnect(this);
        m_search->kill();
        m_search->deleteLater();
    }
    m_results.clear();
    m_searchBusy = true;
    Q_EMIT searchChanged();
    rebuild();

    const QString root = m_url.toLocalFile();
    const QString query = m_query.trimmed();
    const QString fd = QStandardPaths::findExecutable(QStringLiteral("fd"));
    if (fd.isEmpty()) {
        // Fallback: plain recursive walk off the GUI thread.
        const bool hidden = m_showHidden;
        auto *watcher = new QFutureWatcher<QStringList>(this);
        connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, query] {
            watcher->deleteLater();
            if (query != m_query.trimmed())
                return;
            for (const QString &p : watcher->result())
                m_results.append(FolderEntry(itemForLocalPath(p), m_collator));
            m_searchBusy = false;
            Q_EMIT searchChanged();
            rebuild();
        });
        watcher->setFuture(QtConcurrent::run([root, query, hidden] {
            QStringList out;
            QDir::Filters f = QDir::AllEntries | QDir::NoDotAndDotDot;
            if (hidden)
                f |= QDir::Hidden;
            QDirIterator it(root, f, QDirIterator::Subdirectories);
            while (it.hasNext() && out.size() < kMaxResults) {
                it.next();
                if (it.fileName().contains(query, Qt::CaseInsensitive))
                    out << it.filePath();
            }
            return out;
        }));
        return;
    }
    auto *p = new QProcess(this);
    m_search = p;
    QStringList args{QStringLiteral("--absolute-path"), QStringLiteral("--color"), QStringLiteral("never"), QStringLiteral("--ignore-case"),
                     QStringLiteral("--fixed-strings"), QStringLiteral("--max-results"), QString::number(kMaxResults)};
    if (m_showHidden)
        args << QStringLiteral("--hidden") << QStringLiteral("--exclude") << QStringLiteral(".git");
    args << QStringLiteral("--") << query << root;
    connect(p, &QProcess::readyReadStandardOutput, this, [this, p] {
        QList<FolderEntry> batch;
        while (p->canReadLine()) {
            QString line = QString::fromUtf8(p->readLine()).trimmed();
            if (line.endsWith(QLatin1Char('/')) && line.size() > 1)
                line.chop(1);
            if (!line.isEmpty())
                batch.append(FolderEntry(itemForLocalPath(line), m_collator));
        }
        if (batch.isEmpty())
            return;
        const int first = int(m_view.size());
        beginInsertRows({}, first, first + int(batch.size()) - 1);
        m_results.append(batch);
        m_view.append(batch);
        endInsertRows();
        m_rowsDirty = true;
        Q_EMIT countChanged();
    });
    connect(p, &QProcess::finished, this, [this, p] {
        if (m_search == p) {
            m_searchBusy = false;
            Q_EMIT searchChanged();
        }
        p->deleteLater();
    });
    p->start(fd, args);
}

// ---------------------------------------------------------------- selection

void FolderModel::emitSelectionRoles()
{
    if (!m_view.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_view.size()) - 1), {SelectedRole});
    Q_EMIT selectionChanged();
}

void FolderModel::setCurrentIndex(int i)
{
    i = qBound(-1, i, int(m_view.size()) - 1);
    if (i == m_current)
        return;
    m_current = i;
    Q_EMIT currentIndexChanged();
}

void FolderModel::selectOnly(int row)
{
    m_sel.clear();
    if (row >= 0 && row < m_view.size())
        m_sel.insert(m_view.at(row).url());
    m_anchor = row;
    setCurrentIndex(row);
    emitSelectionRoles();
}

void FolderModel::toggle(int row)
{
    if (row < 0 || row >= m_view.size())
        return;
    const QUrl u = m_view.at(row).url();
    if (!m_sel.remove(u))
        m_sel.insert(u);
    m_anchor = row;
    setCurrentIndex(row);
    emitSelectionRoles();
}

void FolderModel::selectRange(int from, int to)
{
    if (from < 0)
        from = m_anchor >= 0 ? m_anchor : to;
    m_sel.clear();
    for (int i = qMax(0, qMin(from, to)); i <= qMin(int(m_view.size()) - 1, qMax(from, to)); ++i)
        m_sel.insert(m_view.at(i).url());
    setCurrentIndex(to);
    emitSelectionRoles();
}

void FolderModel::selectAll()
{
    for (const KFileItem &it : std::as_const(m_view))
        m_sel.insert(it.url());
    emitSelectionRoles();
}

void FolderModel::setSelection(const QList<int> &rows, bool add)
{
    if (!add)
        m_sel.clear();
    for (int r : rows)
        if (r >= 0 && r < m_view.size())
            m_sel.insert(m_view.at(r).url());
    emitSelectionRoles();
}

void FolderModel::clearSelection()
{
    if (m_sel.isEmpty())
        return;
    m_sel.clear();
    m_anchor = -1;
    emitSelectionRoles();
}

void FolderModel::selectUrl(const QString &url)
{
    QUrl u(url);
    if (u.scheme().isEmpty())
        u = QUrl::fromLocalFile(url);
    m_pendingSelect = u.adjusted(QUrl::StripTrailingSlash);
    applyPendingSelect();
}

bool FolderModel::isSelected(int row) const
{
    return row >= 0 && row < m_view.size() && m_sel.contains(m_view.at(row).url());
}

QList<QUrl> FolderModel::selectedUrlList() const
{
    QList<QUrl> out;
    for (const KFileItem &it : m_view)
        if (m_sel.contains(it.url()))
            out << it.url();
    return out;
}

QStringList FolderModel::selectedUrls() const
{
    QStringList out;
    for (const QUrl &u : selectedUrlList())
        out << u.toString();
    return out;
}

QStringList FolderModel::selectedPaths() const
{
    QStringList out;
    for (const QUrl &u : selectedUrlList())
        out << (u.isLocalFile() ? u.toLocalFile() : u.toDisplayString());
    return out;
}

QVariantList FolderModel::selectedItems() const
{
    QVariantList out;
    for (int i = 0; i < m_view.size(); ++i)
        if (m_sel.contains(m_view.at(i).url()))
            out << get(i);
    return out;
}

qint64 FolderModel::selectionSize() const
{
    qint64 total = 0;
    for (const KFileItem &it : m_view)
        if (!it.isDir() && m_sel.contains(it.url()))
            total += qint64(it.size());
    return total;
}

// ---------------------------------------------------------------- git

void FolderModel::scanGit()
{
    if (!m_url.isLocalFile() || searching())
        return;
    const QString base = m_url.toLocalFile();
    m_dirtyQueue.clear();
    QList<int> changed;
    for (int i = 0; i < m_view.size(); ++i) {
        const KFileItem &it = m_view.at(i);
        if (!it.isDir())
            continue;
        const QString branch = readGitHead(base + QLatin1Char('/') + it.text());
        if (branch.isEmpty())
            continue;
        m_gitBranch.insert(it.text(), branch);
        if (m_dirtyQueue.size() < kMaxDirtyChecks)
            m_dirtyQueue << it.text();
        changed << i;
    }
    for (int row : std::as_const(changed))
        Q_EMIT dataChanged(index(row), index(row), {GitBranchRole});
    if (!QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
        nextDirtyCheck();
}

void FolderModel::nextDirtyCheck()
{
    if (m_dirtyProc || m_dirtyQueue.isEmpty())
        return;
    const QString name = m_dirtyQueue.takeFirst();
    const QString dir = m_url.toLocalFile() + QLatin1Char('/') + name;
    auto *p = new QProcess(this);
    m_dirtyProc = p;
    const QUrl forUrl = m_url;
    connect(p, &QProcess::finished, this, [this, p, name, forUrl](int code) {
        p->deleteLater();
        m_dirtyProc = nullptr;
        if (forUrl != m_url)
            return;
        if (code == 0) {
            const int n = int(p->readAllStandardOutput().count('\n'));
            m_gitDirty.insert(name, n);
            const int row = indexOfUrl(QUrl::fromLocalFile(m_url.toLocalFile() + QLatin1Char('/') + name).toString());
            if (row >= 0)
                Q_EMIT dataChanged(index(row), index(row), {GitDirtyRole});
        }
        nextDirtyCheck();
    });
    p->start(QStringLiteral("git"), {QStringLiteral("--no-optional-locks"), QStringLiteral("-C"), dir, QStringLiteral("status"), QStringLiteral("--porcelain")});
}

void FolderModel::updateRepo()
{
    m_repoRoot.clear();
    m_repoBranch.clear();
    m_repoDirty = -1;
    Q_EMIT repoChanged();
    if (!m_url.isLocalFile() || QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty())
        return;
    if (m_repoProc) {
        m_repoProc->disconnect(this);
        m_repoProc->kill();
        m_repoProc->deleteLater();
    }
    auto *p = new QProcess(this);
    m_repoProc = p;
    const QUrl forUrl = m_url;
    // one call: toplevel, branch, then the porcelain status (NUL-free, line based)
    connect(p, &QProcess::finished, this, [this, p, forUrl](int code) {
        p->deleteLater();
        if (m_repoProc == p)
            m_repoProc = nullptr;
        if (code != 0 || forUrl != m_url)
            return;
        const QStringList lines = QString::fromUtf8(p->readAllStandardOutput()).split(QLatin1Char('\n'));
        if (lines.size() < 2)
            return;
        m_repoRoot = lines.at(0).trimmed();
        m_repoBranch = lines.at(1).trimmed();
        int dirty = 0;
        for (int i = 2; i < lines.size(); ++i)
            if (!lines.at(i).trimmed().isEmpty())
                ++dirty;
        m_repoDirty = dirty;
        Q_EMIT repoChanged();
    });
    p->start(QStringLiteral("sh"),
             {QStringLiteral("-c"),
              QStringLiteral("cd \"$1\" && git rev-parse --show-toplevel && (git symbolic-ref --short -q HEAD || git rev-parse --short HEAD) && "
                             "git --no-optional-locks status --porcelain"),
              QStringLiteral("sh"), m_url.toLocalFile()});
}
