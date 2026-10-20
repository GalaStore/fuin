import QtQuick
import Fuin
// Material Symbols glyph (ligature name as text).
Text {
  property real fill: 0
  property real size: 18
  color: Theme.textMain
  font.family: Theme.fontIcon
  font.pointSize: size
  font.variableAxes: ({ "FILL": fill, "GRAD": Theme.light ? 0 : -25, "opsz": 24 })
  renderType: Text.NativeRendering
  horizontalAlignment: Text.AlignHCenter
  verticalAlignment: Text.AlignVCenter
}
