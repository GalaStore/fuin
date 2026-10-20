import QtQuick
import QtQuick.Controls
import Archivos
// Round icon button with tooltip.
Rectangle {
  id: root
  property string icon
  property string tip: ""
  property bool checked: false
  property bool enabled2: true
  property real size: 32
  property color fg: checked ? Theme.textOnSecondaryContainer : Theme.textDim
  signal clicked()
  width: size; height: size; radius: size / 2
  color: checked ? Theme.secondaryContainer : "transparent"
  opacity: enabled2 ? 1 : 0.35
  Behavior on color { CAnim {} }
  Icon { anchors.centerIn: parent; text: root.icon; color: root.fg; size: root.size * 0.5; fill: root.checked ? 1 : 0 }
  StateLayer { id: sl; enabled: root.enabled2; onClicked: root.clicked() }
  ToolTip.visible: tip !== "" && sl.containsMouse
  ToolTip.delay: 600
  ToolTip.text: tip
}
