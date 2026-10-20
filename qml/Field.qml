import QtQuick
import QtQuick.Controls
import Archivos
// Single-line text field.
TextField {
  id: root
  property string icon: ""
  property bool mono: false
  implicitHeight: 36
  leftPadding: icon !== "" ? 36 : 12
  rightPadding: 12
  color: Theme.textMain
  placeholderTextColor: Theme.outline
  selectionColor: Theme.primary
  selectedTextColor: Theme.textOnPrimary
  font.family: mono ? Theme.fontMono : Theme.fontSans
  font.pointSize: 11
  verticalAlignment: TextInput.AlignVCenter
  selectByMouse: true
  background: Rectangle {
    radius: 12
    color: Theme.containerHigh
    border.width: root.activeFocus ? 1.5 : 1
    border.color: root.activeFocus ? Theme.primary : Theme.outlineVariant
    Behavior on border.color { CAnim {} }
    Icon { visible: root.icon !== ""; text: root.icon; color: root.activeFocus ? Theme.primary : Theme.textDim; x: 10; anchors.verticalCenter: parent.verticalCenter; size: 14 }
  }
}
