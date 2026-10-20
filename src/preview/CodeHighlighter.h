// SPDX-License-Identifier: MIT
// Attaches KSyntaxHighlighting to a QML TextEdit/TextArea document (no-op when unavailable).
#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>

class QSyntaxHighlighter;

class CodeHighlighter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY changed)
    Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY changed)
    Q_PROPERTY(QString definitionName READ definitionName WRITE setDefinitionName NOTIFY changed)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
public:
    explicit CodeHighlighter(QObject *parent = nullptr);
    QQuickTextDocument *document() const { return m_doc; }
    void setDocument(QQuickTextDocument *d);
    QString fileName() const { return m_fileName; }
    void setFileName(const QString &f);
    QString definitionName() const { return m_defName; }
    void setDefinitionName(const QString &n);
    bool dark() const { return m_dark; }
    void setDark(bool d);
    QString language() const { return m_language; }

Q_SIGNALS:
    void changed();

private:
    void apply();
    QPointer<QQuickTextDocument> m_doc;
    QString m_fileName, m_defName, m_language;
    bool m_dark = true;
    QSyntaxHighlighter *m_hl = nullptr;
};
