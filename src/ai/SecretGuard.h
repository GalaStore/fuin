// SPDX-License-Identifier: MIT
// Heuristics that keep credentials out of AI context packs, MCP reads and shares.
#pragma once

#include <QByteArray>
#include <QString>

namespace SecretGuard
{
// True when the file name alone looks like a credential (.env, id_rsa, *.pem, ...).
bool isSecretName(const QString &fileName);
// Returns a short human reason when the content contains something that looks like a key/token.
QString scanContent(const QByteArray &data);
// True when the data looks binary (NUL bytes / invalid UTF-8 in the first block).
bool looksBinary(const QByteArray &data);
}
