import QtQuick
import Fuin
// Toggle: the knob is a tomoe that spins when switched; "on" track = chakra gradient.
Item {
  id: root
  property bool checked: false
  property bool enabled2: true
  signal toggled(bool value)
  implicitWidth: 46; implicitHeight: 26
  opacity: enabled2 ? 1 : 0.45
  activeFocusOnTab: true
  Keys.onSpacePressed: flip()
  function flip() { if (enabled2) root.toggled(!root.checked) }
  Rectangle {
    anchors.fill: parent
    radius: height / 2
    color: Theme.containerHighest
    border.width: root.checked ? 0 : 1.5
    border.color: root.activeFocus ? Theme.primary : Theme.outline
    Rectangle {
      anchors.fill: parent
      radius: parent.radius
      opacity: root.checked ? 1 : 0
      Behavior on opacity { Anim {} }
      gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0; color: Theme.primary }
        GradientStop { position: 1; color: Theme.tertiary }
      }
    }
  }
  Rectangle {
    width: root.checked ? 20 : 16; height: width; radius: width / 2
    anchors.verticalCenter: parent.verticalCenter
    x: root.checked ? root.width - width - 3 : 5
    color: root.checked ? Theme.surface : Theme.outline
    Behavior on x { NumberAnimation { duration: Theme.dur(260); easing.type: Easing.OutBack; easing.overshoot: 1.4 } }
    Behavior on width { Anim {} }
    Tomoe {
      anchors.centerIn: parent
      width: parent.width - 4; height: width
      tint: root.checked ? Theme.primary : Theme.containerHighest
      rotation: root.checked ? 360 : 0
      Behavior on rotation { NumberAnimation { duration: Theme.dur(420); easing.type: Easing.OutCubic } }
    }
  }
  MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.flip() }
}
