// SPDX-License-Identifier: MIT
// One browsing location: KCoreDirLister-backed listing with live updates, history, selection,
// filter-as-you-type, recursive search (fd) and git badges.
#pragma once

#include <KFileItem>
#include <QAbstractListModel>
#include <QCollator>
#include <QElapsedTimer>
#include <QHash>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QUrl>

class KCoreDirLister;

class FolderModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString url READ urlString WRITE navigate NOTIFY locationChanged)
    Q_PROPERTY(QString path READ displayPath NOTIFY locationChanged)
    Q_PROPERTY(QString localPath READ localPath NOTIFY locationChanged)
    Q_PROPERTY(QString title READ title NOTIFY locationChanged)
    Q_PROPERTY(bool isLocal READ isLocal NOTIFY locationChanged)
    Q_PROPERTY(bool isTrash READ isTrash NOTIFY locationChanged)
    Q_PROPERTY(bool canBack READ canBack NOTIFY locationChanged)
    Q_PROPERTY(bool canForward READ canForward NOTIFY locationChanged)
    Q_PROPERTY(bool canUp READ canUp NOTIFY locationChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY loadingChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY searchChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY optionsChanged)
    Q_PROPERTY(QString sortField READ sortField WRITE setSortField NOTIFY optionsChanged)
    Q_PROPERTY(bool sortReversed READ sortReversed WRITE setSortReversed NOTIFY optionsChanged)
    Q_PROPERTY(bool dirsOnly READ dirsOnly WRITE setDirsOnly NOTIFY optionsChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(qint64 selectionSize READ selectionSize NOTIFY selectionChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString repoRoot READ repoRoot NOTIFY repoChanged)
    Q_PROPERTY(QString repoBranch READ repoBranch NOTIFY repoChanged)
    Q_PROPERTY(int repoDirty READ repoDirty NOTIFY repoChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        PathRole,
        IsDirRole,
        SizeRole,
        MTimeRole,
        MimeRole,
        MimeCommentRole,
        IconNameRole,
        HiddenRole,
        LinkRole,
        SecretRole,
        KindRole,
        ExtRole,
        GitBranchRole,
        GitDirtyRole,
        SelectedRole,
        LocationRole,
        ThumbRole,
    };
    Q_ENUM(Roles)

    explicit FolderModel(QObject *parent = nullptr);
    ~FolderModel() override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString urlString() const { return m_url.toString(); }
    QString displayPath() const;
    QString localPath() const { return m_url.isLocalFile() ? m_url.toLocalFile() : QString(); }
    QString title() const;
    bool isLocal() const { return m_url.isLocalFile(); }
    bool isTrash() const { return m_url.scheme() == QLatin1String("trash"); }
    bool canBack() const { return m_histIdx > 0; }
    bool canForward() const { return m_histIdx >= 0 && m_histIdx < m_history.size() - 1; }
    bool canUp() const;
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    int count() const { return int(m_view.size()); }
    int totalCount() const { return int(m_source.size()); }
    QString filter() const { return m_filter; }
    void setFilter(const QString &f);
    QString searchQuery() const { return m_query; }
    void setSearchQuery(const QString &q);
    bool searching() const { return !m_query.trimmed().isEmpty(); }
    bool searchBusy() const { return m_searchBusy; }
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool v);
    QString sortField() const { return m_sortField; }
    void setSortField(const QString &f);
    bool sortReversed() const { return m_sortReversed; }
    void setSortReversed(bool v);
    bool dirsOnly() const { return m_dirsOnly; }
    void setDirsOnly(bool v);
    int selectionCount() const { return int(m_sel.size()); }
    qint64 selectionSize() const;
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int i);
    QString repoRoot() const { return m_repoRoot; }
    QString repoBranch() const { return m_repoBranch; }
    int repoDirty() const { return m_repoDirty; }

    Q_INVOKABLE void navigate(const QString &input);
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void up();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOfUrl(const QString &url) const;
    Q_INVOKABLE QString parentUrl() const;
    Q_INVOKABLE QVariantList crumbs() const;

    Q_INVOKABLE void selectOnly(int row);
    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void selectRange(int from, int to);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void setSelection(const QList<int> &rows, bool add);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectUrl(const QString &url); // now or as soon as it appears
    Q_INVOKABLE bool isSelected(int row) const;
    Q_INVOKABLE QStringList selectedUrls() const;
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE QVariantList selectedItems() const;

    KFileItem itemAt(int row) const;
    QList<QUrl> selectedUrlList() const;

Q_SIGNALS:
    void locationChanged();
    void loadingChanged();
    void countChanged();
    void filterChanged();
    void searchChanged();
    void optionsChanged();
    void selectionChanged();
    void currentIndexChanged();
    void repoChanged();
    void revealRow(int row);

private:
    void open(const QUrl &url, bool pushHistory);
    void rebuild();
    bool accepts(const KFileItem &it) const;
    bool lessThan(const KFileItem &a, const KFileItem &b) const;
    int insertPosition(const KFileItem &it) const;
    void onNewItems(const KFileItemList &items);
    void onDeleted(const KFileItemList &items);
    void onRefresh(const QList<QPair<KFileItem, KFileItem>> &items);
    void onCompleted();
    void runSearch();
    void scanGit();
    void nextDirtyCheck();
    void updateRepo();
    void emitSelectionRoles();
    void applyPendingSelect();
    QVariant roleValue(const KFileItem &it, int role) const;

    KCoreDirLister *m_lister = nullptr;
    QUrl m_url;
    QList<QUrl> m_history;
    int m_histIdx = -1;
    bool m_loading = false;
    QString m_error;

    QList<KFileItem> m_source; // all items of the folder (or search results)
    QList<KFileItem> m_view;   // filtered + sorted
    mutable bool m_rowsDirty = true;
    void ensureRowIndex() const;
    mutable QHash<QUrl, int> m_rowCache;

    QString m_filter;
    QString m_query;
    bool m_searchBusy = false;
    QPointer<QProcess> m_search;
    QTimer m_searchDebounce;
    QList<KFileItem> m_results;

    bool m_showHidden = false;
    QString m_sortField = QStringLiteral("name");
    bool m_sortReversed = false;
    bool m_dirsOnly = false;
    QCollator m_collator;

    QSet<QUrl> m_sel;
    int m_anchor = -1;
    int m_current = -1;
    QUrl m_pendingSelect;

    QHash<QString, QString> m_gitBranch; // dir name -> branch
    QHash<QString, int> m_gitDirty;      // dir name -> changed files
    QStringList m_dirtyQueue;
    QPointer<QProcess> m_dirtyProc;
    QString m_repoRoot, m_repoBranch;
    int m_repoDirty = -1;
    QPointer<QProcess> m_repoProc;

    // Listing cost counters (issue #10), reset in open() and logged by onCompleted()
    // under the "fuin.folder.timing" category.
    struct ListingStats {
        int batches = 0, items = 0, maxBatch = 0, rebuilds = 0;
        qint64 rebuildNs = 0, sortNs = 0, insertNs = 0, newItemsNs = 0;
    } m_stats;
    QElapsedTimer m_listTimer;
};
