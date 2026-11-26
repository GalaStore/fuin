import QtQuick
import "../qml"

Item {
    id: root
    width: 400; height: 200
    property int entered: 0
    property string dropped

    Item {
        id: area
        anchors.fill: parent

        Rectangle {
            id: source
            x: 10; y: 10; width: 100; height: 40
            DragProxy { id: proxy; pane: area; dragType: Drag.Internal }
            MouseArea {
                anchors.fill: parent
                drag.target: proxy
                drag.threshold: 8
                drag.onActiveChanged: {
                    if (!drag.active)
                        proxy.Drag.drop()
                    proxy.Drag.active = drag.active
                }
            }
        }
        Rectangle {
            x: 200; y: 10; width: 100; height: 100
            DropArea {
                anchors.fill: parent
                onEntered: root.entered++
                onDropped: d => { root.dropped = d.source.pane === area ? "pane" : "other"; d.accept(Qt.CopyAction) }
            }
        }
    }
}
