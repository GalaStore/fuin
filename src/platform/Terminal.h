// SPDX-License-Identifier: MIT
#pragma once

#include <QString>
#include <QStringList>

namespace Terminal {

struct Launch {
    QString program;
    QStringList args;
    QString workingDirectory;
};

QString detect();

Launch launch(const QString &terminal, const QString &dir, const QString &command);

}
