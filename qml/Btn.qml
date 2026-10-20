import QtQuick
import Archivos
// Pill button. kind: filled | tonal | outline | text | danger
Rectangle {
  id: root
  property string text: ""
  property string icon: ""
  property string kind: "tonal"
  property bool enabled2: true
  property bool compact: false
  signal clicked()
  implicitHeight: compact ? 30 : 36
  implicitWidth: row.implicitWidth + (compact ? 20 : 28)
  radius: height / 2
  opacity: enabled2 ? 1 : 0.45
  activeFocusOnTab: enabled2
  Keys.onReturnPressed: if (enabled2) clicked()
  Keys.onSpacePressed: if (enabled2) clicked()
  readonly property color fg: kind === "filled" ? Theme.textOnPrimary
    : kind === "danger" ? Theme.textOnError
    : kind === "tonal" ? Theme.textOnSecondaryContainer
    : Theme.primary
  color: kind === "filled" ? Theme.primary : kind === "danger" ? Theme.error : kind === "tonal" ? Theme.secondaryContainer : "transparent"
  border.width: kind === "outline" || activeFocus ? 1 : 0
  border.color: activeFocus ? Theme.primary : Theme.outline
  Row {
    id: row
    anchors.centerIn: parent
    spacing: 6
    Icon { visible: root.icon !== ""; text: root.icon; color: root.fg; size: root.compact ? 13 : 15; anchors.verticalCenter: parent.verticalCenter }
    Txt { visible: root.text !== ""; text: root.text; color: root.fg; font.weight: Font.Medium; font.pointSize: root.compact ? 10 : 11; anchors.verticalCenter: parent.verticalCenter }
  }
  StateLayer { tint: root.fg; enabled: root.enabled2; onClicked: root.clicked() }
}
