import QtQuick
import QtQuick.Controls
import Fuin
import "Glyphs.js" as G

// One browsing pane: FolderModel + grid / list / columns views, keyboard navigation,
// type-to-filter, rubber-band selection, drag & drop and the context menu.
FocusScope {
  id: pane
  property var win
  readonly property var paneSelf: pane
  property var tab
  property bool active: true
  property alias model: fm
  property string viewMode: Config.viewMode
  property int iconSize: Config.iconSize
  property var current: null        // single selected item (map) or null
  property bool listing: false      // first fill of a folder; KDirWatch refreshes also toggle fm.loading
  readonly property bool viewFocused: activeView ? activeView.activeFocus : false
  readonly property var activeView: viewMode === "list" ? list : viewMode === "columns" ? columns.currentList : grid
  signal focused()

  // geometry
  readonly property real cellW: { const min = iconSize + 34; const avail = Math.max(min, grid.width - 2); return Math.floor(avail / Math.max(1, Math.floor(avail / min))) }
  readonly property real cellH: iconSize * 0.86 + 62
  readonly property real colSize: 96
  readonly property real colDate: 140
  readonly property real colType: 110
  readonly property real colName: Math.max(160, list.width - 30 - colSize - colDate - colType - 24)

  FolderModel {
    id: fm
    showHidden: Config.showHidden
    sortField: Config.sortField
    sortReversed: Config.sortReversed
    onSelectionChanged: pane.refreshCurrent()
    onLocationChanged: pane.refreshCurrent()
    onRevealRow: row => pane.positionAt(row)
    onLoadingChanged: pane.listing = loading && count === 0
  }
  Component.onCompleted: refreshCurrent()

  onActiveFocusChanged: if (activeFocus) focused()

  function refreshCurrent() { current = fm.selectionCount === 1 ? fm.selectedItems()[0] : null }
  function navigate(u) { fm.navigate(u); focusView() }
  function focusView() { if (activeView) activeView.forceActiveFocus() }
  function positionAt(row) {
    if (row < 0) return
    if (viewMode === "grid") grid.positionViewAtIndex(row, GridView.Contain)
    else if (viewMode === "list") list.positionViewAtIndex(row, ListView.Contain)
    else columns.currentList.positionViewAtIndex(row, ListView.Contain)
  }
  function dragUris(url) {
    const sel = fm.selectedUrls()
    return sel.indexOf(url) >= 0 ? sel : [url]
  }
  function selectedOrCurrent() {
    const s = fm.selectedUrls()
    if (s.length) return s
    return []
  }
  function openIndex(i) {
    const e = fm.get(i)
    if (!e || !e.url) return
    if (e.isDir) navigate(e.url)
    else FileOps.open(e.url)
  }
  function openSelection() {
    const items = fm.selectedItems()
    if (items.length === 1 && items[0].isDir) { navigate(items[0].url); return }
    for (const e of items) if (!e.isDir) FileOps.open(e.url)
  }
  function handleDrop(d, destUrl) {
    const urls = (d.urls || []).map(u => String(u))
    if (!urls.length) return
    FileOps.drop(urls, destUrl, "auto")
    d.accept(Qt.CopyAction)
  }
  function copySel(cut) { const s = fm.selectedUrls(); if (s.length) FileOps.setClipboard(s, cut) }
  function pasteHere() { FileOps.paste(fm.url) }
  function trashSelected() {
    const s = fm.selectedUrls(); if (!s.length) return
    if (fm.isTrash) { win.confirmDelete(s); return }
    FileOps.trash(s)
  }
  function deleteSelected() { const s = fm.selectedUrls(); if (s.length) win.confirmDelete(s) }
  function renameSelected() { if (current) win.askRename(current.url, current.name) }

  // ---------- keyboard
  function moveTo(i, mods) {
    if (fm.count === 0) return
    i = Math.max(0, Math.min(fm.count - 1, i))
    if (mods & Qt.ShiftModifier) fm.selectRange(-1, i)
    else if (mods & Qt.ControlModifier) fm.currentIndex = i
    else fm.selectOnly(i)
    positionAt(i)
  }
  function columnsPerRow() { return Math.max(1, Math.floor(grid.width / cellW)) }
  function handleKey(e) {
    const cur = fm.currentIndex
    const perRow = viewMode === "grid" ? columnsPerRow() : 1
    const page = viewMode === "grid" ? perRow * Math.max(1, Math.floor(grid.height / cellH)) : Math.max(1, Math.floor(list.height / 34))
    switch (e.key) {
    case Qt.Key_Right: if (viewMode === "grid") { moveTo(cur + 1, e.modifiers); e.accepted = true } else if (viewMode === "columns" && current && current.isDir) { navigate(current.url); e.accepted = true } return
    case Qt.Key_Left: if (viewMode === "grid") { moveTo(cur - 1, e.modifiers); e.accepted = true } else if (viewMode === "columns") { fm.up(); e.accepted = true } return
    case Qt.Key_Down: moveTo(cur < 0 ? 0 : cur + perRow, e.modifiers); e.accepted = true; return
    case Qt.Key_Up: moveTo(cur < 0 ? 0 : cur - perRow, e.modifiers); e.accepted = true; return
    case Qt.Key_Home: moveTo(0, e.modifiers); e.accepted = true; return
    case Qt.Key_End: moveTo(fm.count - 1, e.modifiers); e.accepted = true; return
    case Qt.Key_PageDown: moveTo(cur + page, e.modifiers); e.accepted = true; return
    case Qt.Key_PageUp: moveTo(cur - page, e.modifiers); e.accepted = true; return
    case Qt.Key_Return: case Qt.Key_Enter:
      if (fm.selectionCount > 0) openSelection(); else if (cur >= 0) openIndex(cur)
      e.accepted = true; return
    case Qt.Key_Backspace:
      if (fm.filter.length) fm.filter = fm.filter.slice(0, -1); else fm.back()
      e.accepted = true; return
    case Qt.Key_Escape:
      if (fm.filter.length) fm.filter = ""
      else if (fm.searching) fm.searchQuery = ""
      else fm.clearSelection()
      e.accepted = true; return
    case Qt.Key_Space:
      if (!(e.modifiers & Qt.ControlModifier) && fm.filter === "") { win.toggleRightPanel("preview"); e.accepted = true; return }
      if (e.modifiers & Qt.ControlModifier && cur >= 0) { fm.toggle(cur); e.accepted = true; return }
    }
    // type-to-filter (printable, no Ctrl/Alt/Meta)
    if (e.text && e.text.length === 1 && e.text >= " " && !(e.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
      fm.filter = fm.filter + e.text
      e.accepted = true
    }
  }

  // ---------- context menu
  function contextMenu(x, y, onItem) {
    const sel = fm.selectedItems()
    const urls = sel.map(s => s.url)
    const one = sel.length === 1 ? sel[0] : null
    const other = tab ? tab.otherPane(pane) : null
    let items = []
    if (onItem && sel.length) {
      if (fm.isTrash) {
        items.push({ label: qsTr("Restore"), icon: "restore_from_trash", run: () => FileOps.restore(urls) })
        items.push({ label: qsTr("Delete permanently"), icon: "delete_forever", danger: true, shortcut: qsTr("Del"), run: () => win.confirmDelete(urls) })
        win.menu.openAt(mapToItem(win.contentItem, x, y).x, mapToItem(win.contentItem, x, y).y, items)
        return
      }
      items.push({ label: qsTr("Open"), icon: "open_in_new", shortcut: qsTr("Enter"), run: () => openSelection() })
      if (one && !one.isDir) {
        const apps = FileOps.openWithApps(one.mimeType).map(a => ({ label: a.name, icon: "apps", run: () => FileOps.openWith(a.id, urls) }))
        apps.push({ sep: true })
        apps.push({ label: qsTr("Other application…"), icon: "more_horiz", run: () => FileOps.openWithDialog(urls) })
        items.push({ label: qsTr("Open with"), icon: "apps", sub: apps })
      }
      if (one && one.isDir) {
        items.push({ label: qsTr("Open in new tab"), icon: "tab", run: () => win.newTab(one.url) })
        items.push({ label: qsTr("Open in the other pane"), icon: "vertical_split", run: () => win.openInOtherPane(one.url) })
      }
      if (one && one.mimeType)
        items.push({ label: qsTr("Change default app…"), icon: "settings_applications", run: () => win.openSettings(one.mimeType) })
      items.push({ sep: true })
      items.push({ label: qsTr("Cut"), icon: "content_cut", shortcut: "Ctrl+X", run: () => copySel(true) })
      items.push({ label: qsTr("Copy"), icon: "content_copy", shortcut: "Ctrl+C", run: () => copySel(false) })
      items.push({ label: qsTr("Copy path"), icon: "link", shortcut: qsTr("Ctrl+Shift+C"), run: () => FileOps.copyText(fm.selectedPaths().join("\n")) })
      if (other) {
        items.push({ label: qsTr("Copy to the other pane"), icon: "file_copy", shortcut: "F5", run: () => FileOps.copy(urls, other.model.url) })
        items.push({ label: qsTr("Move to the other pane"), icon: "drive_file_move", shortcut: "F6", run: () => FileOps.move(urls, other.model.url) })
      }
      items.push({ sep: true })
      items.push({ label: qsTr("Rename"), icon: "edit", shortcut: "F2", enabled: !!one, run: () => renameSelected() })
      items.push({ label: qsTr("Move to trash"), icon: "delete", shortcut: qsTr("Del"), run: () => trashSelected() })
      items.push({ label: qsTr("Delete permanently"), icon: "delete_forever", shortcut: qsTr("Shift+Del"), danger: true, run: () => deleteSelected() })
      items.push({ sep: true })
      items.push({ label: qsTr("AI context"), icon: "auto_awesome", shortcut: qsTr("Ctrl+Shift+I"), run: () => win.copyContext(urls) })
      if (one && one.kind === "data") {
        items.push({ label: qsTr("Copy SQL (DuckDB)"), icon: "database", run: () => FileOps.copyText(win.duckSql(one.path)) })
        if (AppInfo.hasDuckdb) items.push({ label: qsTr("Open in DuckDB"), icon: "terminal", run: () => win.openDuckdb(one.path) })
      }
      const dir = one && one.isDir ? one.url : fm.url
      items.push({ label: qsTr("Open terminal here"), icon: "terminal", shortcut: "Ctrl+Alt+T", run: () => FileOps.openTerminal(dir) })
      items.push({ label: qsTr("Open in %1").arg(G.appName(Config.editor)), icon: "code", run: () => FileOps.openInEditor(one ? one.url : fm.url) })
      items.push({ sep: true })
      items.push({ label: qsTr("Properties"), icon: "info", shortcut: qsTr("Alt+Enter"), run: () => win.showProps(sel) })
    } else {
      fm.clearSelection()
      if (fm.isTrash) {
        items.push({ label: qsTr("Empty trash"), icon: "delete_sweep", danger: true, run: () => win.confirmEmptyTrash() })
      } else {
        items.push({ label: qsTr("New folder"), icon: "create_new_folder", shortcut: qsTr("Ctrl+Shift+N"), run: () => win.askNew(fm.url, true) })
        items.push({ label: qsTr("New file"), icon: "note_add", run: () => win.askNew(fm.url, false) })
        items.push({ label: qsTr("Paste"), icon: "content_paste", shortcut: "Ctrl+V", enabled: FileOps.clipboardCount > 0, run: () => pasteHere() })
        items.push({ sep: true })
        items.push({ label: qsTr("Select all"), icon: "select_all", shortcut: "Ctrl+A", run: () => fm.selectAll() })
        items.push({ label: Config.showHidden ? qsTr("Hide hidden files") : qsTr("Show hidden files"), icon: "visibility", shortcut: "Ctrl+H", run: () => Config.showHidden = !Config.showHidden })
        items.push({ sep: true })
        items.push({ label: qsTr("AI context for this folder"), icon: "auto_awesome", run: () => win.copyContext([fm.url]) })
        items.push({ label: qsTr("Open terminal here"), icon: "terminal", shortcut: "Ctrl+Alt+T", run: () => FileOps.openTerminal(fm.url) })
        items.push({ label: qsTr("Open in %1").arg(G.appName(Config.editor)), icon: "code", run: () => FileOps.openInEditor(fm.url) })
        items.push({ label: qsTr("Copy path"), icon: "link", run: () => FileOps.copyText(fm.localPath || fm.url) })
      }
    }
    const p = mapToItem(win.contentItem, x, y)
    win.menu.openAt(p.x, p.y, items)
  }

  // ---------- rubber band (grid + list)
  function bandSelect(r, add) {
    const rows = []
    const top = r.y, bottom = top + r.height   // band lives in content coordinates
    if (viewMode === "grid") {
      const per = columnsPerRow()
      for (let i = 0; i < fm.count; i++) {
        const cx = (i % per) * cellW, cy = Math.floor(i / per) * cellH
        if (cx < r.x + r.width && cx + cellW > r.x && cy < bottom && cy + cellH > top) rows.push(i)
      }
    } else {
      const h = 34
      for (let i = 0; i < fm.count; i++) { const cy = i * h; if (cy < bottom && cy + h > top) rows.push(i) }
    }
    fm.setSelection(rows, add)
  }
  component Band: MouseArea {
    id: band
    property point start
    property bool add: false
    anchors.fill: parent
    z: -1
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    onPressed: e => {
      pane.focusView()
      if (e.button === Qt.RightButton) { pane.contextMenu(mapToItem(pane, e.x, e.y).x, mapToItem(pane, e.x, e.y).y, false); return }
      add = (e.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0
      if (!add) fm.clearSelection()
      start = Qt.point(e.x, e.y)
      rect.visible = false
    }
    onPositionChanged: e => {
      if (!(pressedButtons & Qt.LeftButton)) return
      const r = Qt.rect(Math.min(start.x, e.x), Math.min(start.y, e.y), Math.abs(e.x - start.x), Math.abs(e.y - start.y))
      if (r.width < 4 && r.height < 4) return
      rect.x = r.x; rect.y = r.y; rect.width = r.width; rect.height = r.height; rect.visible = true
      pane.bandSelect(r, add)
    }
    onReleased: rect.visible = false
    Rectangle {
      id: rect
      visible: false
      radius: 8
      color: Qt.alpha(Theme.primary, 0.12)
      border.width: 1
      border.color: Qt.alpha(Theme.primary, 0.7)
    }
  }

  // ---------- views
  GridView {
    id: grid
    anchors.fill: parent
    anchors.margins: 6
    visible: pane.viewMode === "grid"
    focus: pane.viewMode === "grid"
    clip: true
    model: pane.viewMode === "grid" ? fm : null
    cellWidth: pane.cellW
    cellHeight: pane.cellH
    keyNavigationEnabled: false
    currentIndex: fm.currentIndex
    highlightFollowsCurrentItem: false
    boundsBehavior: Flickable.StopAtBounds
    cacheBuffer: pane.listing ? 0 : 600
    reuseItems: pane.listing
    ScrollBar.vertical: Scroll {}
    Keys.onPressed: e => pane.handleKey(e)
    delegate: FileDelegate { pane: paneSelf; gridMode: true }
    Band { width: Math.max(grid.width, grid.contentWidth); height: Math.max(grid.height, grid.contentHeight) }
    add: Transition { enabled: Theme.animations && !pane.listing; NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dur(160) } }
    displaced: Transition { enabled: Theme.animations && !pane.listing; NumberAnimation { properties: "x,y"; duration: Theme.dur(180); easing.type: Easing.OutCubic } }
  }

  ListView {
    id: list
    anchors.fill: parent
    anchors.margins: 6
    visible: pane.viewMode === "list"
    focus: pane.viewMode === "list"
    clip: true
    model: pane.viewMode === "list" ? fm : null
    keyNavigationEnabled: false
    currentIndex: fm.currentIndex
    highlightFollowsCurrentItem: false
    boundsBehavior: Flickable.StopAtBounds
    cacheBuffer: pane.listing ? 0 : 800
    reuseItems: pane.listing
    headerPositioning: ListView.OverlayHeader
    ScrollBar.vertical: Scroll {}
    Keys.onPressed: e => pane.handleKey(e)
    delegate: FileDelegate { pane: paneSelf; gridMode: false }
    header: Rectangle {
      z: 3
      width: list.width; height: 30
      color: Theme.containerLowest
      component H: Item {
        id: h
        property string label; property string field; property real w; property bool alignEnd: false
        width: w; height: 30
        readonly property bool on: Config.sortField === field
        Row {
          anchors.verticalCenter: parent.verticalCenter
          anchors.right: h.alignEnd ? parent.right : undefined; anchors.rightMargin: 16
          x: h.alignEnd ? 0 : 6
          spacing: 3
          Txt { text: label; font.pointSize: 9; font.weight: h.on ? Font.DemiBold : Font.Normal; color: h.on ? Theme.primary : Theme.textDim }
          Icon { visible: h.on; text: Config.sortReversed ? "arrow_upward" : "arrow_downward"; size: 9; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
        }
        StateLayer { radius: 8; onClicked: { if (Config.sortField === field) Config.sortReversed = !Config.sortReversed; else { Config.sortField = field; Config.sortReversed = false } } }
      }
      Row {
        x: 10
        Item { width: 30; height: 30 }
        H { label: qsTr("Name"); field: "name"; w: pane.colName }
        H { label: qsTr("Size"); field: "size"; w: pane.colSize; alignEnd: true }
        H { label: qsTr("Modified"); field: "time"; w: pane.colDate }
        H { label: qsTr("Type"); field: "type"; w: pane.colType }
      }
      Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.alpha(Theme.outlineVariant, 0.6) }
    }
    Band { width: list.width; height: Math.max(list.height, list.contentHeight + 40) }
    add: Transition { enabled: Theme.animations && !pane.listing; NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dur(160) } }
    displaced: Transition { enabled: Theme.animations && !pane.listing; NumberAnimation { properties: "y"; duration: Theme.dur(160); easing.type: Easing.OutCubic } }
  }

  ColumnsView {
    id: columns
    anchors.fill: parent
    anchors.margins: 6
    visible: pane.viewMode === "columns"
    pane: paneSelf
  }

  // ---------- states: loading / empty / error / filter chip
  Spinner { anchors.centerIn: parent; visible: fm.loading && fm.count === 0 }
  EmptyState {
    anchors.centerIn: parent
    visible: !fm.loading && fm.count === 0 && pane.viewMode !== "columns"
    error: fm.error !== ""
    icon: fm.error !== "" ? "error" : fm.searching ? (fm.searchBusy ? "travel_explore" : "search_off") : fm.filter !== "" ? "filter_alt_off" : fm.isTrash ? "delete" : "folder_open"
    emblem: fm.isTrash ? "tomoe" : fm.error !== "" ? "tomoe" : "leaf"
    title: fm.error !== "" ? qsTr("Cannot open")
         : fm.searching ? (fm.searchBusy ? qsTr("Searching…") : qsTr("No results for «%1»").arg(fm.searchQuery))
         : fm.filter !== "" ? qsTr("Nothing matches «%1»").arg(fm.filter)
         : fm.isTrash ? qsTr("The trash is empty") : qsTr("Empty folder")
    hint: fm.error !== "" ? fm.error
        : fm.searching ? (fm.searchBusy ? "" : qsTr("The search includes subfolders and respects .gitignore"))
        : fm.filter !== "" ? qsTr("Backspace edits the filter · Esc clears it")
        : fm.isTrash ? qsTr("What you delete with Del shows up here (and can be undone)")
        : qsTr("Drag files here or press Ctrl+Shift+N to create a folder")
  }
  Rectangle {
    visible: fm.filter !== ""
    anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 12
    height: 32; radius: 16; width: frow.implicitWidth + 24
    color: Theme.primaryContainer
    Row { id: frow; anchors.centerIn: parent; spacing: 8
      Icon { text: "filter_alt"; size: 12; color: Theme.textOnPrimaryContainer; fill: 1; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: fm.filter; mono: true; font.pointSize: 10; color: Theme.textOnPrimaryContainer; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: qsTr("%1 of %2").arg(fm.count).arg(fm.totalCount); font.pointSize: 9; color: Theme.textOnPrimaryContainer; opacity: 0.7; anchors.verticalCenter: parent.verticalCenter }
      IconBtn { icon: "close"; size: 20; fg: Theme.textOnPrimaryContainer; onClicked: fm.filter = ""; anchors.verticalCenter: parent.verticalCenter }
    }
  }

  DropArea {
    anchors.fill: parent
    z: -2
    keys: ["text/uri-list"]
    enabled: !fm.searching
    onEntered: d => { if (d.source === pane) d.accepted = false }
    onDropped: d => pane.handleDrop(d, fm.url)
  }
}
