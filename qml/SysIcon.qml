import QtQuick
import Fuin
// Freedesktop theme icon (application / MIME type) with a Material glyph fallback.
Item {
  id: root
  property string name: ""
  property string glyph: "apps"
  property real size: 20
  property color color: Theme.textDim
  readonly property bool themed: name !== "" && DefaultApps.hasIcon(name)
  width: size; height: size
  Image {
    anchors.fill: parent
    visible: root.themed
    source: root.themed ? "image://icon/" + encodeURIComponent(root.name) : ""
    sourceSize: Qt.size(Math.ceil(root.size * 2), Math.ceil(root.size * 2))
    asynchronous: true
    smooth: true
  }
  Icon { anchors.centerIn: parent; visible: !root.themed; text: root.glyph; size: root.size * 0.7; color: root.color }
}
