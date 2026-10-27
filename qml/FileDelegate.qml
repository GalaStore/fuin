import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Fuin
import "Glyphs.js" as G

// One entry in grid or list mode: thumbnail/glyph, name, git + secret badges, selection,
// keyboard focus glow, drag source and drop target (folders).
Item {
  id: it
  required property int index
  required property string name
  required property string url
  required property string path
  required property bool isDir
  required property var size
  required property var mtime
  required property string kind
  required property string ext
  required property string gitBranch
  required property int gitDirty
  required property bool isSelected
  required property string thumb
  required property bool isHidden
  required property bool isSecret
  required property string location
  property var pane
  property bool gridMode: true
  property bool compact: false     // columns view: name only
  readonly property real nameW: compact ? width - 50 : pane.colName
  readonly property bool isCurrent: pane.model.currentIndex === index && pane.viewFocused
  readonly property bool cut: FileOps.clipboardCut && FileOps.clipboardUrls.indexOf(url) >= 0
  readonly property color glyphColor: Theme[G.tone(kind, isDir, gitBranch !== "")]

  width: gridMode ? pane.cellW : ListView.view.width
  height: gridMode ? pane.cellH : 34

  // ---- background + chakra glow on the keyboard-current item
  RectangularShadow {
    anchors.fill: bg
    visible: it.isCurrent
    radius: bg.radius
    blur: 14
    spread: 0
    color: Qt.alpha(Theme.primary, 0.45)
  }
  Rectangle {
    id: bg
    anchors.fill: parent
    anchors.margins: it.gridMode ? 4 : 1
    radius: it.gridMode ? 14 : 9
    color: dropZone.containsDrag ? Qt.alpha(Theme.primary, 0.22) : it.isSelected ? Theme.secondaryContainer : "transparent"
    border.width: it.isCurrent || dropZone.containsDrag ? 1.5 : 0
    border.color: dropZone.containsDrag ? Theme.primary : Qt.alpha(Theme.primary, 0.75)
    Behavior on color { CAnim { ms: 90 } }
    Rectangle { anchors.fill: parent; radius: parent.radius; color: Theme.textMain; opacity: ma.containsMouse && !it.isSelected ? 0.06 : 0; Behavior on opacity { Anim { ms: 120 } } }
  }

  component Visual: Item {
    id: vis
    property real s: 64
    width: s; height: s
    Image {
      id: img
      anchors.fill: parent
      visible: status === Image.Ready
      asynchronous: true
      cache: true
      fillMode: Image.PreserveAspectFit
      sourceSize: Qt.size(Math.ceil(vis.s * 1.5), Math.ceil(vis.s * 1.5))
      source: it.thumb
      opacity: visible ? 1 : 0
      Behavior on opacity { Anim { ms: 180 } }
    }
    Rectangle {
      visible: img.visible && it.kind === "video"
      anchors.centerIn: parent; width: vis.s * 0.32; height: width; radius: width / 2
      color: Qt.alpha(Theme.scrim, 0.55)
      Icon { anchors.centerIn: parent; text: "play_arrow"; fill: 1; color: "white"; size: Math.max(6, vis.s * 0.14) }
    }
    Item {
      anchors.fill: parent
      visible: !img.visible
      Icon {
        anchors.centerIn: parent
        text: G.icon(it.kind, it.ext, it.name, it.isDir)
        fill: it.isDir ? 1 : 0
        size: Math.max(8, vis.s * 0.55)
        color: it.glyphColor
      }
      Txt {
        visible: !it.isDir && it.gridMode && vis.s >= 56 && it.ext !== ""
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: vis.s * 0.06
        text: it.ext.toUpperCase().slice(0, 5)
        mono: true; font.pointSize: 6.5; font.weight: Font.Bold; color: Theme.textDim
      }
    }
  }

  component Badge: Rectangle {
    property string label
    property string glyph
    property color bgc: Theme.tertiaryContainer
    property color fgc: Theme.textOnTertiaryContainer
    height: 18; radius: 9
    width: Math.min(it.width - 16, br.implicitWidth + 12)
    color: bgc
    Row { id: br; anchors.centerIn: parent; spacing: 3
      Icon { visible: glyph !== ""; text: glyph; size: 8; color: fgc; anchors.verticalCenter: parent.verticalCenter }
      Txt { visible: label !== ""; text: label; mono: true; font.pointSize: 7; color: fgc; elide: Text.ElideRight; width: Math.min(implicitWidth, it.width - 44) } }
  }

  // ---- grid
  Column {
    visible: it.gridMode
    anchors.horizontalCenter: parent.horizontalCenter
    y: 8
    spacing: 4
    opacity: it.cut ? 0.45 : it.isHidden ? 0.7 : 1
    Visual { s: it.pane.iconSize * 0.86; anchors.horizontalCenter: parent.horizontalCenter }
    Txt {
      width: it.width - 14
      horizontalAlignment: Text.AlignHCenter
      text: it.name
      wrapMode: Text.Wrap
      maximumLineCount: 2
      elide: Text.ElideMiddle
      font.pointSize: 9.5
      color: it.isSelected ? Theme.textOnSecondaryContainer : Theme.textMain
    }
    Txt {
      visible: it.location !== ""
      width: it.width - 14; horizontalAlignment: Text.AlignHCenter
      text: it.location
      elide: Text.ElideLeft; dim: true; font.pointSize: 8; mono: true
    }
  }
  Row {
    visible: it.gridMode
    anchors.right: parent.right; anchors.rightMargin: 8; anchors.top: parent.top; anchors.topMargin: 8
    spacing: 4
    Badge { visible: it.isSecret; glyph: "key"; label: ""; bgc: Theme.errorContainer; fgc: Theme.textOnErrorContainer; width: 22 }
    Badge { visible: it.gitBranch !== ""; glyph: "commit"; label: it.gitBranch + (it.gitDirty > 0 ? " ●" + it.gitDirty : "") }
  }

  // ---- list
  Row {
    visible: !it.gridMode
    anchors.fill: parent
    anchors.leftMargin: 10
    spacing: 0
    opacity: it.cut ? 0.45 : it.isHidden ? 0.7 : 1
    Item { width: 30; height: parent.height; Visual { s: 22; anchors.centerIn: parent } }
    Item {
      width: it.nameW; height: parent.height
      Row {
        anchors.verticalCenter: parent.verticalCenter
        x: 6; spacing: 8
        Txt { text: it.name; font.pointSize: 10; width: Math.min(implicitWidth, it.nameW - 16 - (lb.visible ? lb.width + 8 : 0) - (sb.visible ? 26 : 0)); elide: Text.ElideMiddle
              color: it.isSelected ? Theme.textOnSecondaryContainer : Theme.textMain }
        Badge { id: lb; visible: it.gitBranch !== ""; glyph: "commit"; label: it.gitBranch + (it.gitDirty > 0 ? " ●" + it.gitDirty : ""); anchors.verticalCenter: parent.verticalCenter }
        Badge { id: sb; visible: it.isSecret; glyph: "key"; label: ""; bgc: Theme.errorContainer; fgc: Theme.textOnErrorContainer; width: 22; anchors.verticalCenter: parent.verticalCenter }
      }
    }
    Txt { visible: !it.compact; width: it.pane.colSize; height: parent.height; text: it.isDir ? "—" : G.size(it.size); dim: true; mono: true; font.pointSize: 9; horizontalAlignment: Text.AlignRight; rightPadding: 16 }
    Txt { visible: !it.compact; width: it.pane.colDate; height: parent.height; text: G.date(it.mtime); dim: true; font.pointSize: 9.5 }
    Txt { visible: !it.compact; width: it.pane.colType; height: parent.height; elide: Text.ElideRight; dim: true; font.pointSize: 9.5
          text: it.location !== "" ? it.location : it.isDir ? qsTr("Folder") : (it.ext !== "" ? it.ext.toUpperCase() : qsTr("File")) }
  }

  ToolTip.visible: it.isSecret && ma.containsMouse
  ToolTip.delay: 700
  ToolTip.text: qsTr("Possible secret: it is not included in the AI context nor shared with agents")

  // ---- interaction
  Item {
    id: proxy
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction | Qt.MoveAction | Qt.LinkAction
    Drag.proposedAction: Qt.MoveAction
    Drag.source: it.pane
  }
  MouseArea {
    id: ma
    anchors.fill: parent
    hoverEnabled: true
    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
    drag.target: proxy
    drag.threshold: 8
    drag.onActiveChanged: {
      if (drag.active) proxy.Drag.mimeData = { "text/uri-list": it.pane.dragUris(it.url).join("\r\n") }
      proxy.Drag.active = drag.active
    }
    onPressed: e => {
      it.pane.focusView()
      if (e.button === Qt.RightButton) {
        if (!it.isSelected) it.pane.model.selectOnly(it.index)
        const p = mapToItem(it.pane, e.x, e.y)
        it.pane.contextMenu(p.x, p.y, true)
      } else if (e.button === Qt.LeftButton && !it.isSelected && !(e.modifiers & (Qt.ControlModifier | Qt.ShiftModifier))) {
        it.pane.model.selectOnly(it.index)
      }
    }
    onClicked: e => {
      if (e.button === Qt.MiddleButton) { if (it.isDir) it.pane.win.newTab(it.url); return }
      if (e.button !== Qt.LeftButton) return
      if (e.modifiers & Qt.ShiftModifier) it.pane.model.selectRange(-1, it.index)
      else if (e.modifiers & Qt.ControlModifier) it.pane.model.toggle(it.index)
      else it.pane.model.selectOnly(it.index)
    }
    onDoubleClicked: e => { if (e.button === Qt.LeftButton) it.pane.openIndex(it.index) }
  }
  DropArea {
    id: dropZone
    anchors.fill: parent
    enabled: it.isDir
    keys: ["text/uri-list"]
    onEntered: d => { if (it.isSelected && d.source === it.pane) d.accepted = false }
    onDropped: d => it.pane.handleDrop(d, it.url)
  }
}
