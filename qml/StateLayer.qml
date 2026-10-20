import QtQuick
import Fuin
// M3 hover/press tint.
MouseArea {
  id: root
  property color tint: Theme.textMain
  property real radius: parent?.radius ?? 0
  property bool active: false
  anchors.fill: parent
  hoverEnabled: true
  cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
  Rectangle {
    anchors.fill: parent
    radius: root.radius
    color: root.tint
    opacity: root.pressed ? 0.14 : root.containsMouse ? 0.08 : root.active ? 0.05 : 0
    Behavior on opacity { Anim { ms: 120 } }
  }
}
