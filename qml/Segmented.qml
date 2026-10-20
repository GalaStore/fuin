import QtQuick
import Archivos
// Segmented control. options: [{ value, label, icon }]
Rectangle {
  id: root
  property var options: []
  property var value
  signal picked(var value)
  implicitHeight: 32
  implicitWidth: segs.implicitWidth + 6
  radius: height / 2
  color: Theme.containerHigh
  Row {
    id: segs
    anchors.centerIn: parent
    spacing: 2
    Repeater {
      model: root.options
      Rectangle {
        id: seg
        required property var modelData
        readonly property bool on: root.value === modelData.value
        height: root.height - 6
        width: segRow.implicitWidth + 22
        radius: height / 2
        color: on ? Theme.secondaryContainer : "transparent"
        Behavior on color { CAnim {} }
        Row {
          id: segRow
          anchors.centerIn: parent
          spacing: 5
          Tomoe { visible: seg.on; width: 12; height: 12; anchors.verticalCenter: parent.verticalCenter }
          Icon { visible: !!seg.modelData.icon && !seg.on; text: seg.modelData.icon || ""; size: 12; color: Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
          Txt { text: seg.modelData.label; font.pointSize: 10; font.weight: seg.on ? Font.DemiBold : Font.Normal; color: seg.on ? Theme.textOnSecondaryContainer : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
        }
        StateLayer { onClicked: root.picked(seg.modelData.value) }
      }
    }
  }
}
