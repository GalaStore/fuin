// SPDX-License-Identifier: MIT
#include "DefaultAppsModel.h"
#include "Config.h"

#include <KApplicationTrader>
#include <KService>
#include <KSycoca>

#include <QFileInfo>
#include <QIcon>
#include <QMimeDatabase>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace {
// Case- and accent-insensitive form used for searching ("vídeo" matches "video").
QString fold(const QString &text)
{
    QString out;
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    out.reserve(decomposed.size());
    for (const QChar c : decomposed)
        if (c.category() != QChar::Mark_NonSpacing)
            out += c.toLower();
    return out;
}

// Search key: "*.py", ".py" and "py" all mean the extension.
QString normalizedFilter(const QString &f)
{
    QString s = fold(f.trimmed());
    if (s.startsWith(QLatin1String("*.")))
        s = s.mid(2);
    else if (s.startsWith(QLatin1Char('.')))
        s = s.mid(1);
    return s;
}
}

DefaultAppsModel::DefaultAppsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DefaultAppsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> DefaultAppsModel::roleNames() const
{
    return {{MimeTypeRole, "mimeType"},       {CommentRole, "comment"},         {IconNameRole, "iconName"},
            {PatternsRole, "patterns"},       {SectionRole, "section"},         {SectionStartRole, "sectionStart"},
            {DefaultIdRole, "defaultId"},
            {DefaultNameRole, "defaultName"}, {DefaultIconRole, "defaultIcon"}, {DefaultTerminalRole, "defaultTerminal"},
            {AppsRole, "apps"}};
}

QVariant DefaultAppsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Entry &e = entryAt(index.row());
    switch (role) {
    case MimeTypeRole:
        return e.mimeType;
    case CommentRole:
        return e.comment;
    case IconNameRole:
        return e.iconName;
    case PatternsRole:
        return e.patterns.join(QLatin1String("  "));
    case SectionRole:
        return e.section;
    case SectionStartRole:
        return index.row() == 0 || entryAt(index.row() - 1).section != e.section;
    case DefaultIdRole:
        return e.defaultId;
    case DefaultNameRole:
        return e.defaultName;
    case DefaultIconRole:
        return e.defaultIcon;
    case DefaultTerminalRole:
        return e.defaultTerminal;
    case AppsRole:
        return e.apps;
    default:
        return {};
    }
}

const DefaultAppsModel::Entry &DefaultAppsModel::entryAt(int row) const
{
    const int r = m_rows.at(row);
    return r >= 0 ? m_all.at(r) : m_extra.at(-1 - r);
}

DefaultAppsModel::Entry DefaultAppsModel::makeEntry(const QString &mimeType, const QString &section) const
{
    const QMimeType mt = QMimeDatabase().mimeTypeForName(mimeType);
    Entry e;
    e.mimeType = mt.name();
    e.comment = mt.comment().isEmpty() ? mt.name() : mt.comment();
    e.iconName = QIcon::hasThemeIcon(mt.iconName()) ? mt.iconName() : mt.genericIconName();
    e.patterns = mt.globPatterns();
    e.section = section;
    resolveApps(e);
    return e;
}

void DefaultAppsModel::resolveApps(Entry &e) const
{
    const KService::Ptr preferred = KApplicationTrader::preferredService(e.mimeType);
    KService::List apps = KApplicationTrader::queryByMimeType(e.mimeType);
    if (preferred && std::none_of(apps.cbegin(), apps.cend(), [&](const KService::Ptr &s) { return s->storageId() == preferred->storageId(); }))
        apps.prepend(preferred);
    e.defaultId = preferred ? preferred->storageId() : QString();
    e.defaultName = preferred ? preferred->name() : QString();
    e.defaultIcon = preferred ? preferred->icon() : QString();
    e.defaultTerminal = preferred && preferred->terminal();
    e.apps.clear();
    for (const KService::Ptr &s : std::as_const(apps)) {
        e.apps << QVariantMap{{QStringLiteral("id"), s->storageId()},
                              {QStringLiteral("name"), s->name()},
                              {QStringLiteral("genericName"), s->genericName()},
                              {QStringLiteral("icon"), s->icon()},
                              {QStringLiteral("terminal"), s->terminal()},
                              {QStringLiteral("isDefault"), s->storageId() == e.defaultId}};
    }
}

