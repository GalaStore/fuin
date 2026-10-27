import QtQuick
import QtQuick.Controls
import Fuin

// Breadcrumbs that turn into an editable path (click empty space or Ctrl+L). Tab completes.
Rectangle {
  id: root
  property var pane
  property bool editing: false
  readonly property var fm: pane ? pane.model : null
  height: 36
  radius: 18
  color: Theme.containerHigh
  border.width: editing ? 1.5 : 1
  border.color: editing ? Theme.primary : Theme.outlineVariant
  Behavior on border.color { CAnim {} }
  function edit() { editing = true; field.text = fm.isLocal ? fm.path : fm.url; field.forceActiveFocus(); field.selectAll() }
  readonly property var crumbs: fm ? (fm.url, fm.crumbs()) : []

  Flickable {
    id: fl
    visible: !root.editing
    anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: gitChip.visible ? gitChip.width + 10 : 6
    contentWidth: crow.width
    clip: true
    interactive: contentWidth > width
    onContentWidthChanged: contentX = Math.max(0, contentWidth - width)
    MouseArea { width: Math.max(fl.width, crow.width); height: fl.height; onClicked: root.edit(); cursorShape: Qt.IBeamCursor }
    Row {
      id: crow
      height: fl.height
      Repeater {
        model: root.crumbs
        Row {
          id: cr
          required property var modelData
          required property int index
          readonly property bool last: index === root.crumbs.length - 1
          height: crow.height
          Icon { visible: cr.index > 0; text: "chevron_right"; size: 11; color: Theme.outline; anchors.verticalCenter: parent.verticalCenter }
          Rectangle {
            height: 28; radius: 14
            width: cl.implicitWidth + 18
            anchors.verticalCenter: parent.verticalCenter
            color: dropC.containsDrag ? Qt.alpha(Theme.primary, 0.25) : "transparent"
            Row {
              id: cl
              anchors.centerIn: parent; spacing: 5
              Icon { visible: !!cr.modelData.icon; text: cr.modelData.icon || ""; size: 11; color: cr.last ? Theme.primary : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
              Txt { visible: !cr.modelData.icon || cr.modelData.label !== "/"; text: cr.modelData.label; font.pointSize: 10; font.weight: cr.last ? Font.DemiBold : Font.Normal
                    color: cr.last ? Theme.textMain : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
            }
            StateLayer { radius: 14; onClicked: root.pane.navigate(cr.modelData.url) }
            DropArea { id: dropC; anchors.fill: parent; keys: ["text/uri-list"]; onDropped: d => root.pane.handleDrop(d, cr.modelData.url) }
          }
        }
      }
    }
  }
  Rectangle {
    id: gitChip
    visible: !root.editing && !!root.fm && root.fm.repoBranch !== ""
    anchors.right: parent.right; anchors.rightMargin: 5; anchors.verticalCenter: parent.verticalCenter
    height: 26; radius: 13; width: gr.implicitWidth + 18
    color: Theme.tertiaryContainer
    Row { id: gr; anchors.centerIn: parent; spacing: 5
      Icon { text: "commit"; size: 10; color: Theme.textOnTertiaryContainer; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: root.fm ? root.fm.repoBranch : ""; mono: true; font.pointSize: 9; color: Theme.textOnTertiaryContainer; anchors.verticalCenter: parent.verticalCenter }
      Txt { visible: !!root.fm && root.fm.repoDirty > 0; text: "●" + (root.fm ? root.fm.repoDirty : 0); mono: true; font.pointSize: 8.5; color: Theme.textOnTertiaryContainer; opacity: 0.8; anchors.verticalCenter: parent.verticalCenter } }
    ToolTip.visible: gh.hovered
    ToolTip.text: root.fm ? qsTr("Git repository: %1").arg(AppInfo.prettyPath(root.fm.repoRoot)) + " · " + (root.fm.repoDirty > 0 ? qsTr("%1 uncommitted changes").arg(root.fm.repoDirty) : qsTr("clean")) : ""
    HoverHandler { id: gh }
  }
  TextField {
    id: field
    visible: root.editing
    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
    color: Theme.textMain
    font.family: Theme.fontMono; font.pointSize: 10
    selectionColor: Theme.primary; selectedTextColor: Theme.textOnPrimary
    verticalAlignment: TextInput.AlignVCenter
    background: null
    placeholderText: qsTr("Path or URL (~/Dev, sftp://server/, smb://, trash:/ …)")
    placeholderTextColor: Theme.outline
    onAccepted: { root.pane.navigate(text.trim()); root.editing = false }
    Keys.onEscapePressed: { root.editing = false; root.pane.focusView() }
    onActiveFocusChanged: if (!activeFocus) root.editing = false
    Keys.onTabPressed: e => { const c = completer.complete(text); if (c) text = c; e.accepted = true }
  }
  QtObject {
    id: completer
    function complete(t) {
      // local completion of the last path component
      const home = AppInfo.homePath
      const full = t.startsWith("~") ? home + t.slice(1) : t
      const slash = full.lastIndexOf("/")
      if (slash < 0) return ""
      const dir = full.slice(0, slash) || "/"
      const prefix = full.slice(slash + 1)
      const names = AppInfo.childDirs(dir, prefix)
      if (!names.length) return ""
      let common = names[0]
      for (const n of names) { let i = 0; while (i < common.length && common[i] === n[i]) i++; common = common.slice(0, i) }
      const out = (dir === "/" ? "" : dir) + "/" + common + (names.length === 1 ? "/" : "")
      return out.startsWith(home) ? "~" + out.slice(home.length) : out
    }
  }
}
