// SPDX-License-Identifier: MIT
// Coarse file categories used for icons, colours and preview routing.
#pragma once

#include <QString>

namespace FileKind
{
// Returns one of: folder, image, video, audio, pdf, data, model, code, markdown, archive, text, other
QString classify(const QString &fileName, const QString &mimeType, bool isDir);
QString extension(const QString &fileName);
}