void DefaultAppsModel::reload()
{
    KSycoca::self()->ensureCacheValid();
    const QMimeDatabase db;
    QList<Entry> all;
    QSet<QString> seen;
    auto add = [&](const QMimeType &mt, const QString &section) {
        if (!mt.isValid() || mt.isDefault() || seen.contains(mt.name()))
            return;
        seen.insert(mt.name());
        all << makeEntry(mt.name(), section);
    };

    for (const QString &m : std::as_const(m_pinned))
        add(db.mimeTypeForName(m), tr("Selected type"));
    for (const QString &m : Config::instance()->openedMimeTypes())
        add(db.mimeTypeForName(m), tr("Recently opened"));

    const QString common = tr("Common");
    // Everyday types, resolved through the MIME database (aliases map to the canonical name).
    for (const char *name : {"text/plain", "text/markdown", "text/x-python", "text/csv", "application/json", "application/x-shellscript", "application/pdf",
                             "inode/directory"})
        add(db.mimeTypeForName(QString::fromLatin1(name)), common);
    // Data and model formats Fūin previews, only if the system's MIME database knows them.
    for (const char *suffix : {"parquet", "safetensors", "gguf"}) {
        const QMimeType mt = db.mimeTypeForFile(QStringLiteral("x.") + QString::fromLatin1(suffix), QMimeDatabase::MatchExtension);
        add(mt, common);
    }

    // Media families: every type with file patterns that an installed application declares.
    const QList<QPair<QString, QString>> families{{QStringLiteral("image/"), tr("Images")},
                                                  {QStringLiteral("video/"), tr("Video")},
                                                  {QStringLiteral("audio/"), tr("Audio")}};
    const QList<QMimeType> everything = db.allMimeTypes();
    for (const auto &[prefix, section] : families) {
        QList<Entry> group;
        for (const QMimeType &mt : everything) {
            if (!mt.name().startsWith(prefix) || mt.globPatterns().isEmpty() || seen.contains(mt.name()))
                continue;
            const KService::List apps = KApplicationTrader::queryByMimeType(mt.name());
            const bool declared = std::any_of(apps.cbegin(), apps.cend(), [&](const KService::Ptr &s) { return s->mimeTypes().contains(mt.name()); });
            if (!declared)
                continue;
            seen.insert(mt.name());
            group << makeEntry(mt.name(), section);
        }
        std::sort(group.begin(), group.end(), [](const Entry &a, const Entry &b) { return a.comment.localeAwareCompare(b.comment) < 0; });
        all << group;
    }

    beginResetModel();
    m_all = all;
    m_loaded = true;
    endResetModel();
    rebuildRows();
}

void DefaultAppsModel::setFilter(const QString &f)
{
    if (f == m_filter)
        return;
    m_filter = f;
    Q_EMIT filterChanged();
    rebuildRows();
}

void DefaultAppsModel::rebuildRows()
{
    const QString f = normalizedFilter(m_filter);
    const QString ext = QStringLiteral("*.") + f;
    auto matches = [&](const Entry &e) {
        if (f.isEmpty())
            return true;
        if (e.mimeType.contains(f) || fold(e.comment).contains(f) || fold(e.section).contains(f) || fold(e.defaultName).contains(f))
            return true;
        return std::any_of(e.patterns.cbegin(), e.patterns.cend(), [&](const QString &p) { return p.compare(ext, Qt::CaseInsensitive) == 0; });
    };

    beginResetModel();
    m_rows.clear();
    m_extra.clear();
    QSet<QString> listed;
    for (int i = 0; i < m_all.size(); ++i) {
        listed.insert(m_all.at(i).mimeType);
        if (matches(m_all.at(i)))
            m_rows << i;
    }
    // Searching also reaches the rest of the MIME database: by extension, exact name or text.
    if (!f.isEmpty()) {
        const QMimeDatabase db;
        QList<QMimeType> found;
        if (!f.contains(QLatin1Char('/')))
            found << db.mimeTypesForFileName(QStringLiteral("x.") + f);
        const QMimeType byName = db.mimeTypeForName(f);
        if (byName.isValid())
            found << byName;
        if (f.size() >= 3) {
            const QList<QMimeType> everything = db.allMimeTypes();
            for (const QMimeType &mt : everything)
                if (mt.name().contains(f) || fold(mt.comment()).contains(f))
                    found << mt;
        }
        for (const QMimeType &mt : std::as_const(found)) {
            if (mt.isDefault() || listed.contains(mt.name()) || m_extra.size() >= 40)
                continue;
            listed.insert(mt.name());
            m_extra << makeEntry(mt.name(), tr("Other types"));
            m_rows << -int(m_extra.size());
        }
    }
    endResetModel();
    Q_EMIT countChanged();
}

