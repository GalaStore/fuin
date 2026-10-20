import QtQuick
import Fuin
// Bottom-centre toast. show(text, kind, actionText, actionFn, ms); kind: ok | error | info
Item {
  id: root
  property string text: ""
  property string kind: "ok"
  property string actionText: ""
  property var action: null
  property bool shown: false
  anchors.horizontalCenter: parent.horizontalCenter
  anchors.bottom: parent.bottom
  anchors.bottomMargin: shown ? 44 : -height
  width: Math.min(parent.width - 40, pill.implicitWidth)
  height: pill.implicitHeight
  opacity: shown ? 1 : 0
  z: 1000
  Behavior on anchors.bottomMargin { NumberAnimation { duration: Theme.dur(260); easing.type: Easing.OutCubic } }
  Behavior on opacity { Anim {} }
  function show(t, k, actText, actFn, ms) {
    text = t; kind = k || "ok"; actionText = actText || ""; action = actFn || null
    shown = true
    hideTimer.interval = ms || (kind === "error" ? 9000 : actionText ? 6000 : 2200)
    hideTimer.restart()
  }
  Timer { id: hideTimer; onTriggered: root.shown = false }
  Rectangle {
    id: pill
    width: root.width
    implicitWidth: Math.min(640, content.implicitWidth + 32)
    implicitHeight: Math.max(40, content.implicitHeight + 16)
    radius: Math.min(height / 2, 20)
    color: root.kind === "error" ? Theme.errorContainer : Theme.inverseSurface
    Row {
      id: content
      anchors.verticalCenter: parent.verticalCenter
      x: 16
      spacing: 10
      Tomoe { visible: root.kind === "ok"; width: 16; height: 16; tint: Theme.inversePrimary; anchors.verticalCenter: parent.verticalCenter
        RotationAnimation on rotation { running: root.shown && root.kind === "ok" && Theme.animations; from: 0; to: 360; duration: 700; loops: 1 } }
      Icon { visible: root.kind !== "ok"; text: root.kind === "error" ? "error" : "info"; color: root.kind === "error" ? Theme.textOnErrorContainer : Theme.inverseOnSurface; anchors.verticalCenter: parent.verticalCenter }
      Txt {
        text: root.text
        color: root.kind === "error" ? Theme.textOnErrorContainer : Theme.inverseOnSurface
        wrapMode: Text.Wrap
        width: Math.min(implicitWidth, 460)
        maximumLineCount: 6
        elide: Text.ElideRight
        anchors.verticalCenter: parent.verticalCenter
      }
      Rectangle {
        visible: root.actionText !== ""
        width: at.implicitWidth + 20; height: 28; radius: 14
        color: "transparent"; border.width: 1
        border.color: root.kind === "error" ? Theme.textOnErrorContainer : Theme.inversePrimary
        anchors.verticalCenter: parent.verticalCenter
        Txt { id: at; anchors.centerIn: parent; text: root.actionText; font.weight: Font.DemiBold; color: root.kind === "error" ? Theme.textOnErrorContainer : Theme.inversePrimary }
        StateLayer { onClicked: { root.shown = false; if (root.action) root.action() } }
      }
      Item { width: 4; height: 1 }
    }
    MouseArea { anchors.fill: parent; z: -1; onClicked: root.shown = false }
  }
}
