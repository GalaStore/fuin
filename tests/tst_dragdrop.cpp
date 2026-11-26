// SPDX-License-Identifier: MIT
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>

class TestDragDrop : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dropsOnAreaInsideSourcePane()
    {
        QQuickView view;
        QQmlComponent component(view.engine(), QUrl::fromLocalFile(QStringLiteral(FUIN_DRAGDROP_QML)));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        auto *root = qobject_cast<QQuickItem *>(component.create());
        QVERIFY(root);
        view.setContent(QUrl(), &component, root);
        view.resize(400, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(50, 30));
        for (int x = 60; x <= 290; x += 20)
            QTest::mouseMove(&view, QPoint(x, 40));
        QTRY_VERIFY(root->property("entered").toInt() > 0);
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(290, 40));
        QTRY_COMPARE(root->property("dropped").toString(), QStringLiteral("pane"));
    }
};

QTEST_MAIN(TestDragDrop)
#include "tst_dragdrop.moc"
