import QtQuick
import QtQuick.Effects
import Archivos
// Tinted tomoe glyph.
Image {
  id: root
  property color tint: Theme.primary
  width: 16; height: 16
  source: "qrc:/assets/tomoe.svg"
  sourceSize: Qt.size(Math.max(32, width * 2), Math.max(32, height * 2))
  smooth: true
  layer.enabled: true
  layer.effect: MultiEffect { colorization: 1; colorizationColor: root.tint }
}
