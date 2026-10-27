// SPDX-License-Identifier: MIT
#include "FileChooserPortal.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSet>
#include <QUrl>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcPortal, "fuin.portal")

void registerPortalTypes()
{
    qDBusRegisterMetaType<PortalFilterRule>();
    qDBusRegisterMetaType<QList<PortalFilterRule>>();
    qDBusRegisterMetaType<PortalFilter>();
    qDBusRegisterMetaType<QList<PortalFilter>>();
    qDBusRegisterMetaType<PortalChoiceOption>();
    qDBusRegisterMetaType<QList<PortalChoiceOption>>();
    qDBusRegisterMetaType<PortalChoice>();
    qDBusRegisterMetaType<QList<PortalChoice>>();
    qDBusRegisterMetaType<PortalChoiceAnswer>();
    qDBusRegisterMetaType<QList<PortalChoiceAnswer>>();
    qDBusRegisterMetaType<QList<QByteArray>>();
}

namespace {
// The process is D-Bus activated, so it can leave when idle and the next call starts it again
// (which also picks up an upgraded binary).
constexpr int kIdleExitMs = 10 * 60 * 1000;

// Paths arrive as NUL-terminated byte strings in the file system encoding.
QString bytesToPath(QByteArray b)
{
    if (b.endsWith('\0'))
        b.chop(1);
    return QFile::decodeName(b);
}

template<typename T>
T option(const QVariantMap &options, const char *key)
{
    return qdbus_cast<T>(options.value(QString::fromLatin1(key)));
}

QVariantMap filterToQml(const PortalFilter &f)
{
    QStringList patterns;
    for (const PortalFilterRule &r : f.rules)
        patterns << r.pattern;
    return {{u"name"_s, f.name}, {u"patterns"_s, patterns}};
}

// "name.ext" -> "name (2).ext", skipping names already taken on disk or earlier in the batch.
QString uniquePath(const QDir &dir, const QString &name, QSet<QString> &taken)
{
    const QFileInfo fi(name);
    const QString base = fi.completeBaseName().isEmpty() ? name : fi.completeBaseName();
    const QString ext = fi.completeBaseName().isEmpty() || fi.suffix().isEmpty() ? QString() : u'.' + fi.completeSuffix();
    QString candidate = name;
    for (int n = 2; taken.contains(candidate) || dir.exists(candidate); ++n)
        candidate = u"%1 (%2)%3"_s.arg(base).arg(n).arg(ext);
    taken.insert(candidate);
    return dir.filePath(candidate);
}
}

FileChooserPortal::FileChooserPortal(QObject *parent)
    : QObject(parent)
{
    registerPortalTypes();
    m_idle.setSingleShot(true);
    m_idle.setInterval(kIdleExitMs);
    connect(&m_idle, &QTimer::timeout, this, [this] {
        if (!m_requests.isEmpty())
            return;
        // Release the name first so a call racing with the exit activates a fresh instance.
        QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(kService));
        QCoreApplication::quit();
    });
}

bool FileChooserPortal::registerOnBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QString::fromLatin1(kPath), this, QDBusConnection::ExportScriptableSlots)) {
        qCWarning(lcPortal) << "cannot register" << kPath << bus.lastError().message();
        return false;
    }
    if (!bus.registerService(QString::fromLatin1(kService))) {
        qCWarning(lcPortal) << "cannot own" << kService << bus.lastError().message();
        return false;
    }
    armIdleExit();
    return true;
}

void FileChooserPortal::armIdleExit()
{
    if (m_requests.isEmpty())
        m_idle.start();
    else
        m_idle.stop();
}

uint FileChooserPortal::OpenFile(const QDBusObjectPath &handle, const QString &app_id, const QString &, const QString &title,
                                 const QVariantMap &options, QVariantMap &)
{
    begin(u"open"_s, handle, app_id, title, options);
    return Ended;
}

uint FileChooserPortal::SaveFile(const QDBusObjectPath &handle, const QString &app_id, const QString &, const QString &title,
                                 const QVariantMap &options, QVariantMap &)
{
    begin(u"save"_s, handle, app_id, title, options);
    return Ended;
}

uint FileChooserPortal::SaveFiles(const QDBusObjectPath &handle, const QString &app_id, const QString &, const QString &title,
                                  const QVariantMap &options, QVariantMap &)
{
    begin(u"saveFiles"_s, handle, app_id, title, options);
    return Ended;
}

