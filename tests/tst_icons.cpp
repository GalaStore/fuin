// SPDX-License-Identifier: MIT
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGlyphRun>
#include <QRegularExpression>
#include <QSet>
#include <QTest>
#include <QTextLayout>

using namespace Qt::StringLiterals;

class TestIcons : public QObject
{
    Q_OBJECT

    QString m_family;
    QSet<QString> m_listed;

    static QString read(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

    bool isOneGlyph(const QString &name) const
    {
        QTextLayout layout(name, QFont(m_family, 24));
        layout.beginLayout();
        layout.createLine();
        layout.endLayout();
        const auto runs = layout.glyphRuns();
        if (runs.size() != 1 || runs.first().glyphIndexes().size() != 1)
            return false;
        return runs.first().glyphIndexes().first() != 0;
    }

    QSet<QString> usedInSources() const
    {
        QSet<QString> used;
        static const QRegularExpression key(u"\\b(?:icon|glyph|i)\\s*:\\s*([^;}\\n]*)"_s);
        static const QRegularExpression iconText(u"\\bIcon\\s*\\{[^}\\n]*?\\btext\\s*:\\s*\"([a-z][a-z0-9_]*)\""_s);
        static const QRegularExpression glyphsReturn(u"(?:\\breturn|\\?|:)\\s*\"([a-z][a-z0-9_]*)\""_s);
        static const QRegularExpression literal(u"\"([a-z][a-z0-9_]*)\""_s);
        static const QRegularExpression comparison(u"[=!]==?\\s*\"[^\"]*\""_s);
        const QDir qml(QStringLiteral(FUIN_QML_DIR));
        for (const QString &file : qml.entryList({u"*.qml"_s, u"*.js"_s}, QDir::Files)) {
            const QString text = read(qml.filePath(file));
            for (auto it = key.globalMatch(text); it.hasNext();) {
                QString expr = it.next().captured(1);
                expr.remove(comparison);
                if (expr.contains(u'(') && !expr.contains(u'?'))
                    continue;
                for (auto l = literal.globalMatch(expr); l.hasNext();)
                    used.insert(l.next().captured(1));
            }
            for (auto it = iconText.globalMatch(text); it.hasNext();)
                used.insert(it.next().captured(1));
            if (file == u"Glyphs.js") {
                const int end = text.indexOf(u"function tone"_s);
                for (auto it = glyphsReturn.globalMatch(text.left(end)); it.hasNext();)
                    used.insert(it.next().captured(1));
            }
        }
        return used;
    }

private Q_SLOTS:
    void initTestCase()
    {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(FUIN_ICON_FONT));
        QVERIFY(id >= 0);
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        QVERIFY(!families.isEmpty());
        m_family = families.first();
        for (const QString &line : read(QStringLiteral(FUIN_ICON_LIST)).split(u'\n', Qt::SkipEmptyParts))
            m_listed.insert(line.trimmed());
        QVERIFY(m_listed.size() > 50);
    }

    void everyListedIconShapesToOneGlyph()
    {
        for (const QString &name : std::as_const(m_listed))
            QVERIFY2(isOneGlyph(name), qPrintable(u"missing from the subset: "_s + name));
    }

    void unknownNameDoesNotShapeToOneGlyph() { QVERIFY(!isOneGlyph(u"definitely_not_an_icon"_s)); }

    void everyIconUsedInQmlIsInTheSubset()
    {
        const QSet<QString> used = usedInSources();
        QVERIFY(used.size() > 50);
        for (const QString &name : used)
            QVERIFY2(m_listed.contains(name) && isOneGlyph(name), qPrintable(u"used but not in the subset: "_s + name + u" (run tools/subset-icons.sh)"_s));
    }
};

QTEST_MAIN(TestIcons)
#include "tst_icons.moc"
