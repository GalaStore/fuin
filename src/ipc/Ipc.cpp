// SPDX-License-Identifier: MIT
#include "Ipc.h"
#include "../app/Config.h"
#include "../fs/FileOps.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QUrl>

using namespace Qt::StringLiterals;

QJsonObject IpcClient::request(const QJsonObject &req, int timeoutMs)
{
    QLocalSocket s;
    s.connectToServer(Config::socketPath());
    if (!s.waitForConnected(400))
        return {};
    s.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n');
    s.flush();
    QByteArray buf;
    while (!buf.contains('\n')) {
        if (!s.waitForReadyRead(timeoutMs))
            break;
        buf += s.readAll();
    }
    return QJsonDocument::fromJson(buf.trimmed()).object();
}

bool IpcClient::isRunning()
{
    return request({{u"cmd"_s, u"ping"_s}}, 800).value(u"ok"_s).toBool();
}

IpcServer::IpcServer(FileOps *ops, QObject *parent)
    : QObject(parent)
    , m_ops(ops)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *sock = m_server.nextPendingConnection()) {
            connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
            connect(sock, &QLocalSocket::readyRead, this, [this, sock] {
                while (sock->canReadLine()) {
                    const QJsonObject req = QJsonDocument::fromJson(sock->readLine().trimmed()).object();
                    handle(sock, req);
                }
            });
        }
    });
}

QString IpcServer::socketPath() const
{
    return Config::socketPath();
}

bool IpcServer::listen(const QString &path)
{
    const QString p = path.isEmpty() ? Config::socketPath() : path;
    if (QFile::exists(p) && (!path.isEmpty() || !IpcClient::isRunning()))
        QLocalServer::removeServer(p); // stale socket from a crash
    return m_server.listen(p);
}

void IpcServer::publishState(const QVariantMap &state)
{
    m_state = state;
}

static void reply(QLocalSocket *sock, const QJsonObject &o)
{
    if (!sock)
        return;
    sock->write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n');
    sock->flush();
}

static QString toUrlString(const QString &p)
{
    const QUrl u(p);
    if (!u.scheme().isEmpty() && u.scheme().size() > 1)
        return u.toString();
    return QUrl::fromLocalFile(QFileInfo(Config::expand(p)).absoluteFilePath()).toString();
}

void IpcServer::handle(QLocalSocket *sock, const QJsonObject &req)
{
    const QString cmd = req.value(u"cmd"_s).toString();
    const QString client = req.value(u"client"_s).toString(tr("agent"));
    if (cmd == "ping"_L1) {
        reply(sock, {{u"ok"_s, true}, {u"pid"_s, QCoreApplication::applicationPid()}});
    } else if (cmd == "open"_L1) {
        Q_EMIT openRequested(toUrlString(req.value(u"url"_s).toString()), req.value(u"newWindow"_s).toBool(true));
        reply(sock, {{u"ok"_s, true}});
    } else if (cmd == "reveal"_L1 || cmd == "preview"_L1) {
        const QString url = toUrlString(req.value(u"path"_s).toString());
        if (cmd == "reveal"_L1)
            Q_EMIT revealRequested(url);
        else
            Q_EMIT previewRequested(url);
        if (req.contains(u"client"_s))
            Q_EMIT agentActivity(tr("%1 showed %2").arg(client, QUrl(url).fileName()));
        reply(sock, {{u"ok"_s, true}});
    } else if (cmd == "state"_L1) {
        QJsonObject o = QJsonObject::fromVariantMap(m_state);
        o.insert(u"ok"_s, true);
        reply(sock, o);
    } else if (cmd == "confirm"_L1) {
        const QString id = QString::number(++m_seq);
        m_pending.insert(id, {sock, req});
        Q_EMIT pendingChanged();
        Q_EMIT confirmRequested(id, req.value(u"title"_s).toString(), req.value(u"detail"_s).toString(), client);
    } else if (cmd == "dev"_L1 && qEnvironmentVariableIntValue("FUIN_DEV") == 1) {
        Q_EMIT devRequested(req.toVariantMap());
        reply(sock, {{u"ok"_s, true}});
    } else {
        reply(sock, {{u"ok"_s, false}, {u"error"_s, u"unknown command"_s}});
    }
}

void IpcServer::resolve(const QString &id, bool approved)
{
    const Pending p = m_pending.take(id);
    Q_EMIT pendingChanged();
    if (!p.sock)
        return;
    if (approved) {
        // Perform the approved write here so it lands in the GUI's undo history.
        const QString action = p.req.value(u"action"_s).toString();
        const QJsonObject args = p.req.value(u"args"_s).toObject();
        if (action == "mkdir"_L1) {
            const QFileInfo fi(args.value(u"path"_s).toString());
            m_ops->mkdir(QUrl::fromLocalFile(fi.absolutePath()).toString(), fi.fileName());
        } else if (action == "trash"_L1) {
            QStringList urls;
            for (const QJsonValue &v : args.value(u"paths"_s).toArray())
                urls << QUrl::fromLocalFile(v.toString()).toString();
            m_ops->trash(urls);
        } else if (action == "move"_L1 || action == "copy"_L1) {
            QStringList urls;
            for (const QJsonValue &v : args.value(u"sources"_s).toArray())
                urls << QUrl::fromLocalFile(v.toString()).toString();
            const QString dest = QUrl::fromLocalFile(args.value(u"destination"_s).toString()).toString();
            action == "move"_L1 ? m_ops->move(urls, dest) : m_ops->copy(urls, dest);
        }
    }
    reply(p.sock, {{u"ok"_s, true}, {u"approved"_s, approved}});
}
