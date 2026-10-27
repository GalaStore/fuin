import QtQuick
import QtQuick.Controls
import Fuin
import "Glyphs.js" as G

// Preview pane: images, video/PDF posters, code (KSyntaxHighlighting), Markdown, tables with
// schema (CSV/JSON/JSONL/Parquet), ML model headers (safetensors/GGUF/ONNX) and folders.
Item {
  id: root
  property var pane
  readonly property var entry: pane ? pane.current : null
  readonly property string target: entry ? entry.url : (pane && pane.model.isLocal ? pane.model.url : "")
  readonly property var r: pv.result
  readonly property string kind: r.kind || (entry && entry.kind === "image" ? "image" : "")

  Previewer { id: pv; source: root.target }
  // folder summary goes stale when items are added/removed: re-inspect (debounced)
  Timer { id: staleTimer; interval: 400; onTriggered: if (!root.entry) pv.reload() }
  Connections { target: root.pane ? root.pane.model : null; function onCountChanged() { if (!root.entry) staleTimer.restart() } }

  // ---- header
  Column {
    id: head
    x: 16; y: 14; width: parent.width - 32
    spacing: 4
    Row {
      spacing: 8; width: parent.width
      Icon { text: entry ? G.icon(entry.kind, entry.ext, entry.name, entry.isDir) : "folder"; size: 14; fill: 1; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: entry ? entry.name : (pane ? pane.model.title : ""); width: parent.width - 30; wrapMode: Text.WrapAnywhere; maximumLineCount: 2; elide: Text.ElideRight
            font.pointSize: 11.5; font.weight: Font.DemiBold }
    }
    Txt {
      width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 9
      text: {
        if (!entry) {
          if (r.kind === "folder") return qsTr("%1 items · current folder").arg(r.dirs + r.files) + (pane.model.repoBranch ? " ·  " + pane.model.repoBranch : "")
          return (pane && pane.model.selectionCount > 1) ? qsTr("%1 items selected · %2").arg(pane.model.selectionCount).arg(G.size(pane.model.selectionSize)) : ""
        }
        const parts = []
        if (entry.isDir) parts.push(r.kind === "folder" ? qsTr("%1 items").arg(r.dirs + r.files) : qsTr("Folder"))
        else parts.push(G.size(entry.size))
        parts.push(G.date(entry.mtime))
        if (r.mime) parts.push(r.mime)
        if (r.kind === "image" && r.width > 0) parts.push(r.width + "×" + r.height)
        return parts.filter(s => s).join(" · ")
      }
    }
    Flow {
      width: parent.width; spacing: 6
      visible: (r.project && r.project.length > 0) || !!r.secret || (entry && entry.gitBranch)
      Repeater { model: r.project || []
        Rectangle { required property string modelData; height: 22; radius: 11; width: pt.implicitWidth + 14; color: Theme.secondaryContainer
          Txt { id: pt; anchors.centerIn: parent; text: modelData; font.pointSize: 8.5; color: Theme.textOnSecondaryContainer } } }
      Rectangle { visible: !!(entry && entry.gitBranch); height: 22; radius: 11; width: gb.implicitWidth + 14; color: Theme.tertiaryContainer
        Txt { id: gb; anchors.centerIn: parent; text: " " + (entry ? entry.gitBranch : "") + (entry && entry.gitDirty > 0 ? "  ●" + entry.gitDirty : ""); mono: true; font.pointSize: 8.5; color: Theme.textOnTertiaryContainer } }
      Rectangle { visible: !!r.secret; height: 22; radius: 11; width: sr.implicitWidth + 14; color: Theme.errorContainer
        Row { id: sr; anchors.centerIn: parent; spacing: 4
          Icon { text: "key"; size: 10; color: Theme.textOnErrorContainer; anchors.verticalCenter: parent.verticalCenter }
          Txt { text: qsTr("Possible secret: %1").arg(r.secret || ""); font.pointSize: 8.5; color: Theme.textOnErrorContainer; anchors.verticalCenter: parent.verticalCenter } } }
    }
    // data / model actions
    Flow {
      width: parent.width; spacing: 6
      visible: kind === "table" || kind === "model" || (entry && entry.isDir) || kind === "text"
      Btn { visible: root.kind === "table"; compact: true; kind: "tonal"; icon: "database"; text: qsTr("Copy SQL"); onClicked: FileOps.copyText(pv.duckdbSql(entry.path)) }
      Btn { visible: root.kind === "table" && AppInfo.hasDuckdb; compact: true; kind: "tonal"; icon: "terminal"; text: qsTr("Open in DuckDB"); onClicked: pane.win.openDuckdb(entry.path) }
      Btn { visible: root.kind === "table" || root.kind === "model"; compact: true; kind: "text"; icon: "schema"; text: qsTr("Copy schema"); onClicked: FileOps.copyText(root.schemaMarkdown()) }
      Btn { visible: !!(entry && entry.isDir); compact: true; kind: "tonal"; icon: "code"; text: qsTr("Open in %1").arg(G.appName(Config.editor)); onClicked: FileOps.openInEditor(entry.url) }
      Btn { visible: !!entry && !r.secret; compact: true; kind: "text"; icon: "auto_awesome"; text: qsTr("AI context"); onClicked: pane.win.copyContext([entry.url]) }
    }
    Item { width: 1; height: 4 }
    ChakraLine { width: parent.width; height: 2; opacity: 0.6 }
  }

  function schemaMarkdown() {
    const name = entry ? entry.name : ""
    if (r.kind === "table") {
      let s = (r.totalRows !== undefined ? qsTr("Schema of `%1` (%2 rows)").arg(name).arg(r.totalRows) : qsTr("Schema of `%1`").arg(name))
              + "\n\n" + qsTr("| column | type | nulls | min | max |") + "\n|---|---|---|---|---|\n"
      for (const c of r.columns || []) s += "| " + c.name + " | " + c.type + " | " + (c.nulls ?? "") + " | " + (c.min ?? "") + " | " + (c.max ?? "") + " |\n"
      return s
    }
    let s = qsTr("Model `%1`").arg(name) + "\n\n"
    for (const f of r.fields || []) s += "- " + f[0] + ": " + f[1] + "\n"
    return s
  }

  // ---- body
  Item {
    id: body
    anchors.top: head.bottom; anchors.topMargin: 12
    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
    anchors.margins: 12; anchors.leftMargin: 14

    Image {
      id: pimg
      anchors.fill: parent
      visible: status === Image.Ready && (kind === "image" || kind === "video" || kind === "pdf")
      fillMode: Image.PreserveAspectFit
      asynchronous: true
      horizontalAlignment: Image.AlignHCenter; verticalAlignment: Image.AlignTop
      sourceSize: Qt.size(Math.max(64, width * 1.5), Math.max(64, height * 1.5))
      source: !entry ? "" : kind === "image" ? entry.url
            : (kind === "video" || kind === "pdf") ? "image://thumb/" + encodeURIComponent(entry.url) + "@" + entry.mtime : ""
    }

    // code / text
    ScrollView {
      anchors.fill: parent
      visible: kind === "text" && !(entry && entry.isDir)
      clip: true
      ScrollBar.vertical: Scroll {}
      ScrollBar.horizontal: Scroll {}
      TextArea {
        id: code
        readOnly: true
        selectByMouse: true
        text: kind === "text" ? (r.text || "") + (r.truncated ? "\n\n" + qsTr("… (preview truncated)") : "") : ""
        font.family: Theme.fontMono; font.pointSize: 9
        color: Theme.textMain
        selectionColor: Theme.primary; selectedTextColor: Theme.textOnPrimary
        wrapMode: TextEdit.NoWrap
        textFormat: TextEdit.PlainText
        background: null
        padding: 0
      }
    }
    CodeHighlighter { document: code.textDocument; fileName: entry ? entry.name : ""; definitionName: r.language || ""; dark: !Theme.light }

    // markdown (files and folder READMEs)
    ScrollView {
      anchors.fill: parent
      visible: kind === "markdown" || (kind === "folder" && !!r.readme)
      clip: true
      contentWidth: availableWidth
      ScrollBar.vertical: Scroll {}
      Text {
        width: body.width - 12
        text: kind === "markdown" ? (r.text || "") : kind === "folder" ? (r.readme || "") : ""
        textFormat: Text.MarkdownText
        wrapMode: Text.Wrap
        color: Theme.textMain
        linkColor: Theme.primary
        font.family: Theme.fontSans; font.pointSize: 10
        onLinkActivated: l => Qt.openUrlExternally(l)
      }
    }

    DataTable { anchors.fill: parent; visible: kind === "table"; info: kind === "table" ? r : ({}) }

    // model metadata
    Flickable {
      anchors.fill: parent
      visible: kind === "model"
      contentHeight: mcol.height
      clip: true
      boundsBehavior: Flickable.StopAtBounds
      ScrollBar.vertical: Scroll {}
      Column {
        id: mcol
        width: parent.width
        spacing: 4
        Repeater {
          model: kind === "model" ? (r.fields || []) : []
          Rectangle {
            required property var modelData
            width: mcol.width; height: Math.max(30, fv.implicitHeight + 12); radius: 10
            color: Theme.containerHigh
            Txt { x: 10; width: 110; anchors.verticalCenter: parent.verticalCenter; text: modelData[0]; dim: true; font.pointSize: 9; elide: Text.ElideRight }
            Txt { id: fv; x: 124; width: parent.width - 134; anchors.verticalCenter: parent.verticalCenter; text: String(modelData[1]); font.pointSize: 9.5; mono: true; wrapMode: Text.WrapAnywhere }
          }
        }
        Txt { visible: (r.tensors || []).length > 0; text: qsTr("Tensors"); topPadding: 10; font.pointSize: 9; font.weight: Font.DemiBold; dim: true }
        Repeater {
          model: kind === "model" ? (r.tensors || []).slice(0, 40) : []
          Row {
            required property var modelData
            spacing: 8
            Txt { text: modelData.name; mono: true; font.pointSize: 8; width: mcol.width * 0.55; elide: Text.ElideMiddle }
            Txt { text: modelData.dtype; mono: true; font.pointSize: 8; color: Theme.tertiary; width: 56 }
            Txt { text: modelData.shape; mono: true; font.pointSize: 8; dim: true }
          }
        }
      }
    }

    // folder without README / binary / error / audio
    EmptyState {
      anchors.centerIn: parent
      visible: !pv.busy && (kind === "binary" || kind === "error" || kind === "audio" || (kind === "folder" && !r.readme) || (!entry && !pane.model.isLocal)
               || ((kind === "video" || kind === "pdf") && pimg.status !== Image.Ready))
      icon: kind === "error" ? "error" : kind === "folder" ? "folder_open" : kind === "audio" ? "music_note" : kind === "video" ? "movie" : kind === "pdf" ? "picture_as_pdf" : "draft"
      emblem: kind === "folder" ? "konoha" : "tomoe"
      title: kind === "error" ? qsTr("No preview") : kind === "binary" ? qsTr("Binary file") : kind === "folder" ? (entry ? qsTr("Folder") : qsTr("Select a file"))
           : kind === "video" || kind === "pdf" ? qsTr("No thumbnail") : kind === "audio" ? qsTr("Audio") : ""
      hint: kind === "error" ? (r.msg || "") : kind === "folder" ? qsTr("%1 folders · %2 files").arg(r.dirs).arg(r.files) : (kind === "video" || kind === "pdf") ? qsTr("Install ffmpegthumbs / kdegraphics-thumbnailers for thumbnails") : ""
    }
    Spinner { anchors.centerIn: parent; visible: pv.busy }
  }
}
