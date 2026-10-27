import QtQuick
import QtQuick.Controls
import Fuin
import "Glyphs.js" as G

// IA panel: context packs for chats, MCP connection for agents, semantic search (feature-flagged).
Item {
  id: root
  property var pane
  property var win
  readonly property var targets: pane ? (pane.model.selectionCount > 0 ? pane.model.selectedUrls() : (pane.model.isLocal ? [pane.model.url] : [])) : []
  readonly property var pack: win ? win.contextPack.last : ({})
  property string activity: ""

  component Card: Rectangle {
    default property alias content: inner.data
    property string title
    property string icon
    width: parent.width
    height: inner.implicitHeight + 50
    radius: 16
    color: Theme.containerHigh
    Row { x: 14; y: 12; spacing: 8
      Icon { text: icon; size: 13; fill: 1; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: title; font.pointSize: 10.5; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter } }
    Column { id: inner; x: 14; y: 40; width: parent.width - 28; spacing: 8 }
  }
  component Code: Rectangle {
    property string code
    width: parent.width; height: ct.implicitHeight + 16; radius: 10
    color: Theme.containerLowest
    Txt { id: ct; x: 10; y: 8; width: parent.width - 52; text: code; mono: true; font.pointSize: 8; wrapMode: Text.WrapAnywhere; color: Theme.textMain }
    IconBtn { anchors.right: parent.right; anchors.rightMargin: 4; anchors.top: parent.top; anchors.topMargin: 4; size: 26; icon: "content_copy"; tip: qsTr("Copy"); onClicked: FileOps.copyText(code) }
  }

  Flickable {
    anchors.fill: parent
    anchors.margins: 12
    contentHeight: col.height + 20
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: Scroll {}
    Column {
      id: col
      width: parent.width
      spacing: 10

      // ---------- 1. context pack
      Card {
        title: qsTr("AI context"); icon: "auto_awesome"
        Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 9
              text: root.pane && root.pane.model.selectionCount > 0
                    ? qsTr("Bundles %1 selected item(s) into a single document (tree + contents) to paste into Claude, Cursor or ChatGPT. Respects .gitignore and skips binaries, lockfiles and secrets.").arg(root.pane.model.selectionCount)
                    : qsTr("Bundles the current folder into a single document (tree + contents) to paste into Claude, Cursor or ChatGPT. Respects .gitignore and skips binaries, lockfiles and secrets.") }
        Segmented {
          options: [{ value: "markdown", label: "Markdown", icon: "markdown" }, { value: "xml", label: "XML", icon: "code" }]
          value: Config.contextFormat
          onPicked: v => Config.contextFormat = v
        }
        Row {
          spacing: 8
          Btn { kind: "filled"; icon: "content_copy"; text: win && win.contextPack.busy ? qsTr("Preparing…") : qsTr("Copy context"); enabled2: root.targets.length > 0 && !(win && win.contextPack.busy)
                onClicked: win.copyContext(root.targets) }
          Btn { kind: "text"; icon: "terminal"; text: "CLI"; onClicked: FileOps.copyText("fuin --context " + root.targets.map(u => "'" + AppInfo.toLocal(u) + "'").join(" ") + " | wl-copy") }
        }
        // result
        Column {
          visible: root.pack.files !== undefined
          width: parent.width; spacing: 6
          Flow {
            width: parent.width; spacing: 6
            Repeater {
              model: [
                { t: qsTr("≈ %1 tokens").arg(G.count(root.pack.tokens)), i: "token", c: Theme.primaryContainer, f: Theme.textOnPrimaryContainer },
                { t: qsTr("%1 files · %2").arg(root.pack.files || 0).arg(G.size(root.pack.bytes)), i: "description", c: Theme.secondaryContainer, f: Theme.textOnSecondaryContainer },
                { t: qsTr("%1 skipped").arg((root.pack.skipped || []).length), i: "block", c: Theme.containerHighest, f: Theme.textMain },
                { t: root.pack.secretsBlocked === 1 ? qsTr("1 secret blocked") : qsTr("%1 secrets blocked").arg(root.pack.secretsBlocked || 0), i: "key", c: Theme.errorContainer, f: Theme.textOnErrorContainer, hide: !root.pack.secretsBlocked }
              ].filter(x => !x.hide)
              Rectangle {
                required property var modelData
                height: 24; radius: 12; width: rr.implicitWidth + 16; color: modelData.c
                Row { id: rr; anchors.centerIn: parent; spacing: 4
                  Icon { text: modelData.i; size: 10; color: modelData.f; anchors.verticalCenter: parent.verticalCenter }
                  Txt { text: modelData.t; font.pointSize: 8.5; color: modelData.f; anchors.verticalCenter: parent.verticalCenter } }
              }
            }
          }
          Txt { visible: (root.pack.tokens || 0) > 150000; width: parent.width; wrapMode: Text.Wrap; font.pointSize: 8.5; color: Theme.error
                text: qsTr("Careful: over ~150 K tokens; it may not fit in the model's context. Select fewer files.") }
          Repeater {
            model: (root.pack.skipped || []).slice(0, 6)
            Txt { required property var modelData; width: col.width - 28; elide: Text.ElideMiddle; dim: true; mono: true; font.pointSize: 8
                  text: "– " + modelData.path + "  (" + modelData.reason + ")" }
          }
        }
      }

      // ---------- 2. MCP
      Card {
        title: qsTr("Agents (MCP)"); icon: "hub"
        Row {
          spacing: 8
          Rectangle { width: 8; height: 8; radius: 4; color: Ipc.listening ? Theme.tertiary : Theme.error; anchors.verticalCenter: parent.verticalCenter }
          Txt { width: parent.parent.width - 16; wrapMode: Text.Wrap; text: Ipc.listening ? qsTr("Connected: agents see your current folder and selection") : qsTr("Local socket unavailable"); font.pointSize: 9; anchors.verticalCenter: parent.verticalCenter }
        }
        Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 9
              text: qsTr("Claude Code, Cursor and other agents can list, read (without secrets), search, inspect data and model schemas, build context packs and show you files here.") }
        Txt { text: "Claude Code"; font.pointSize: 9; font.weight: Font.DemiBold }
        Code { code: AppInfo.claudeCommand() }
        Txt { text: qsTr("Cursor (~/.cursor/mcp.json) and other clients"); font.pointSize: 9; font.weight: Font.DemiBold }
        Code { code: AppInfo.mcpJson() }
        Row {
          spacing: 10; width: parent.width
          TomoeSwitch { checked: Config.mcpAllowWrite; onToggled: v => Config.mcpAllowWrite = v; anchors.verticalCenter: parent.verticalCenter }
          Column {
            width: parent.width - 60; anchors.verticalCenter: parent.verticalCenter
            Txt { text: qsTr("Allow agents to write"); font.pointSize: 9.5 }
            Txt { text: qsTr("Create, copy, move and move to the trash. Every change asks for your confirmation and can be undone."); dim: true; font.pointSize: 8.5; width: parent.width; wrapMode: Text.Wrap }
          }
        }
        Txt { visible: root.activity !== ""; text: qsTr("Last: %1").arg(root.activity); dim: true; font.pointSize: 8.5; width: parent.width; elide: Text.ElideRight }
      }

      // ---------- 3. semantic search (flagged)
      Card {
        title: qsTr("Ask your files"); icon: "travel_explore"
        opacity: AppInfo.semanticSearchEnabled ? 1 : 0.75
        Field { width: parent.width; icon: "search"; enabled: AppInfo.semanticSearchEnabled; placeholderText: qsTr("Where is the DAG that loads sales into BigQuery?") }
        Txt { width: parent.width; wrapMode: Text.Wrap; font.pointSize: 8.5; dim: true
              text: AppInfo.hasOllama ? qsTr("Ollama detected. Semantic search (local embeddings, nothing leaves your computer) will arrive in a future version.")
                                      : qsTr("Disabled: it needs Ollama with an embeddings model (e.g. nomic-embed-text). Everything would be computed locally; nothing leaves your computer.") }
      }

      Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 8; horizontalAlignment: Text.AlignHCenter
            text: qsTr("Files marked 🔑 (.env, keys, tokens) are never sent to the AI.") }
    }
  }
  Connections { target: Ipc; function onAgentActivity(text) { root.activity = text } }
}
