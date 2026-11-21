import QtQuick
import Fuin
// Text value of fuin.conf. Empty means automatic (the placeholder shows what that resolves to);
// edits are written when the field loses focus or on Return.
Row {
  id: root
  property string value
  property string placeholder
  property string status: ""      // warning shown next to the field, if any
  property bool mono: true
  signal edited(string value)
  width: parent ? parent.width : 0
  spacing: 6
  onValueChanged: if (!field.activeFocus) field.text = value
  Field {
    id: field
    width: root.width - (reset.visible ? reset.width + root.spacing : 0)
    mono: root.mono
    text: root.value
    placeholderText: root.placeholder
    onEditingFinished: if (text.trim() !== root.value) root.edited(text)
    Keys.onEscapePressed: event => { if (text !== root.value) { text = root.value; event.accepted = true } else event.accepted = false }
    Icon { visible: root.status !== ""; text: "warning"; color: Theme.error; size: 14
           anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter }
  }
  IconBtn { id: reset; visible: root.value !== ""; size: 36; icon: "restart_alt"; tip: qsTr("Use the automatic value"); onClicked: root.edited("") }
  Txt { visible: root.status !== ""; width: root.width; text: root.status; color: Theme.error; font.pointSize: 8.5; wrapMode: Text.Wrap }
}
