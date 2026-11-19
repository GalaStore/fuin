import QtQuick
import Fuin

// Root of `fuin --portal`: one picker window per FileChooser request, answered through Portal.respond().
QtObject {
  id: root
  property var windows: ({})   // request id -> window
  property var focused: null
  property Component winComp: Component { AppWindow {} }

  property Connections portal: Connections {
    target: Portal
    function onRequested(req) {
      const w = root.winComp.createObject(null, { picker: req, startUrl: req.folder })
      let answered = false
      const answer = (ok, urls, filter) => { if (answered) return; answered = true; Portal.respond(req.id, ok, urls, filter) }
      w.picked.connect((ok, urls, filter) => { answer(ok, urls, filter); w.close() })
      w.closing2.connect(() => {
        answer(false, [], -1)
        delete root.windows[req.id]
        if (root.focused === w) root.focused = null
        w.destroy()
      })
      w.activeChanged.connect(() => { if (w.active) root.focused = w })
      root.windows[req.id] = w
      root.focused = w
      w.raise(); w.requestActivate()
    }
    function onCloseRequested(id) { const w = root.windows[id]; if (w) w.close() }
  }
  property Connections ipc: Connections {
    target: Ipc
    function onDevRequested(a) { if (root.focused) root.focused.dev(a) }
  }
}
