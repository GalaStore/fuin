// SPDX-License-Identifier: MIT
// Local IPC between the running GUI and other Fūin processes (CLI `--reveal`, the MCP
// server). Unix socket at $XDG_RUNTIME_DIR/fuin.sock (user-only), one JSON line per request.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

class QLocalSocket;
class FileOps;

namespace IpcClient
{
// Sends one request to the running GUI; returns {} when no instance is running.
QJsonObject request(const QJsonObject &req, int timeoutMs = 3000);
bool isRunning();
}

class IpcServer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening CONSTANT)
    Q_PROPERTY(QString socketPath READ socketPath CONSTANT)
    Q_PROPERTY(int pendingConfirmations READ pendingConfirmations NOTIFY pendingChanged)
public:
    explicit IpcServer(FileOps *ops, QObject *parent = nullptr);
    bool listen(const QString &path = {}); // default: the GUI's single-instance socket
    bool listening() const { return m_server.isListening(); }
    QString socketPath() const;
    int pendingConfirmations() const { return int(m_pending.size()); }

    // QML publishes what the user is looking at (active pane, selection, tabs).
    Q_INVOKABLE void publishState(const QVariantMap &state);
    // Answer to confirmRequested().
    Q_INVOKABLE void resolve(const QString &id, bool approved);

Q_SIGNALS:
    void openRequested(const QString &url, bool newWindow);
    void revealRequested(const QString &url);
    void previewRequested(const QString &url);
    void confirmRequested(const QString &id, const QString &title, const QString &detail, const QString &client);
    void pendingChanged();
    void agentActivity(const QString &text);
    // Developer automation (only when FUIN_DEV=1): drive the UI and grab screenshots for docs/tests.
    void devRequested(const QVariantMap &args);

private:
    void handle(QLocalSocket *sock, const QJsonObject &req);
    struct Pending {
        QPointer<QLocalSocket> sock;
        QJsonObject req;
    };
    QLocalServer m_server;
    QVariantMap m_state;
    QHash<QString, Pending> m_pending;
    FileOps *m_ops;
    int m_seq = 0;
};
