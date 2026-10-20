import QtQuick
import Archivos

// Root object: owns the windows and routes IPC requests (CLI --reveal, MCP agents) to them.
QtObject {
  id: app
  required property string startUrl
  required property string revealUrl
  required property bool startPreview

  property var windows: []
  readonly property var focused: {
    for (const w of windows) if (w.active) return w
    return windows.length ? windows[windows.length - 1] : null
  }
  property Component winComp: Component { AppWindow {} }

  function openWindow(url, reveal, preview) {
    const w = winComp.createObject(null, { startUrl: url || "", revealUrl: reveal || "", startPreview: !!preview })
    w.closing2.connect(() => { windows = windows.filter(x => x !== w); w.destroy(); if (!windows.length) Qt.quit() })
    windows = windows.concat([w])
    return w
  }

  property Connections ipc: Connections {
    target: Ipc
    function onOpenRequested(url, newWindow) {
      if (newWindow || !app.focused) app.openWindow(url)
      else { app.focused.newTab(url); app.focused.raise(); app.focused.requestActivate() }
    }
    function onRevealRequested(url) { if (app.focused) app.focused.reveal(url, false); else app.openWindow("", url, false) }
    function onPreviewRequested(url) { if (app.focused) app.focused.reveal(url, true); else app.openWindow("", url, true) }
    function onDevRequested(a) { if (app.focused) app.focused.dev(a) }
    function onConfirmRequested(id, title, detail, client) { if (app.focused) app.focused.askAgent(id, title, detail, client); else Ipc.resolve(id, false) }
  }

  Component.onCompleted: openWindow(startUrl, revealUrl, startPreview)
}
