import QtQuick
import QtQuick.Controls
import Fuin
import "Glyphs.js" as G

// Miller columns: up to two ancestor folders (read-only navigation) + the pane's folder.
Item {
  id: root
  property var pane
  readonly property alias currentList: cur
  readonly property var ancestors: {
    const cr = pane.model.crumbs()
    pane.model.url // dependency
    const urls = cr.map(c => c.url)
    return urls.slice(Math.max(0, urls.length - 3), urls.length - 1)
  }
  readonly property real colW: Math.max(200, Math.min(300, width / 3.2))

  Row {
    anchors.fill: parent
    spacing: 6
    Repeater {
      model: root.ancestors
      Rectangle {
        id: col
        required property string modelData
        required property int index
        readonly property string nextUrl: index + 1 < root.ancestors.length ? root.ancestors[index + 1] : root.pane.model.url
        width: root.colW; height: root.height
        radius: 14
        color: Qt.alpha(Theme.containerLow, 0.6)
        FolderModel { id: am; url: col.modelData; showHidden: Config.showHidden; sortField: "name" }
        ListView {
          anchors.fill: parent; anchors.margins: 4
          clip: true
          model: am
          boundsBehavior: Flickable.StopAtBounds
          ScrollBar.vertical: Scroll {}
          delegate: Rectangle {
            required property string name
            required property string url
            required property bool isDir
            required property string kind
            required property string ext
            required property string gitBranch
            readonly property bool onPath: url === col.nextUrl
            width: ListView.view.width; height: 30; radius: 9
            color: onPath ? Theme.secondaryContainer : "transparent"
            opacity: isDir ? 1 : 0.5
            Row { x: 8; anchors.verticalCenter: parent.verticalCenter; spacing: 8
              Icon { text: G.icon(kind, ext, name, isDir); size: 12; fill: isDir ? 1 : 0; color: isDir ? (gitBranch ? Theme.tertiary : Theme.primary) : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
              Txt { text: name; font.pointSize: 9.5; width: root.colW - 60; elide: Text.ElideMiddle; color: parent.parent.onPath ? Theme.textOnSecondaryContainer : Theme.textMain; anchors.verticalCenter: parent.verticalCenter }
            }
            Icon { visible: isDir; anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: "chevron_right"; size: 11; color: Theme.textDim }
            StateLayer { enabled: isDir; onClicked: root.pane.navigate(url) }
          }
          onCountChanged: Qt.callLater(() => { const i = am.indexOfUrl(col.nextUrl); if (i >= 0) positionViewAtIndex(i, ListView.Center) })
        }
      }
    }
    // current folder: full selection behaviour
    Rectangle {
      width: Math.max(root.colW, root.width - root.ancestors.length * (root.colW + 6))
      height: root.height
      radius: 14
      color: "transparent"
      ListView {
        id: cur
        anchors.fill: parent; anchors.margins: 2
        clip: true
        focus: root.visible
        model: root.visible ? root.pane.model : null
        keyNavigationEnabled: false
        reuseItems: root.pane.listing
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: Scroll {}
        Keys.onPressed: e => root.pane.handleKey(e)
        delegate: FileDelegate { pane: root.pane; gridMode: false; compact: true }
      }
    }
  }
}
