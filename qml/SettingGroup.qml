import QtQuick
import Fuin
// One setting: label, optional hint and the control(s) below.
Column {
  id: root
  property string label
  property string hint
  default property alias content: body.data
  width: parent ? parent.width : 0
  spacing: 6
  Txt { text: root.label; font.pointSize: 10.5; font.weight: Font.DemiBold }
  Txt { visible: root.hint !== ""; width: parent.width; text: root.hint; dim: true; font.pointSize: 8.5; wrapMode: Text.Wrap }
  Column { id: body; width: parent.width; spacing: 6 }
}
