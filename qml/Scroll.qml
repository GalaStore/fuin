import QtQuick
import QtQuick.Controls
import Fuin
ScrollBar {
  id: root
  implicitWidth: 10
  implicitHeight: 10
  padding: 2
  contentItem: Rectangle {
    implicitWidth: 6; implicitHeight: 6
    radius: 3
    color: root.pressed ? Theme.primary : Theme.outline
    opacity: root.policy === ScrollBar.AlwaysOn || (root.active && root.size < 1.0) ? 0.7 : 0
    Behavior on opacity { Anim { ms: 250 } }
  }
}
