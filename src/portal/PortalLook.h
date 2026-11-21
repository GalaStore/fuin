// SPDX-License-Identifier: MIT
// `fuin --portal` runs with Qt's generic platform theme (the user's may call the portal and
// deadlock its startup), so the parts of the desktop look that theme would have set are read here.
#pragma once

#include <QByteArray>

namespace PortalLook {
// `platformTheme` is the QT_QPA_PLATFORMTHEME the process was started with.
void apply(const QByteArray &platformTheme);
}
