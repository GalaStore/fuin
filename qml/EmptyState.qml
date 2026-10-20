import QtQuick
import Fuin
// Friendly empty / error / no-results state with a faint emblem.
Column {
  id: root
  property string icon: "folder_open"
  property string emblem: "leaf"
  property string title: ""
  property string hint: ""
  property bool error: false
  spacing: 10
  Item {
    width: 120; height: 96
    anchors.horizontalCenter: parent.horizontalCenter
    Watermark { kind: root.emblem; width: 96; height: 96; anchors.centerIn: parent; opacity: 0.09; rotation: -18; tint: root.error ? Theme.error : Theme.primary }
    Icon { anchors.centerIn: parent; text: root.icon; size: 30; fill: 1; color: root.error ? Theme.error : Theme.primary; opacity: 0.85 }
  }
  Txt { anchors.horizontalCenter: parent.horizontalCenter; text: root.title; font.pointSize: 12.5; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
  Txt { anchors.horizontalCenter: parent.horizontalCenter; width: Math.min(implicitWidth, 380); text: root.hint; dim: true; font.pointSize: 10; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; visible: text !== "" }
}
