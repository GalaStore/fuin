import QtQuick
import QtQuick.Controls
import Archivos
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
    IconBtn { anchors.right: parent.right; anchors.rightMargin: 4; anchors.top: parent.top; anchors.topMargin: 4; size: 26; icon: "content_copy"; tip: "Copiar"; onClicked: FileOps.copyText(code) }
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
        title: "Contexto para IA"; icon: "auto_awesome"
        Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 9
              text: "Empaqueta " + (root.pane && root.pane.model.selectionCount > 0 ? root.pane.model.selectionCount + " elemento(s) seleccionado(s)" : "la carpeta actual") +
                    " como un único documento (árbol + contenido) para pegar en Claude, Cursor o ChatGPT. Respeta .gitignore y omite binarios, lockfiles y secretos." }
        Segmented {
          options: [{ value: "markdown", label: "Markdown", icon: "markdown" }, { value: "xml", label: "XML", icon: "code" }]
          value: Config.contextFormat
          onPicked: v => Config.contextFormat = v
        }
        Row {
          spacing: 8
          Btn { kind: "filled"; icon: "content_copy"; text: win && win.contextPack.busy ? "Preparando…" : "Copiar contexto"; enabled2: root.targets.length > 0 && !(win && win.contextPack.busy)
                onClicked: win.copyContext(root.targets) }
          Btn { kind: "text"; icon: "terminal"; text: "CLI"; onClicked: FileOps.copyText("archivos --context " + root.targets.map(u => "'" + AppInfo.toLocal(u) + "'").join(" ") + " | wl-copy") }
        }
        // result
        Column {
          visible: root.pack.files !== undefined
          width: parent.width; spacing: 6
          Flow {
            width: parent.width; spacing: 6
            Repeater {
              model: [
                { t: "≈ " + G.count(root.pack.tokens) + " tokens", i: "token", c: Theme.primaryContainer, f: Theme.textOnPrimaryContainer },
                { t: (root.pack.files || 0) + " archivos · " + G.size(root.pack.bytes), i: "description", c: Theme.secondaryContainer, f: Theme.textOnSecondaryContainer },
                { t: (root.pack.skipped || []).length + " omitidos", i: "block", c: Theme.containerHighest, f: Theme.textMain },
                { t: (root.pack.secretsBlocked || 0) + (root.pack.secretsBlocked === 1 ? " secreto bloqueado" : " secretos bloqueados"), i: "key", c: Theme.errorContainer, f: Theme.textOnErrorContainer, hide: !root.pack.secretsBlocked }
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
                text: "Ojo: supera ~150 K tokens; puede no caber en el contexto del modelo. Selecciona menos archivos." }
          Repeater {
            model: (root.pack.skipped || []).slice(0, 6)
            Txt { required property var modelData; width: col.width - 28; elide: Text.ElideMiddle; dim: true; mono: true; font.pointSize: 8
                  text: "– " + modelData.path + "  (" + modelData.reason + ")" }
          }
        }
      }

      // ---------- 2. MCP
      Card {
        title: "Agentes (MCP)"; icon: "hub"
        Row {
          spacing: 8
          Rectangle { width: 8; height: 8; radius: 4; color: Ipc.listening ? Theme.tertiary : Theme.error; anchors.verticalCenter: parent.verticalCenter }
          Txt { width: parent.parent.width - 16; wrapMode: Text.Wrap; text: Ipc.listening ? "Conectado: los agentes ven tu carpeta y selección actuales" : "Socket local no disponible"; font.pointSize: 9; anchors.verticalCenter: parent.verticalCenter }
        }
        Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 9
              text: "Claude Code, Cursor y otros agentes pueden listar, leer (sin secretos), buscar, inspeccionar esquemas de datos y modelos, crear paquetes de contexto y mostrarte archivos aquí." }
        Txt { text: "Claude Code"; font.pointSize: 9; font.weight: Font.DemiBold }
        Code { code: AppInfo.claudeCommand() }
        Txt { text: "Cursor (~/.cursor/mcp.json) y otros clientes"; font.pointSize: 9; font.weight: Font.DemiBold }
        Code { code: AppInfo.mcpJson() }
        Row {
          spacing: 10; width: parent.width
          TomoeSwitch { checked: Config.mcpAllowWrite; onToggled: v => Config.mcpAllowWrite = v; anchors.verticalCenter: parent.verticalCenter }
          Column {
            width: parent.width - 60; anchors.verticalCenter: parent.verticalCenter
            Txt { text: "Permitir escritura a agentes"; font.pointSize: 9.5 }
            Txt { text: "Crear, copiar, mover y enviar a la papelera. Cada cambio te pide confirmación y se puede deshacer."; dim: true; font.pointSize: 8.5; width: parent.width; wrapMode: Text.Wrap }
          }
        }
        Txt { visible: root.activity !== ""; text: "Último: " + root.activity; dim: true; font.pointSize: 8.5; width: parent.width; elide: Text.ElideRight }
      }

      // ---------- 3. semantic search (flagged)
      Card {
        title: "Pregunta a tus archivos"; icon: "travel_explore"
        opacity: AppInfo.semanticSearchEnabled ? 1 : 0.75
        Field { width: parent.width; icon: "search"; enabled: AppInfo.semanticSearchEnabled; placeholderText: "¿Dónde está el DAG que carga ventas en BigQuery?" }
        Txt { width: parent.width; wrapMode: Text.Wrap; font.pointSize: 8.5; dim: true
              text: AppInfo.hasOllama ? "Ollama detectado. La búsqueda semántica (embeddings locales, nada sale de tu equipo) llegará en una próxima versión."
                                      : "Desactivada: necesita Ollama con un modelo de embeddings (p. ej. nomic-embed-text). Todo se calcularía en local; nada sale de tu equipo." }
      }

      Txt { width: parent.width; wrapMode: Text.Wrap; dim: true; font.pointSize: 8; horizontalAlignment: Text.AlignHCenter
            text: "Los archivos con 🔑 (.env, claves, tokens) nunca se envían a la IA." }
    }
  }
  Connections { target: Ipc; function onAgentActivity(text) { root.activity = text } }
}
