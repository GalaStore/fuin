// SPDX-License-Identifier: MIT
// D-Bus structs of org.freedesktop.impl.portal.FileChooser.
#pragma once

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>

struct PortalFilterRule {
    uint type = 0; // 0: glob, 1: MIME type
    QString pattern;
};

struct PortalFilter { // (sa(us))
    QString name;
    QList<PortalFilterRule> rules;
    bool operator==(const PortalFilter &o) const
    {
        if (name != o.name || rules.size() != o.rules.size())
            return false;
        for (qsizetype i = 0; i < rules.size(); ++i)
            if (rules[i].type != o.rules[i].type || rules[i].pattern != o.rules[i].pattern)
                return false;
        return true;
    }
};

struct PortalChoiceOption {
    QString id, label;
};

struct PortalChoice { // (ssa(ss)s)
    QString id, label;
    QList<PortalChoiceOption> options;
    QString initial;
};

struct PortalChoiceAnswer { // (ss)
    QString id, value;
};

Q_DECLARE_METATYPE(PortalFilterRule)
Q_DECLARE_METATYPE(PortalFilter)
Q_DECLARE_METATYPE(PortalChoiceOption)
Q_DECLARE_METATYPE(PortalChoice)
Q_DECLARE_METATYPE(PortalChoiceAnswer)

inline QDBusArgument &operator<<(QDBusArgument &a, const PortalFilterRule &r)
{
    a.beginStructure();
    a << r.type << r.pattern;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, PortalFilterRule &r)
{
    a.beginStructure();
    a >> r.type >> r.pattern;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const PortalFilter &f)
{
    a.beginStructure();
    a << f.name << f.rules;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, PortalFilter &f)
{
    a.beginStructure();
    a >> f.name >> f.rules;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const PortalChoiceOption &o)
{
    a.beginStructure();
    a << o.id << o.label;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, PortalChoiceOption &o)
{
    a.beginStructure();
    a >> o.id >> o.label;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const PortalChoice &c)
{
    a.beginStructure();
    a << c.id << c.label << c.options << c.initial;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, PortalChoice &c)
{
    a.beginStructure();
    a >> c.id >> c.label >> c.options >> c.initial;
    a.endStructure();
    return a;
}
inline QDBusArgument &operator<<(QDBusArgument &a, const PortalChoiceAnswer &c)
{
    a.beginStructure();
    a << c.id << c.value;
    a.endStructure();
    return a;
}
inline const QDBusArgument &operator>>(const QDBusArgument &a, PortalChoiceAnswer &c)
{
    a.beginStructure();
    a >> c.id >> c.value;
    a.endStructure();
    return a;
}

void registerPortalTypes();
