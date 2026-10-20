import QtQuick
import Fuin
// Body text (rounded Google Sans Flex when installed, system sans otherwise).
Text {
  property bool dim: false
  property bool mono: false
  color: dim ? Theme.textDim : Theme.textMain
  font.family: mono ? Theme.fontMono : Theme.fontSans
  font.pointSize: 13
  font.variableAxes: !mono && Theme.sansHasRond ? ({ "ROND": 25 }) : ({})
  renderType: Text.NativeRendering
  textFormat: Text.PlainText
  verticalAlignment: Text.AlignVCenter
}
