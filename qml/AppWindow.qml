import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Fuin
import "Glyphs.js" as G

// One top-level window: sidebar · tabs + toolbar · split panes · right panel (preview | IA) · status bar.
ApplicationWindow {
  id: appWin
  property string startUrl: ""
  property string revealUrl: ""
  property bool startPreview: false
  signal closing2()

  width: 1320; height: 820
  minimumWidth: 720; minimumHeight: 460
  visible: true
  title: (pane ? pane.model.title : "") + " — Fūin"
  color: Theme.surface

  property var tabs: []
  property int current: 0
  readonly property var tab: tabs.length ? tabs[Math.min(current, tabs.length - 1)] : null
  readonly property var pane: tab ? tab.pane : null
  property string rightPanel: Config.showPreview ? "preview" : ""
  readonly property alias menu: ctx
  readonly property alias contextPack: packer

  // ---------------------------------------------------------------- tab management
  Component { id: tabComp; TabPage {} }
  function newTab(url) {
    const t = tabComp.createObject(stack, { win: appWin, startUrl: url || (pane ? pane.model.url : AppInfo.toUrl(AppInfo.homePath)) })
    t.anchors.fill = stack
    tabs = tabs.concat([t]); current = tabs.length - 1
    Qt.callLater(() => t.takeFocus())
    return t
  }
  function closeTab(i) {
    if (tabs.length <= 1) { appWin.close(); return }
    const t = tabs[i]
    tabs = tabs.filter((_, j) => j !== i)
    current = Math.min(current >= i && current > 0 ? current - 1 : current, tabs.length - 1)
    t.destroy()
    Qt.callLater(() => tab.takeFocus())
  }
  function openInOtherPane(url) {
    if (!tab.split) tab.toggleSplit()
    const o = tab.otherPane(pane)
    if (o) o.navigate(url)
  }
  function reveal(url, withPreview) {
    const trimmed = url.replace(/\/+$/, "")
    let parent = trimmed.substring(0, trimmed.lastIndexOf("/"))
    if (parent.endsWith(":/") || parent.endsWith("://")) parent += "/"
    pane.navigate(parent || url)
    pane.model.selectUrl(url)
    if (withPreview) rightPanel = "preview"
    appWin.raise(); appWin.requestActivate()
  }
  function toggleRightPanel(which) {
    rightPanel = rightPanel === which ? "" : which
    if (which === "preview" || rightPanel === "") Config.showPreview = rightPanel === "preview"
  }
  // FUIN_DEV=1 automation: {"cmd":"dev","panel":"ai","view":"list","split":true,"select":url,"grab":"/tmp/x.png"}
  function dev(a) {
    if (a.navigate) pane.navigate(a.navigate)
    if (a.view) Config.viewMode = a.view
    if (a.panel !== undefined) rightPanel = a.panel
    if (a.split !== undefined && a.split !== tab.split) tab.toggleSplit()
    if (a.select) pane.model.selectUrl(a.select)
    if (a.search !== undefined) { search.text = a.search; pane.model.searchQuery = a.search }
    if (a.filter !== undefined) pane.model.filter = a.filter
    if (a.menu) pane.contextMenu(pane.width / 2, pane.height / 3, true)
    if (a.closeMenu) ctx.close()
    if (a.toast) toast(a.toast, "ok")
    if (a.hidden !== undefined) Config.showHidden = a.hidden
    if (a.copy) FileOps.copy(a.copy, a.dest)
    if (a.trash) FileOps.trash(a.trash)
    if (a.undo) FileOps.undo()
    if (a.context) copyContext(a.context)
    if (a.key) keysDlg.open()
    if (a.approve !== undefined && agentDlg.opened) { Ipc.resolve(agentDlg.reqId, a.approve); agentDlg.close() }
    if (a.grab) Qt.callLater(() => AppInfo.grabWindow(appWin, a.grab))
  }
  function toast(text, kind) { toaster.show(text, kind) }

  // ---------------------------------------------------------------- actions used by panes
  function copyContext(urls) { packer.build(urls, Config.contextFormat, true) }
  function sqlQuote(p) { return "'" + p.replace(/'/g, "''") + "'" }
  function duckSql(path) {
    const low = path.toLowerCase()
    const fn = low.endsWith(".parquet") ? "read_parquet" : (low.endsWith(".json") || low.endsWith(".jsonl") || low.endsWith(".ndjson")) ? "read_json_auto" : "read_csv_auto"
    return "SELECT * FROM " + fn + "(" + sqlQuote(path) + ") LIMIT 100;"
  }
  function openDuckdb(path) {
    const dir = path.substring(0, path.lastIndexOf("/")) || "/"
    const view = "CREATE VIEW t AS " + duckSql(path).replace(" LIMIT 100;", ";")
    FileOps.openTerminal(dir, "duckdb -cmd " + shq(view + " SUMMARIZE t;"))
  }
  function shq(s) { return "'" + s.replace(/'/g, "'\\''") + "'" }
  function confirmDelete(urls) { delDlg.urls = urls; delDlg.open() }
  function confirmEmptyTrash() { emptyDlg.open() }
  function askRename(url, name) { nameDlg.mode = "rename"; nameDlg.target = url; nameDlg.initial = name; nameDlg.open() }
  function askNew(dirUrl, folder) { nameDlg.mode = folder ? "folder" : "file"; nameDlg.target = dirUrl; nameDlg.initial = folder ? "Nueva carpeta" : "nuevo.txt"; nameDlg.open() }
  function showProps(items) { propsDlg.items = items; propsDlg.open() }

  ContextPack {
    id: packer
    onCopied: s => appWin.toast("Contexto copiado · ≈ " + G.count(s.tokens) + " tokens · " + s.files + " ficheros" + (s.secretsBlocked ? " · " + s.secretsBlocked + (s.secretsBlocked === 1 ? " secreto bloqueado" : " secretos bloqueados") : ""), "ok")
  }
  Connections {
    target: FileOps
    function onNotify(text, kind, undoable) { toaster.show(text, kind, undoable ? "Deshacer" : "", undoable ? () => FileOps.undo() : null) }
    function onCreated(url) { if (appWin.active && appWin.pane) appWin.pane.model.selectUrl(url) }
  }

  // publish state for agents (MCP fuin_get_current_view)
  Timer {
    id: publish
    interval: 400
    running: true
    repeat: true
    property string last: ""
    onTriggered: {
      if (!pane) return
      const sel = pane.model.selectedPaths()
      const st = { url: pane.model.url, path: pane.model.localPath, title: pane.model.title, selection: sel.slice(0, 200), selectionCount: sel.length,
                   count: pane.model.count, filter: pane.model.filter, search: pane.model.searchQuery, repoRoot: pane.model.repoRoot, repoBranch: pane.model.repoBranch,
                   view: Config.viewMode, tabs: tabs.map(t => t.pane.model.url) }
      const j = JSON.stringify(st)
      if (j !== last) { last = j; Ipc.publishState(st) }
    }
  }

  // ---------------------------------------------------------------- layout
  Rectangle { anchors.fill: parent; color: Theme.surface }
  Watermark { kind: "konoha"; width: 520; height: 520; x: -150; y: parent.height - 360; opacity: 0.025 }

  Sidebar { id: side; win: appWin; x: 0; y: 0; width: 236; height: parent.height - status.height }

  Item {
    id: main
    anchors.left: side.right; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: status.top
    anchors.rightMargin: 10

    // tab strip
    Row {
      id: tabStrip
      x: 0; y: 8; height: 34; spacing: 6
      visible: appWin.tabs.length > 1
      Repeater {
        model: appWin.tabs.length
        Rectangle {
          required property int index
          readonly property var t: appWin.tabs[index]
          readonly property bool on: index === appWin.current
          height: 32; radius: 16
          width: Math.min(220, tl.implicitWidth + 64)
          color: on ? Theme.secondaryContainer : th.hovered ? Theme.containerHigh : "transparent"
          Behavior on color { CAnim {} }
          HoverHandler { id: th }
          TapHandler { onTapped: { appWin.current = index; appWin.tab.takeFocus() } }
          TapHandler { acceptedButtons: Qt.MiddleButton; onTapped: appWin.closeTab(index) }
          Icon { x: 12; anchors.verticalCenter: parent.verticalCenter; text: t && t.isTrash ? "delete" : "folder"; fill: on ? 1 : 0; size: 12; color: on ? Theme.textOnSecondaryContainer : Theme.textDim }
          Txt { id: tl; x: 32; width: parent.width - 62; anchors.verticalCenter: parent.verticalCenter; text: t ? t.title : ""; elide: Text.ElideRight; font.pointSize: 9.5
                color: on ? Theme.textOnSecondaryContainer : Theme.textDim }
          IconBtn { anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter; size: 24; icon: "close"; onClicked: appWin.closeTab(index) }
        }
      }
      IconBtn { size: 30; icon: "add"; tip: "Pestaña nueva (Ctrl+T)"; onClicked: appWin.newTab(); anchors.verticalCenter: parent.verticalCenter }
    }

    // toolbar
    Item {
      id: toolbar
      anchors.left: parent.left; anchors.right: parent.right
      y: tabStrip.visible ? 48 : 10
      height: 42
      Row {
        id: nav
        spacing: 2; anchors.verticalCenter: parent.verticalCenter
        IconBtn { icon: "arrow_back"; tip: "Atrás (Alt+←)"; enabled2: pane && pane.model.canBack; onClicked: pane.model.back() }
        IconBtn { icon: "arrow_forward"; tip: "Adelante (Alt+→)"; enabled2: pane && pane.model.canForward; onClicked: pane.model.forward() }
        IconBtn { icon: "arrow_upward"; tip: "Subir (Alt+↑)"; enabled2: pane && pane.model.canUp; onClicked: pane.model.up() }
      }
      PathBar { id: pathBar; pane: appWin.pane; anchors.left: nav.right; anchors.leftMargin: 8; anchors.right: search.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter }
      Field {
        id: search
        width: activeFocus || text !== "" ? 280 : 200
        Behavior on width { Anim {} }
        anchors.right: tools.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
        icon: pane && pane.model.searchBusy ? "progress_activity" : "search"
        placeholderText: "Buscar en " + (pane ? pane.model.title : "") + "  (Ctrl+F)"
        font.pointSize: 10
        onTextEdited: searchDebounce.restart()
        Timer { id: searchDebounce; interval: 220; onTriggered: if (appWin.pane) appWin.pane.model.searchQuery = search.text }
        Keys.onEscapePressed: { text = ""; pane.model.searchQuery = ""; pane.focusView() }
        Keys.onDownPressed: pane.focusView()
        Keys.onReturnPressed: { searchDebounce.stop(); pane.model.searchQuery = text; pane.focusView() }
        Connections { target: appWin; function onPaneChanged() { search.text = appWin.pane ? appWin.pane.model.searchQuery : "" } }
      }
      Row {
        id: tools
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Rectangle {
          height: 36; radius: 18; width: vrow.width + 8; color: Theme.containerHigh; anchors.verticalCenter: parent.verticalCenter
          Row { id: vrow; x: 4; anchors.verticalCenter: parent.verticalCenter; spacing: 0
            IconBtn { size: 30; icon: "grid_view"; tip: "Cuadrícula (Ctrl+1)"; checked: Config.viewMode === "grid"; onClicked: Config.viewMode = "grid" }
            IconBtn { size: 30; icon: "view_list"; tip: "Lista (Ctrl+2)"; checked: Config.viewMode === "list"; onClicked: Config.viewMode = "list" }
            IconBtn { size: 30; icon: "view_column"; tip: "Columnas (Ctrl+3)"; checked: Config.viewMode === "columns"; onClicked: Config.viewMode = "columns" } }
        }
        Item { width: 6; height: 1 }
        IconBtn { icon: Config.showHidden ? "visibility" : "visibility_off"; tip: "Ficheros ocultos (Ctrl+H)"; checked: Config.showHidden; onClicked: Config.showHidden = !Config.showHidden }
        IconBtn { icon: "vertical_split"; tip: "Vista dividida (F3)"; checked: tab && tab.split; onClicked: tab.toggleSplit() }
        IconBtn { icon: "right_panel_open"; tip: "Vista previa (F9 · Espacio)"; checked: appWin.rightPanel === "preview"; onClicked: appWin.toggleRightPanel("preview") }
        IconBtn { icon: "auto_awesome"; tip: "IA y agentes (Ctrl+I)"; checked: appWin.rightPanel === "ai"; onClicked: appWin.toggleRightPanel("ai") }
        IconBtn { icon: "more_vert"; tip: "Más"; onClicked: { const p = mapToItem(appWin.contentItem, 0, height); ctx.openAt(p.x - 200, p.y, appWin.moreItems()) } }
      }
    }

    // content
    Item {
      id: stack
      anchors.left: parent.left; anchors.top: toolbar.bottom; anchors.topMargin: 8; anchors.bottom: parent.bottom
      anchors.right: sidePanel.visible ? sidePanel.left : parent.right
      anchors.rightMargin: sidePanel.visible ? 10 : 0
    }
    Connections { target: appWin; function onCurrentChanged() { for (let i = 0; i < appWin.tabs.length; ++i) appWin.tabs[i].visible = i === appWin.current } function onTabsChanged() { for (let i = 0; i < appWin.tabs.length; ++i) appWin.tabs[i].visible = i === appWin.current } }

    // right panel
    ChakraFrame {
      id: sidePanel
      visible: appWin.rightPanel !== ""
      anchors.right: parent.right; anchors.top: toolbar.bottom; anchors.topMargin: 8; anchors.bottom: parent.bottom
      width: Math.min(420, Math.max(300, main.width * 0.3))
      radius: 16; fill: Theme.containerLow; ring: false; borderWidth: 1
      clip: true
      Segmented {
        id: rseg
        x: 12; y: 10
        options: [{ value: "preview", label: "Vista previa", icon: "visibility" }, { value: "ai", label: "IA", icon: "auto_awesome" }]
        value: appWin.rightPanel
        onPicked: v => appWin.rightPanel = v
      }
      IconBtn { anchors.right: parent.right; anchors.rightMargin: 8; y: 10; size: 30; icon: "close"; onClicked: appWin.toggleRightPanel(appWin.rightPanel) }
      Loader {
        anchors.top: rseg.bottom; anchors.topMargin: 6; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        sourceComponent: appWin.rightPanel === "ai" ? aiComp : appWin.rightPanel === "preview" ? previewComp : null
      }
      Component { id: previewComp; Preview { pane: appWin.pane } }
      Component { id: aiComp; AiPanel { pane: appWin.pane; win: appWin } }
    }
  }

  // status bar
  Item {
    id: status
    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
    height: 30
    Row {
      x: 18; anchors.verticalCenter: parent.verticalCenter; spacing: 14
      Txt {
        dim: true; font.pointSize: 8.5; anchors.verticalCenter: parent.verticalCenter
        text: !pane ? "" : pane.model.loading ? "Cargando…" :
              (pane.model.searching ? G.count(pane.model.count) + " resultados" : G.count(pane.model.count) + " elementos" + (pane.model.filter ? " (filtro «" + pane.model.filter + "»)" : ""))
              + (pane.model.selectionCount > 0 ? "  ·  " + pane.model.selectionCount + " seleccionados" + (pane.model.selectionSize > 0 ? " · " + G.size(pane.model.selectionSize) : "") : "")
      }
      Txt { visible: FileOps.clipboardCount > 0; dim: true; font.pointSize: 8.5; anchors.verticalCenter: parent.verticalCenter
            text: (FileOps.clipboardCut ? "✂ " : "⧉ ") + FileOps.clipboardCount + " en el portapapeles" }
      Txt { visible: Ipc.pendingConfirmations > 0; font.pointSize: 8.5; color: Theme.tertiary; anchors.verticalCenter: parent.verticalCenter; text: "Un agente espera confirmación" }
    }
    Row {
      anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; spacing: 10
      visible: FileOps.busy
      Txt { text: FileOps.opLabel + (FileOps.speed ? " · " + FileOps.speed : "") + (FileOps.jobCount > 1 ? " · " + FileOps.jobCount + " tareas" : ""); font.pointSize: 8.5; anchors.verticalCenter: parent.verticalCenter }
      Rectangle {
        width: 180; height: 6; radius: 3; color: Theme.containerHighest; anchors.verticalCenter: parent.verticalCenter
        Rectangle { width: parent.width * Math.max(0.04, FileOps.progress); height: parent.height; radius: 3; color: Theme.primary; Behavior on width { Anim {} } }
      }
      IconBtn { size: 24; icon: "close"; tip: "Cancelar"; onClicked: FileOps.cancelAll() }
    }
    Txt { visible: !FileOps.busy; anchors.right: parent.right; anchors.rightMargin: 18; anchors.verticalCenter: parent.verticalCenter; dim: true; font.pointSize: 8
          text: pane && pane.model.repoBranch ? " " + pane.model.repoBranch + (pane.model.repoDirty > 0 ? " · " + pane.model.repoDirty + " cambios" : " · limpio") : "" }
  }

  function moreItems() {
    return [
      { label: "Ventana nueva", icon: "open_in_new", run: () => FileOps.openInFileManagerWindow(pane.model.url) },
      { label: "Pestaña nueva", icon: "tab", shortcut: "Ctrl+T", run: () => newTab() },
      { sep: true },
      { label: "Abrir terminal aquí", icon: "terminal", shortcut: "Ctrl+Alt+T", enabled: pane.model.isLocal, run: () => FileOps.openTerminal(pane.model.localPath) },
      { label: "Abrir en " + G.appName(Config.editor), icon: "code", enabled: pane.model.isLocal, run: () => FileOps.openInEditor(pane.model.localPath) },
      { label: "Copiar ruta", icon: "content_copy", run: () => FileOps.copyText(pane.model.isLocal ? pane.model.localPath : pane.model.url) },
      { sep: true },
      { label: FileOps.canUndo ? "Deshacer: " + FileOps.undoText : "Nada que deshacer", icon: "undo", enabled: FileOps.canUndo, shortcut: "Ctrl+Z", run: () => FileOps.undo() },
      { label: "Atajos de teclado", icon: "keyboard", shortcut: "F1", run: () => keysDlg.open() },
      { label: "Ajustes (" + Config.configFile.split("/").pop() + ")", icon: "settings", run: () => FileOps.openInEditor(Config.configFile) },
      { label: "Acerca de Fūin " + AppInfo.version, icon: "info", run: () => aboutDlg.open() }
    ]
  }

  // ---------------------------------------------------------------- overlays
  CtxMenu { id: ctx }
  Toast { id: toaster }

  Modal {
    id: delDlg
    property var urls: []
    title: "¿Eliminar definitivamente?"; icon: "delete_forever"
    Txt { width: parent.width; wrapMode: Text.Wrap; text: delDlg.urls.length === 1 ? "«" + decodeURIComponent(String(delDlg.urls[0]).split("/").pop()) + "» se borrará sin pasar por la papelera. No se puede deshacer."
                                                                              : delDlg.urls.length + " elementos se borrarán sin pasar por la papelera. No se puede deshacer." }
    buttons: [
      Btn { kind: "text"; text: "Cancelar"; onClicked: delDlg.close() },
      Btn { kind: "danger"; icon: "delete_forever"; text: "Eliminar"; onClicked: { FileOps.deleteForever(delDlg.urls); delDlg.close() } }
    ]
    onOpened: delBtnFocus.forceActiveFocus()
    Item { id: delBtnFocus; focus: true; Keys.onReturnPressed: { FileOps.deleteForever(delDlg.urls); delDlg.close() } }
    onClosed: if (appWin.pane) appWin.pane.focusView()
  }
  Modal {
    id: emptyDlg
    title: "¿Vaciar la papelera?"; icon: "delete_sweep"
    Txt { width: parent.width; wrapMode: Text.Wrap; text: "Todos los elementos de la papelera se borrarán para siempre." }
    buttons: [
      Btn { kind: "text"; text: "Cancelar"; onClicked: emptyDlg.close() },
      Btn { kind: "danger"; icon: "delete_sweep"; text: "Vaciar"; onClicked: { FileOps.emptyTrash(); emptyDlg.close() } }
    ]
  }
  Modal {
    id: nameDlg
    property string mode: "rename"
    property string target: ""
    property string initial: ""
    title: mode === "rename" ? "Renombrar" : mode === "folder" ? "Nueva carpeta" : "Nuevo archivo"
    icon: mode === "rename" ? "edit" : mode === "folder" ? "create_new_folder" : "note_add"
    Field {
      id: nameField
      width: parent.width
      Keys.onReturnPressed: nameDlg.accept()
      Keys.onEnterPressed: nameDlg.accept()
    }
    Txt { visible: nameField.text.indexOf("/") >= 0; text: "El nombre no puede contener «/»"; color: Theme.error; font.pointSize: 9 }
    buttons: [
      Btn { kind: "text"; text: "Cancelar"; onClicked: nameDlg.close() },
      Btn { kind: "filled"; text: nameDlg.mode === "rename" ? "Renombrar" : "Crear"; enabled2: nameField.text.trim() !== "" && nameField.text.indexOf("/") < 0; onClicked: nameDlg.accept() }
    ]
    function accept() {
      const n = nameField.text.trim()
      if (n === "" || n.indexOf("/") >= 0) return
      if (mode === "rename") { if (n !== initial) FileOps.rename(target, n) }
      else if (mode === "folder") FileOps.mkdir(target, n)
      else FileOps.newFile(target, n)
      close()
    }
    onOpened: {
      nameField.text = initial; nameField.forceActiveFocus()
      const dot = initial.lastIndexOf(".")
      if (mode !== "folder" && dot > 0) nameField.select(0, dot); else nameField.selectAll()
    }
    onClosed: if (appWin.pane) appWin.pane.focusView()
  }
  Modal {
    id: propsDlg
    property var items: []
    readonly property var one: items.length === 1 ? items[0] : null
    title: one ? one.name : items.length + " elementos"; icon: "info"
    Repeater {
      model: propsDlg.one ? [
        ["Tipo", propsDlg.one.mimeComment + "  (" + propsDlg.one.mimeType + ")"],
        ["Ubicación", propsDlg.one.location],
        ["Tamaño", propsDlg.one.isDir ? "—" : G.size(propsDlg.one.size) + "  (" + G.count(propsDlg.one.size) + " bytes)"],
        ["Modificado", new Date(propsDlg.one.mtime).toLocaleString(Qt.locale("es_ES"))],
        ["Enlace", propsDlg.one.isLink ? "Sí" : "No"],
        ["Git", propsDlg.one.gitBranch ? propsDlg.one.gitBranch + (propsDlg.one.gitDirty ? " · " + propsDlg.one.gitDirty + " cambios" : "") : "—"]
      ] : [["Total", G.size(propsDlg.items.reduce((a, b) => a + (b.isDir ? 0 : b.size), 0)) + " (sin contar carpetas)"]]
      Row {
        required property var modelData
        spacing: 12
        Txt { width: 100; text: modelData[0]; dim: true; font.pointSize: 9.5 }
        Txt { width: propsDlg.width - 160; text: modelData[1]; font.pointSize: 9.5; wrapMode: Text.WrapAnywhere }
      }
    }
    buttons: [ Btn { kind: "tonal"; text: "Cerrar"; onClicked: propsDlg.close() } ]
  }
  Modal {
    id: agentDlg
    property string reqId: ""
    property string detail: ""
    property string client: ""
    title: "Un agente quiere hacer un cambio"; icon: "smart_toy"
    closePolicy: Popup.NoAutoClose
    Txt { width: parent.width; wrapMode: Text.Wrap; text: (agentDlg.client || "Un agente MCP") + " solicita:"; dim: true }
    Rectangle { width: parent.width; height: adt.implicitHeight + 20; radius: 12; color: Theme.containerLowest
      Txt { id: adt; x: 12; y: 10; width: parent.width - 24; text: agentDlg.detail; mono: true; font.pointSize: 9; wrapMode: Text.WrapAnywhere } }
    Txt { width: parent.width; wrapMode: Text.Wrap; font.pointSize: 9; dim: true; text: "Podrás deshacerlo con Ctrl+Z." }
    buttons: [
      Btn { kind: "text"; text: "Rechazar"; onClicked: { Ipc.resolve(agentDlg.reqId, false); agentDlg.close() } },
      Btn { kind: "filled"; icon: "check"; text: "Permitir"; onClicked: { Ipc.resolve(agentDlg.reqId, true); agentDlg.close() } }
    ]
  }
  function askAgent(id, title, detail, client) {
    agentDlg.reqId = id; agentDlg.title = title || "Un agente quiere hacer un cambio"; agentDlg.detail = detail; agentDlg.client = client
    agentDlg.open(); appWin.raise(); appWin.requestActivate()
  }
  Modal {
    id: keysDlg
    title: "Atajos de teclado"; icon: "keyboard"
    Grid {
      columns: 2; columnSpacing: 18; rowSpacing: 5
      Repeater {
        model: ["Ctrl+L", "Editar ruta", "Ctrl+F", "Buscar (recursivo)", "Escribir", "Filtrar al vuelo", "Ctrl+T / Ctrl+W", "Abrir / cerrar pestaña",
                "Ctrl+Tab", "Siguiente pestaña", "F3", "Vista dividida", "F5 / F6", "Copiar / mover al otro panel", "F9 · Espacio", "Vista previa",
                "Ctrl+I", "Panel IA", "Ctrl+Mayús+I", "Copiar contexto para IA", "F2", "Renombrar", "Supr", "Mover a la papelera",
                "Mayús+Supr", "Eliminar definitivamente", "Ctrl+Z", "Deshacer", "Ctrl+H", "Ocultos", "Ctrl+1/2/3", "Cuadrícula / lista / columnas",
                "Ctrl+Mayús+N", "Nueva carpeta", "Ctrl+Mayús+C", "Copiar ruta", "Ctrl+Alt+T", "Terminal aquí", "Alt+←/→/↑", "Atrás / adelante / subir"]
        Txt { required property string modelData; required property int index; text: modelData; mono: index % 2 === 0; font.pointSize: 9; color: index % 2 === 0 ? Theme.primary : Theme.textMain }
      }
    }
    buttons: [ Btn { kind: "tonal"; text: "Cerrar"; onClicked: keysDlg.close() } ]
  }
  Modal {
    id: aboutDlg
    title: "Fūin " + AppInfo.version; icon: "folder_special"
    Txt { width: parent.width; wrapMode: Text.Wrap; text: "Gestor de ficheros nativo (Qt 6 + KDE Frameworks / KIO) con vista previa de datos y modelos, contexto para IA y servidor MCP. Licencia MIT." }
    Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 9
          text: "Colores: " + (Theme.colorsSource || "tema integrado") + "\nAjustes: " + Config.configFile }
    buttons: [ Btn { kind: "tonal"; text: "Cerrar"; onClicked: aboutDlg.close() } ]
  }

  // ---------------------------------------------------------------- shortcuts
  readonly property bool typing: !!activeFocusItem && activeFocusItem.cursorPosition !== undefined
  readonly property bool modalOpen: delDlg.opened || nameDlg.opened || propsDlg.opened || agentDlg.opened || emptyDlg.opened || keysDlg.opened || aboutDlg.opened || ctx.visible
  component Key: Shortcut { property bool always: false; enabled: !appWin.modalOpen && (always || !appWin.typing); context: Qt.WindowShortcut }
  Key { sequence: "Ctrl+L"; always: true; onActivated: pathBar.edit() }
  Key { sequence: "Ctrl+F"; always: true; onActivated: { search.forceActiveFocus(); search.selectAll() } }
  Key { sequence: "Ctrl+T"; always: true; onActivated: newTab() }
  Key { sequence: "Ctrl+W"; always: true; onActivated: closeTab(current) }
  Key { sequence: "Ctrl+Tab"; always: true; onActivated: { current = (current + 1) % tabs.length; tab.takeFocus() } }
  Key { sequence: "Ctrl+Shift+Tab"; always: true; onActivated: { current = (current - 1 + tabs.length) % tabs.length; tab.takeFocus() } }
  Key { sequence: "Ctrl+H"; always: true; onActivated: Config.showHidden = !Config.showHidden }
  Key { sequence: "F2"; onActivated: pane.renameSelected() }
  Key { sequence: "Delete"; onActivated: pane.trashSelected() }
  Key { sequence: "Shift+Delete"; onActivated: pane.deleteSelected() }
  Key { sequence: "Ctrl+C"; onActivated: pane.copySel(false) }
  Key { sequence: "Ctrl+X"; onActivated: pane.copySel(true) }
  Key { sequence: "Ctrl+V"; onActivated: pane.pasteHere() }
  Key { sequence: "Ctrl+A"; onActivated: pane.model.selectAll() }
  Key { sequence: "Ctrl+Z"; onActivated: FileOps.undo() }
  Key { sequence: "Ctrl+Shift+C"; onActivated: { const p = pane.model.selectedPaths(); FileOps.copyText(p.length ? p.join("\n") : pane.model.localPath || pane.model.url); toast("Ruta copiada", "ok") } }
  Key { sequence: "Ctrl+Shift+N"; onActivated: askNew(pane.model.url, true) }
  Key { sequence: "Ctrl+Shift+I"; onActivated: copyContext(pane.model.selectionCount ? pane.model.selectedUrls() : [pane.model.url]) }
  Key { sequence: "Ctrl+I"; always: true; onActivated: toggleRightPanel("ai") }
  Key { sequence: "F9"; always: true; onActivated: toggleRightPanel("preview") }
  Key { sequence: "F3"; always: true; onActivated: tab.toggleSplit() }
  Key { sequence: "F5"; onActivated: { const o = tab.otherPane(pane); if (o && pane.model.selectionCount) FileOps.copy(pane.model.selectedUrls(), o.model.url); else pane.model.refresh() } }
  Key { sequence: "F6"; onActivated: { const o = tab.otherPane(pane); if (o && pane.model.selectionCount) FileOps.move(pane.model.selectedUrls(), o.model.url) } }
  Key { sequence: "Alt+Left"; always: true; onActivated: pane.model.back() }
  Key { sequence: "Alt+Right"; always: true; onActivated: pane.model.forward() }
  Key { sequence: "Alt+Up"; always: true; onActivated: pane.model.up() }
  Key { sequence: "Alt+Home"; always: true; onActivated: pane.navigate(AppInfo.toUrl(AppInfo.homePath)) }
  Key { sequence: "Alt+Return"; onActivated: { const s = pane.model.selectedItems(); if (s.length) showProps(s) } }
  Key { sequence: "Ctrl+1"; always: true; onActivated: Config.viewMode = "grid" }
  Key { sequence: "Ctrl+2"; always: true; onActivated: Config.viewMode = "list" }
  Key { sequence: "Ctrl+3"; always: true; onActivated: Config.viewMode = "columns" }
  Key { sequences: ["Ctrl++", "Ctrl+="]; always: true; onActivated: Config.iconSize = Math.min(192, Config.iconSize + 16) }
  Key { sequence: "Ctrl+-"; always: true; onActivated: Config.iconSize = Math.max(32, Config.iconSize - 16) }
  Key { sequence: "Ctrl+Alt+T"; always: true; onActivated: FileOps.openTerminal(pane.model.localPath) }
  Key { sequence: "F1"; always: true; onActivated: keysDlg.open() }
  Key { sequence: "Ctrl+Q"; always: true; onActivated: Qt.quit() }

  Component.onCompleted: {
    newTab(startUrl || AppInfo.toUrl(AppInfo.homePath))
    if (revealUrl) Qt.callLater(() => reveal(revealUrl, startPreview))
  }
  onClosing: closing2()
}
