import QtQuick
import QtQuick.Controls
import Fuin
// Settings view: section rail on the left, page on the right. The raw config file stays one click away.
Popup {
  id: root
  property string section: "apps"
  modal: true
  focus: true
  anchors.centerIn: Overlay.overlay
  width: Math.min(parent ? parent.width - 60 : 900, 900)
  height: Math.min(parent ? parent.height - 60 : 640, 640)
  padding: 18
  closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
  Overlay.modal: Rectangle { color: Qt.alpha(Theme.scrim, 0.45) }
  enter: Transition { ParallelAnimation {
    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dur(140) }
    NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.dur(180); easing.type: Easing.OutCubic } } }
  exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.dur(100) } }
  background: ChakraFrame { radius: 28; fill: Theme.containerHigh; ringOpacity: 0.55; borderWidth: 1 }

  function show(mime) {
    section = "apps"
    open()
    if (mime) appsPage.focusMime(mime)
    else { appsPage.ensureLoaded(); Qt.callLater(() => appsPage.focusSearch()) }
  }

  contentItem: Item {
    Column {
      id: rail
      width: 228
      height: parent.height
      spacing: 4
      Row {
        spacing: 10; height: 40
        Icon { text: "settings"; color: Theme.primary; size: 18; anchors.verticalCenter: parent.verticalCenter }
        Txt { text: "Ajustes"; font.pointSize: 14; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
      }
      Repeater {
        model: [{ value: "apps", label: "Aplicaciones por defecto", icon: "apps" }]
        Rectangle {
          id: nav
          required property var modelData
          readonly property bool on: root.section === modelData.value
          width: rail.width - 8; height: 40; radius: 20
          color: on ? Theme.secondaryContainer : "transparent"
          Row {
            x: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 10
            Icon { text: nav.modelData.icon; size: 14; fill: nav.on ? 1 : 0; color: nav.on ? Theme.textOnSecondaryContainer : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
            Txt { text: nav.modelData.label; font.pointSize: 10.5; color: nav.on ? Theme.textOnSecondaryContainer : Theme.textMain; anchors.verticalCenter: parent.verticalCenter }
          }
          StateLayer { radius: 20; onClicked: root.section = nav.modelData.value }
        }
      }
    }
    Column {
      anchors.left: rail.left; anchors.bottom: parent.bottom
      width: rail.width - 8
      spacing: 6
      Txt { width: parent.width; dim: true; font.pointSize: 8.5; elide: Text.ElideMiddle; text: Config.configFile }
      Btn { kind: "outline"; compact: true; icon: "edit_document"; text: "Abrir " + Config.configFile.split("/").pop()
            onClicked: { FileOps.openInEditor(Config.configFile); root.close() } }
    }
    Rectangle { id: divider; x: rail.width + 8; width: 1; height: parent.height; color: Theme.outlineVariant }
    IconBtn { anchors.right: parent.right; y: 0; size: 32; icon: "close"; tip: "Cerrar (Esc)"; onClicked: root.close() }
    DefaultAppsPage {
      id: appsPage
      visible: root.section === "apps"
      anchors.left: divider.right; anchors.leftMargin: 18
      anchors.right: parent.right; anchors.rightMargin: 40
      anchors.top: parent.top; anchors.bottom: parent.bottom
    }
  }
}
