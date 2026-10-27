// SPDX-License-Identifier: MIT
#include "PlacesModel.h"
#include "../app/Config.h"

#include <KIO/Global>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>
#include <Solid/StorageDrive>
#include <Solid/StorageVolume>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    auto *n = Solid::DeviceNotifier::instance();
    connect(n, &Solid::DeviceNotifier::deviceAdded, this, &PlacesModel::reload);
    connect(n, &Solid::DeviceNotifier::deviceRemoved, this, &PlacesModel::reload);
    m_usageTimer.setInterval(30000);
    connect(&m_usageTimer, &QTimer::timeout, this, &PlacesModel::reload);
    m_usageTimer.start();
    reload();
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    return {{SectionRole, "section"}, {NameRole, "name"},       {UrlRole, "url"},         {IconRole, "icon"},  {UsageRole, "usage"},
            {DetailRole, "detail"},   {MountedRole, "mounted"}, {RemovableRole, "removable"}, {UdiRole, "udi"}};
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case SectionRole: return e.section;
    case NameRole: return e.name;
    case UrlRole: return e.url;
    case IconRole: return e.icon;
    case UsageRole: return e.usage;
    case DetailRole: return e.detail;
    case MountedRole: return e.mounted;
    case RemovableRole: return e.removable;
    case UdiRole: return e.udi;
    }
    return {};
}

void PlacesModel::reload()
{
    QList<Entry> out;
    auto place = [&out](const QString &section, const QString &name, const QString &path, const QString &icon) {
        Entry e;
        e.section = section;
        e.name = name;
        e.url = path.contains(QLatin1String(":/")) ? path : QUrl::fromLocalFile(path).toString();
        e.icon = icon;
        out << e;
    };
    const QString home = QDir::homePath();
    const QString places = tr("Places");
    place(places, tr("Home"), home, QStringLiteral("home"));
    struct Std {
        QStandardPaths::StandardLocation loc;
        const char *name;
        const char *icon;
    };
    const Std stds[] = {
        {QStandardPaths::DesktopLocation, QT_TR_NOOP("Desktop"), "desktop_windows"}, {QStandardPaths::DownloadLocation, QT_TR_NOOP("Downloads"), "download"},
        {QStandardPaths::DocumentsLocation, QT_TR_NOOP("Documents"), "description"}, {QStandardPaths::PicturesLocation, QT_TR_NOOP("Pictures"), "image"},
        {QStandardPaths::MusicLocation, QT_TR_NOOP("Music"), "music_note"},         {QStandardPaths::MoviesLocation, QT_TR_NOOP("Videos"), "movie"},
    };
    for (const Std &s : stds) {
        const QString p = QStandardPaths::writableLocation(s.loc);
        if (!p.isEmpty() && p != home && QFileInfo(p).isDir())
            place(places, tr(s.name), p, QString::fromLatin1(s.icon));
    }
    place(places, tr("Recent"), QStringLiteral("recentlyused:/files"), QStringLiteral("schedule"));
    for (const QVariant &v : Config::instance()->extraPlaces()) {
        const QVariantMap m = v.toMap();
        place(places, m.value(QStringLiteral("name")).toString(), m.value(QStringLiteral("path")).toString(),
              m.value(QStringLiteral("icon")).toString());
    }
    const QStringList projects = Config::instance()->projectDirs();
    for (const QString &p : projects)
        place(tr("Projects"), projects.size() == 1 ? tr("Projects") : QFileInfo(p).fileName(), p, QStringLiteral("terminal"));

    // ---- drives
    {
        Entry root;
        root.section = tr("Drives");
        root.name = tr("System");
        root.url = QUrl::fromLocalFile(QStringLiteral("/")).toString();
        root.icon = QStringLiteral("hard_drive");
        const QStorageInfo si(QStringLiteral("/"));
        if (si.bytesTotal() > 0) {
            root.usage = 1.0 - qreal(si.bytesAvailable()) / qreal(si.bytesTotal());
            root.detail = tr("%1 free of %2").arg(KIO::convertSize(si.bytesAvailable()), KIO::convertSize(si.bytesTotal()));
        }
        out << root;
    }
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &d : devices) {
        const auto *acc = d.as<Solid::StorageAccess>();
        if (!acc || acc->isIgnored())
            continue;
        const QString mp = acc->filePath();
        if (mp == QLatin1String("/") || mp.startsWith(QLatin1String("/boot")) || mp == home || mp == QLatin1String("/home")
            || mp.startsWith(QLatin1String("/var")) || mp.startsWith(QLatin1String("/.snapshots")))
            continue;
        Entry e;
        e.section = tr("Drives");
        e.udi = d.udi();
        e.name = d.description().isEmpty() ? d.product() : d.description();
        e.mounted = acc->isAccessible();
        e.url = e.mounted ? QUrl::fromLocalFile(mp).toString() : QString();
        Solid::Device drive = d;
        while (drive.isValid() && !drive.is<Solid::StorageDrive>())
            drive = drive.parent();
        if (drive.isValid()) {
            const auto *sd = drive.as<Solid::StorageDrive>();
            e.removable = sd->isRemovable() || sd->isHotpluggable();
        }
        e.icon = e.removable ? QStringLiteral("usb") : QStringLiteral("storage");
        if (const auto *vol = d.as<Solid::StorageVolume>())
            e.detail = KIO::convertSize(vol->size()) + QStringLiteral(" · ") + vol->fsType();
        if (e.mounted) {
            const QStorageInfo si(mp);
            if (si.bytesTotal() > 0) {
                e.usage = 1.0 - qreal(si.bytesAvailable()) / qreal(si.bytesTotal());
                e.detail = tr("%1 free of %2").arg(KIO::convertSize(si.bytesAvailable()), KIO::convertSize(si.bytesTotal()));
            }
        }
        out << e;
    }
    place(tr("Network"), tr("Network"), QStringLiteral("remote:/"), QStringLiteral("lan"));
    place(tr("Other"), tr("Trash"), QStringLiteral("trash:/"), QStringLiteral("delete"));

    beginResetModel();
    m_entries = out;
    endResetModel();
    Q_EMIT countChanged();
}

void PlacesModel::mount(const QString &udi)
{
    Solid::Device d(udi);
    auto *acc = d.as<Solid::StorageAccess>();
    if (!acc)
        return;
    connect(acc, &Solid::StorageAccess::setupDone, this,
            [this, acc](Solid::ErrorType err, const QVariant &data, const QString &) {
                if (err != Solid::NoError) {
                    Q_EMIT message(tr("Could not mount: %1").arg(data.toString()), QStringLiteral("error"));
                    return;
                }
                reload();
                Q_EMIT mounted(QUrl::fromLocalFile(acc->filePath()).toString());
            },
            Qt::SingleShotConnection);
    acc->setup();
}

void PlacesModel::unmount(const QString &udi)
{
    Solid::Device d(udi);
    auto *acc = d.as<Solid::StorageAccess>();
    if (!acc)
        return;
    connect(acc, &Solid::StorageAccess::teardownDone, this,
            [this](Solid::ErrorType err, const QVariant &data, const QString &) {
                if (err != Solid::NoError)
                    Q_EMIT message(tr("Could not eject: %1").arg(data.toString()), QStringLiteral("error"));
                else
                    Q_EMIT message(tr("Ejected. You can now remove it."), QStringLiteral("ok"));
                reload();
            },
            Qt::SingleShotConnection);
    acc->teardown();
}
