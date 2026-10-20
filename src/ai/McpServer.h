// SPDX-License-Identifier: MIT
// Model Context Protocol server over stdio (`fuin --mcp`).
//
// Dual-era: answers legacy `initialize` handshakes (2024-11-05 … 2025-11-25) and modern
// stateless requests (2026-07-28, `server/discover`, per-request `_meta`). Read tools are
// sandboxed to configured roots (default: $HOME) and never return secrets. Write tools are
// disabled unless the user enables them in Fūin, and every write must be approved in the GUI.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class McpServer
{
public:
    int run();                                   // blocking stdio loop
    QJsonObject handle(const QJsonObject &msg);  // one JSON-RPC message -> response ({} for notifications)
    static QJsonArray toolDefinitions();

private:
    QJsonObject callTool(const QString &name, const QJsonObject &args);
    QString resolvePath(const QString &p, bool mustExist, QString *err) const;
    bool m_legacy = false;
    QString m_version;
};
