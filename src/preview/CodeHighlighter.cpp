// SPDX-License-Identifier: MIT
#include "CodeHighlighter.h"

#include <QSyntaxHighlighter>
#include <QTextDocument>

#ifdef ARCHIVOS_HAVE_KSYNTAX
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>

static KSyntaxHighlighting::Repository &repo()
{
    static KSyntaxHighlighting::Repository r;
    return r;
}
#endif

CodeHighlighter::CodeHighlighter(QObject *parent)
    : QObject(parent)
{
}

void CodeHighlighter::setDocument(QQuickTextDocument *d)
{
    if (d == m_doc)
        return;
    m_doc = d;
    delete m_hl;
    m_hl = nullptr;
    apply();
}

void CodeHighlighter::setFileName(const QString &f)
{
    if (f == m_fileName)
        return;
    m_fileName = f;
    apply();
}

void CodeHighlighter::setDefinitionName(const QString &n)
{
    if (n == m_defName)
        return;
    m_defName = n;
    apply();
}

void CodeHighlighter::setDark(bool d)
{
    if (d == m_dark)
        return;
    m_dark = d;
    apply();
}

void CodeHighlighter::apply()
{
#ifdef ARCHIVOS_HAVE_KSYNTAX
    if (!m_doc || !m_doc->textDocument())
        return;
    if (!m_hl)
        m_hl = new KSyntaxHighlighting::SyntaxHighlighter(m_doc->textDocument());
    auto *hl = static_cast<KSyntaxHighlighting::SyntaxHighlighter *>(m_hl);
    KSyntaxHighlighting::Definition def = m_defName.isEmpty() ? repo().definitionForFileName(m_fileName) : repo().definitionForName(m_defName);
    hl->setTheme(repo().defaultTheme(m_dark ? KSyntaxHighlighting::Repository::DarkTheme : KSyntaxHighlighting::Repository::LightTheme));
    hl->setDefinition(def);
    m_language = def.isValid() ? def.name() : QString();
    hl->rehighlight();
#endif
    Q_EMIT changed();
}
