// SPDX-License-Identifier: MIT
// xdg-desktop-portal FileChooser backend (`fuin --portal`). Each D-Bus call opens a picker
// window through QML; the reply is delayed until the user accepts or cancels it.
#pragma once

#include "PortalTypes.h"

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>

class PortalRequest;

class FileChooserPortal : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
public:
    static constexpr auto kService = "org.freedesktop.impl.portal.desktop.fuin";
    static constexpr auto kPath = "/org/freedesktop/portal/desktop";

    explicit FileChooserPortal(QObject *parent = nullptr);
    bool registerOnBus();

    // The picker's answer. For "saveFiles" `urls` holds the chosen folder; `filter` indexes the
    // request's filters (-1: none).
    Q_INVOKABLE void respond(const QString &id, bool accepted, const QStringList &urls, int filter);
    Q_INVOKABLE bool exists(const QString &url) const;

public Q_SLOTS:
    Q_SCRIPTABLE uint OpenFile(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                               const QVariantMap &options, QVariantMap &results);
    Q_SCRIPTABLE uint SaveFile(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                               const QVariantMap &options, QVariantMap &results);
    Q_SCRIPTABLE uint SaveFiles(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                                const QVariantMap &options, QVariantMap &results);

Q_SIGNALS:
    void requested(const QVariantMap &request);
    void closeRequested(const QString &id);

private:
    enum Response : uint { Success = 0, Cancelled = 1, Ended = 2 };
    struct Request {
        QDBusMessage msg;
        QString mode;
        QList<PortalFilter> filters;
        QList<PortalChoice> choices;
        QStringList files;
        QPointer<PortalRequest> object;
    };
    void begin(const QString &mode, const QDBusObjectPath &handle, const QString &appId, const QString &title, const QVariantMap &options);
    void finish(const QString &id, uint response, const QVariantMap &results);
    void armIdleExit();

    QHash<QString, Request> m_requests;
    QTimer m_idle;
};

// org.freedesktop.impl.portal.Request at the call's handle path: lets the frontend cancel it.
class PortalRequest : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")
public:
    using QObject::QObject;
public Q_SLOTS:
    Q_SCRIPTABLE void Close() { Q_EMIT closed(); }
Q_SIGNALS:
    void closed();
};
