import QtQuick
import QtQuick.Controls
import Archivos
// Centered dialog: title + body + buttons; content goes into the default property.
Popup {
  id: root
  property string title: ""
  property string icon: ""
  default property alias body: col.data
  property alias buttons: btnRow.data
  modal: true
  focus: true
  anchors.centerIn: Overlay.overlay
  width: Math.min(parent ? parent.width - 60 : 520, 520)
  padding: 22
  closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
  Overlay.modal: Rectangle { color: Qt.alpha(Theme.scrim, 0.45) }
  enter: Transition { ParallelAnimation {
    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dur(140) }
    NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.dur(180); easing.type: Easing.OutCubic } } }
  exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.dur(100) } }
  background: ChakraFrame { radius: 28; fill: Theme.containerHigh; ringOpacity: 0.55; borderWidth: 1 }
  contentItem: Column {
    spacing: 14
    Row {
      spacing: 10
      visible: root.title !== ""
      Icon { visible: root.icon !== ""; text: root.icon; color: Theme.primary; size: 18; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: root.title; font.pointSize: 14; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
    }
    Column { id: col; width: parent.width; spacing: 10 }
    Row { id: btnRow; spacing: 8; anchors.right: parent.right }
  }
}