void FileChooserPortal::begin(const QString &mode, const QDBusObjectPath &handle, const QString &appId, const QString &title, const QVariantMap &options)
{
    const QString id = handle.path();
    if (m_requests.contains(id)) {
        sendErrorReply(QDBusError::InvalidArgs, u"duplicate request handle"_s);
        return;
    }
    setDelayedReply(true);

    Request r;
    r.msg = message();
    r.mode = mode;
    r.filters = option<QList<PortalFilter>>(options, "filters");
    r.choices = option<QList<PortalChoice>>(options, "choices");
    for (const QByteArray &f : option<QList<QByteArray>>(options, "files"))
        r.files << QFileInfo(bytesToPath(f)).fileName();

    int currentFilter = -1;
    if (options.contains(u"current_filter"_s)) {
        const auto cf = option<PortalFilter>(options, "current_filter");
        currentFilter = int(r.filters.indexOf(cf));
        if (currentFilter < 0) { // the spec allows a current filter that is not in the list
            r.filters << cf;
            currentFilter = int(r.filters.size() - 1);
        }
    }
    QVariantList filters;
    for (const PortalFilter &f : std::as_const(r.filters))
        filters << filterToQml(f);

    QString folder = bytesToPath(options.value(u"current_folder"_s).toByteArray());
    QString name = options.value(u"current_name"_s).toString();
    const QString file = bytesToPath(options.value(u"current_file"_s).toByteArray());
    if (!file.isEmpty()) {
        const QFileInfo fi(file);
        folder = fi.absolutePath();
        name = fi.fileName();
    }
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = QDir::homePath();

    r.object = new PortalRequest(this);
    connect(r.object, &PortalRequest::closed, this, [this, id] {
        finish(id, Ended, {});
        Q_EMIT closeRequested(id);
    });
    QDBusConnection::sessionBus().registerObject(id, r.object, QDBusConnection::ExportScriptableSlots);
    m_requests.insert(id, r);
    armIdleExit();

    Q_EMIT requested({
        {u"id"_s, id},
        {u"mode"_s, mode},
        {u"appId"_s, appId},
        {u"title"_s, title},
        {u"acceptLabel"_s, options.value(u"accept_label"_s).toString()},
        {u"multiple"_s, mode == "open"_L1 && options.value(u"multiple"_s).toBool()},
        {u"directory"_s, mode == "open"_L1 && options.value(u"directory"_s).toBool()},
        {u"filters"_s, filters},
        {u"currentFilter"_s, currentFilter},
        {u"folder"_s, QUrl::fromLocalFile(folder).toString()},
        {u"name"_s, mode == "save"_L1 ? name : QString()},
        {u"files"_s, r.files},
    });
}

void FileChooserPortal::respond(const QString &id, bool accepted, const QStringList &urls, int filter)
{
    const auto it = m_requests.constFind(id);
    if (it == m_requests.cend())
        return;
    if (!accepted) {
        finish(id, Cancelled, {});
        return;
    }
    QStringList uris;
    for (const QString &u : urls) {
        const QUrl url(u);
        if (url.isLocalFile()) // the portal only hands out file:// URIs
            uris << url.toString(QUrl::FullyEncoded);
    }
    if (it->mode == "saveFiles"_L1 && !uris.isEmpty()) {
        const QDir dir(QUrl(uris.first()).toLocalFile());
        QSet<QString> taken;
        uris.clear();
        for (const QString &f : it->files)
            uris << QUrl::fromLocalFile(uniquePath(dir, f, taken)).toString(QUrl::FullyEncoded);
    } else if (it->mode == "save"_L1) {
        uris = uris.mid(0, 1);
    }
    if (uris.isEmpty()) {
        finish(id, Ended, {});
        return;
    }

    QVariantMap results{{u"uris"_s, uris}};
    if (filter >= 0 && filter < it->filters.size())
        results.insert(u"current_filter"_s, QVariant::fromValue(it->filters.at(filter)));
    if (!it->choices.isEmpty()) {
        // Choices are not shown yet; answering with their initial values is what the spec expects then.
        QList<PortalChoiceAnswer> answers;
        for (const PortalChoice &c : it->choices)
            answers << PortalChoiceAnswer{c.id, c.initial};
        results.insert(u"choices"_s, QVariant::fromValue(answers));
    }
    finish(id, Success, results);
}

bool FileChooserPortal::exists(const QString &url) const
{
    return QFileInfo::exists(QUrl(url).toLocalFile());
}

void FileChooserPortal::finish(const QString &id, uint response, const QVariantMap &results)
{
    if (!m_requests.contains(id))
        return;
    const Request r = m_requests.take(id);
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.send(r.msg.createReply({response, results}));
    bus.unregisterObject(id);
    if (r.object)
        r.object->deleteLater();
    armIdleExit();
}
