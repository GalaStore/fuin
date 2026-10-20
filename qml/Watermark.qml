import QtQuick
import QtQuick.Effects
import Fuin
// Faint static emblem (konoha | leaf | tomoe).
Image {
  id: root
  property string kind: "leaf"
  property color tint: Theme.primary
  width: 64; height: 64
  opacity: 0.05
  source: "qrc:/assets/" + kind + ".svg"
  sourceSize: Qt.size(Math.max(32, width), Math.max(32, height))
  fillMode: Image.PreserveAspectFit
  smooth: true
  asynchronous: true
  layer.enabled: true
  layer.effect: MultiEffect { colorization: 1; colorizationColor: root.tint }
}