int DefaultAppsModel::focusMimeType(const QString &mimeType)
{
    const QMimeType mt = QMimeDatabase().mimeTypeForName(mimeType);
    if (!mt.isValid())
        return -1;
    if (!m_filter.isEmpty()) {
        m_filter.clear();
        Q_EMIT filterChanged();
    }
    if (!m_loaded)
        reload();
    const bool listed = std::any_of(m_all.cbegin(), m_all.cend(), [&](const Entry &e) { return e.mimeType == mt.name(); });
    if (!listed) {
        m_pinned.removeAll(mt.name());
        m_pinned.prepend(mt.name());
        reload();
    } else {
        rebuildRows();
    }
    for (int row = 0; row < m_rows.size(); ++row) {
        const int r = m_rows.at(row);
        if (r >= 0 && m_all.at(r).mimeType == mt.name())
            return row;
    }
    return -1;
}

QString DefaultAppsModel::mimeTypeAt(int row) const
{
    return data(index(row), MimeTypeRole).toString();
}

bool DefaultAppsModel::setDefault(const QString &mimeType, const QString &storageId)
{
    const KService::Ptr service = KService::serviceByStorageId(storageId);
    if (mimeType.isEmpty() || !service || !service->isValid()) {
        Q_EMIT failed(tr("Application «%1» not found").arg(storageId));
        return false;
    }
    // Writes [Default Applications] in ~/.config/mimeapps.list (shared with every XDG app).
    KApplicationTrader::setPreferredService(mimeType, service);

    m_busy = true;
    Q_EMIT busyChanged();
    const QString name = service->name();
    const QString kbuildsycoca = QStandardPaths::findExecutable(QStringLiteral("kbuildsycoca6"));
    if (kbuildsycoca.isEmpty()) {
        finishSycocaRefresh(mimeType, name); // ensureCacheValid() rebuilds in-process
        return true;
    }
    // Rebuild the user's KSycoca out of process so the UI stays responsive.
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, mimeType, name] {
        proc->deleteLater();
        finishSycocaRefresh(mimeType, name);
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, mimeType, name](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        finishSycocaRefresh(mimeType, name);
    });
    proc->start(kbuildsycoca, {});
    return true;
}

void DefaultAppsModel::finishSycocaRefresh(const QString &mimeType, const QString &appName)
{
    KSycoca::self()->ensureCacheValid();
    refreshApps();
    m_busy = false;
    Q_EMIT busyChanged();
    const KService::Ptr now = KApplicationTrader::preferredService(mimeType);
    if (now && now->name() == appName)
        Q_EMIT defaultChanged(mimeType, appName);
    else
        Q_EMIT failed(tr("The system did not apply %1 for %2; check ~/.config/mimeapps.list").arg(appName, mimeType));
}

void DefaultAppsModel::refreshApps()
{
    for (Entry &e : m_all)
        resolveApps(e);
    for (Entry &e : m_extra)
        resolveApps(e);
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0), index(int(m_rows.size()) - 1));
}

bool DefaultAppsModel::hasIcon(const QString &name) const
{
    if (name.isEmpty())
        return false;
    if (QFileInfo(name).isAbsolute())
        return QFileInfo::exists(name);
    return QIcon::hasThemeIcon(name);
}
