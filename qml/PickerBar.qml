import QtQuick
import Fuin

// Bottom bar of a window opened as the system file chooser (`fuin --portal`): file name, filter, Cancel / Accept.
Rectangle {
  id: bar
  required property var win
  required property var request    // see FileChooserPortal::begin()
  readonly property var pane: win.pane
  readonly property bool saving: request.mode !== "open"
  property alias name: nameField.text
  property int filterIndex: request.currentFilter >= 0 ? request.currentFilter : (request.filters.length ? 0 : -1)
  readonly property var patterns: filterIndex >= 0 ? request.filters[filterIndex].patterns : []
  // Picked folder for directory pickers and SaveFiles: the single selected folder, else the one shown.
  readonly property string targetFolder: {
    if (!pane) return ""
    const s = pane.current
    return s && s.isDir && pane.model.selectionCount === 1 ? s.url : pane.model.url
  }
  property var chosenFiles: []
  readonly property bool canAccept: {
    if (!pane || !pane.model.isLocal) return false
    if (request.mode === "save") return validName(name)
    if (request.mode === "saveFiles" || request.directory) return true
    return chosenFiles.length > 0 && (request.multiple || chosenFiles.length === 1)
  }
  signal picked(bool accepted, var urls, int filter)

  height: 58
  color: Theme.containerLow

  function refreshChosen() { chosenFiles = pane ? pane.model.selectedItems().filter(e => !e.isDir).map(e => e.url) : [] }
  onPaneChanged: refreshChosen()
  function validName(n) { return n.trim() !== "" && n.indexOf("/") < 0 && n !== "." && n !== ".." }
  function saveUrl() { return AppInfo.toUrl(pane.model.localPath.replace(/\/$/, "") + "/" + name.trim()) }
  function accept() {
    if (!canAccept) return
    if (request.mode === "save") {
      const u = saveUrl()
      if (Portal.exists(u)) { win.confirmOverwrite(name.trim(), () => picked(true, [u], filterIndex)); return }
      picked(true, [u], filterIndex)
    } else if (request.mode === "saveFiles" || request.directory) {
      picked(true, [targetFolder], filterIndex)
    } else {
      picked(true, chosenFiles, filterIndex)
    }
  }
  // Enter / double-click on a file in the view.
  function activate(entry) {
    if (request.mode === "save") { name = entry.name; accept() }
    else if (request.mode === "open" && !request.directory && pane.model.isLocal)
      chosenFiles.indexOf(entry.url) < 0 ? picked(true, [entry.url], filterIndex) : accept()
  }
  function focusName() { nameField.forceActiveFocus(); const dot = name.lastIndexOf("."); nameField.select(0, dot > 0 ? dot : name.length) }

  Connections {
    target: bar.pane ? bar.pane.model : null
    function onSelectionChanged() { bar.refreshChosen() }
    function onLocationChanged() { bar.refreshChosen() }
  }
  Connections {
    target: bar.pane
    enabled: bar.request.mode === "save"
    function onCurrentChanged() { const c = bar.pane.current; if (c && !c.isDir) bar.name = c.name }
  }

  Rectangle { width: parent.width; height: 1; color: Theme.outlineVariant }
  Row {
    x: 18; spacing: 10; anchors.verticalCenter: parent.verticalCenter
    Field {
      id: nameField
      visible: bar.request.mode === "save"
      width: Math.min(420, bar.width * 0.4)
      icon: "edit"
      placeholderText: qsTr("File name")
      text: bar.request.name
      Keys.onReturnPressed: bar.accept()
      Keys.onEnterPressed: bar.accept()
      Keys.onDownPressed: bar.pane.focusView()
    }
    Btn {
      id: filterBtn
      visible: bar.request.filters.length > 0
      kind: "outline"; icon: "filter_list"
      text: bar.filterIndex >= 0 ? bar.request.filters[bar.filterIndex].name : ""
      onClicked: {
        const p = filterBtn.mapToItem(bar.win.contentItem, 0, 0)
        bar.win.menu.openAt(p.x, p.y - 8 - 40 * bar.request.filters.length,
          bar.request.filters.map((f, i) => ({ label: f.name, icon: i === bar.filterIndex ? "check" : "", run: () => bar.filterIndex = i })))
      }
    }
    Txt {
      visible: bar.request.mode === "saveFiles"
      anchors.verticalCenter: parent.verticalCenter; dim: true; font.pointSize: 9.5
      text: qsTr("Saving %n file(s) in the selected folder", "", bar.request.files.length)
    }
    Txt {
      visible: !!bar.pane && !bar.pane.model.isLocal
      anchors.verticalCenter: parent.verticalCenter; color: Theme.error; font.pointSize: 9.5
      text: qsTr("Only local folders can be picked")
    }
  }
  Row {
    anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; spacing: 8
    Btn { kind: "text"; text: qsTr("Cancel"); onClicked: bar.picked(false, [], -1) }
    Btn {
      kind: "filled"; enabled2: bar.canAccept
      icon: bar.saving ? "save" : bar.request.directory ? "folder_open" : "check"
      text: bar.request.acceptLabel ? bar.request.acceptLabel.replace(/_(.)/g, "$1")
            : bar.saving ? qsTr("Save") : bar.request.directory ? qsTr("Select folder") : qsTr("Open")
      onClicked: bar.accept()
    }
  }
}
