import QtQuick
import Archivos
// Rounded card with a thin chakra-gradient ring (primary → tertiary → secondary).
Item {
  id: root
  property real radius: 16
  property real borderWidth: 1.5
  property color fill: Theme.containerLow
  property real ringOpacity: 0.85
  property bool ring: true
  default property alias content: body.data
  Rectangle {
    anchors.fill: parent
    radius: root.radius
    opacity: root.ring ? root.ringOpacity : 0
    visible: opacity > 0
    Behavior on opacity { Anim { ms: 250 } }
    gradient: Gradient {
      orientation: Gradient.Horizontal
      GradientStop { position: 0; color: Theme.primary }
      GradientStop { position: 0.55; color: Theme.tertiary }
      GradientStop { position: 1; color: Theme.secondary }
    }
  }
  Rectangle {
    id: body
    anchors.fill: parent
    anchors.margins: root.ring ? root.borderWidth : 0
    radius: Math.max(0, root.radius - (root.ring ? root.borderWidth : 0))
    color: root.fill
  }
}
