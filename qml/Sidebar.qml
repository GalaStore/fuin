import QtQuick
import QtQuick.Controls
import Fuin

// Places · Proyectos · Discos (Solid, with usage bars) · Red · Papelera. Every entry is a drop target.
Rectangle {
  id: root
  property var win
  readonly property string currentUrl: win && win.pane ? win.pane.model.url : ""
  color: Theme.containerLowest

  Flickable {
    anchors.fill: parent
    anchors.topMargin: 8
    contentHeight: col.height + 70
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: Scroll {}
    Column {
      id: col
      x: 10; width: root.width - 20
      spacing: 1
      Repeater {
        model: Places
        Column {
          id: entry
          required property int index
          required property string section
          required property string name
          required property string url
          required property string icon
          required property real usage
          required property string detail
          required property bool mounted
          required property bool removable
          required property string udi
          readonly property bool firstOfSection: index === 0 || Places.sectionAt(index - 1) !== section
          readonly property bool on: url !== "" && root.currentUrl === url
          width: col.width
          Txt {
            visible: entry.firstOfSection && entry.section !== qsTranslate("PlacesModel", "Other")
            text: entry.section; dim: true; font.pointSize: 8.5; font.weight: Font.DemiBold
            leftPadding: 14; topPadding: 12; bottomPadding: 4; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6
          }
          Item { visible: entry.firstOfSection && entry.section === qsTranslate("PlacesModel", "Other"); width: 1; height: 8 }
          Rectangle {
            id: pl
            width: col.width; height: entry.usage >= 0 ? 46 : 36; radius: 12
            color: dz.containsDrag ? Qt.alpha(Theme.primary, 0.22) : entry.on ? Theme.secondaryContainer : "transparent"
            Behavior on color { CAnim { ms: 100 } }
            ChakraLine { vertical: true; width: 3; height: entry.on ? 16 : 0; x: 2; anchors.verticalCenter: parent.verticalCenter; Behavior on height { Anim {} } }
            Icon { id: pi; x: 14; y: (entry.usage >= 0 ? 10 : (parent.height - height) / 2); text: entry.icon; size: 13; fill: entry.on ? 1 : 0
                   color: entry.on ? Theme.textOnSecondaryContainer : Theme.textDim; width: 18 }
            Txt { anchors.left: pi.right; anchors.leftMargin: 10; anchors.right: ab.visible ? ab.left : parent.right; anchors.rightMargin: 8
                  y: entry.usage >= 0 ? 6 : (parent.height - height) / 2
                  text: entry.name; elide: Text.ElideRight; font.pointSize: 10; opacity: entry.mounted ? 1 : 0.6
                  color: entry.on ? Theme.textOnSecondaryContainer : Theme.textMain; font.weight: entry.on ? Font.DemiBold : Font.Normal }
            Rectangle {
              visible: entry.usage >= 0
              anchors.left: pi.right; anchors.leftMargin: 10; anchors.right: parent.right; anchors.rightMargin: 14
              y: 32; height: 4; radius: 2; color: Theme.containerHighest
              Rectangle { width: parent.width * Math.max(0, Math.min(1, entry.usage)); height: parent.height; radius: 2
                gradient: Gradient { orientation: Gradient.Horizontal
                  GradientStop { position: 0; color: entry.usage > 0.9 ? Theme.error : Theme.primary }
                  GradientStop { position: 1; color: entry.usage > 0.9 ? Theme.error : Theme.tertiary } } }
            }
            StateLayer {
              id: sl
              onClicked: {
                if (!entry.mounted && entry.udi !== "") { Places.mount(entry.udi); return }
                root.win.pane.navigate(entry.url)
              }
            }
            IconBtn { id: ab; visible: entry.udi !== "" && (entry.removable && entry.mounted || !entry.mounted); icon: entry.mounted ? "eject" : "power_settings_new"
                      tip: entry.mounted ? qsTr("Eject") : qsTr("Mount"); size: 26; anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter
                      onClicked: entry.mounted ? Places.unmount(entry.udi) : Places.mount(entry.udi) }
            ToolTip.visible: entry.detail !== "" && sl.containsMouse
            ToolTip.delay: 700
            ToolTip.text: entry.detail
            DropArea { id: dz; anchors.fill: parent; keys: ["text/uri-list"]; enabled: entry.url !== "" && entry.url.indexOf("remote:") !== 0 && entry.url.indexOf("recentlyused:") !== 0
              onDropped: d => { FileOps.drop((d.urls || []).map(u => String(u)), entry.url, "auto"); d.accept(Qt.CopyAction) } }
          }
        }
      }
    }
  }
  // brand
  Row {
    anchors.bottom: parent.bottom; anchors.bottomMargin: 14; x: 20
    spacing: 8
    opacity: 0.55
    Tomoe { width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter }
    Txt { text: "Fūin"; font.pointSize: 9.5; font.weight: Font.DemiBold; dim: true; anchors.verticalCenter: parent.verticalCenter }
  }
  Watermark { kind: "leaf"; width: 120; height: 120; anchors.bottom: parent.bottom; anchors.right: parent.right; anchors.margins: -20; rotation: -30; opacity: 0.04 }
  Connections { target: Places; function onMounted(url) { root.win.pane.navigate(url) } function onMessage(text, kind) { root.win.toast(text, kind) } }
}
