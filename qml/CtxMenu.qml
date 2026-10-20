import QtQuick
import QtQuick.Controls
import Fuin
// Context menu. items: [{ label, icon, shortcut, enabled, danger, sep, sub: [items], run: fn }]
// Keyboard: ↑/↓ move, Enter runs, → opens a submenu, ← / Esc goes back.
Popup {
  id: root
  property var items: []
  property var subItems: null
  property string subTitle: ""
  property int hi: -1
  readonly property var shown: subItems !== null ? subItems : items
  padding: 6
  width: 300
  modal: false
  focus: true
  closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
  function openAt(px, py, list) {
    items = list; subItems = null; hi = -1
    x = px; y = py
    open()
    Qt.callLater(clamp)
  }
  // keep the whole menu inside the window (it may open near an edge or be taller than the space below)
  function clamp() {
    const p = parent
    if (!p || !opened) return
    if (x + width > p.width) x = Math.max(4, p.width - width - 4)
    if (y + height > p.height) y = Math.max(4, p.height - height - 4)
  }
  onHeightChanged: clamp()
  function step(d) {
    const n = shown.length
    for (let k = 1; k <= n; k++) {
      const i = (hi + d * k + n * 2) % n
      const it = shown[i]
      if (!it.sep && (it.enabled === undefined || it.enabled)) { hi = i; return }
    }
  }
  function activate(it) {
    if (!it || it.sep || it.enabled === false) return
    if (it.sub) { subTitle = it.label; subItems = it.sub; hi = -1; return }
    close()
    if (it.run) it.run()
  }
  enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dur(90) } }
  background: Rectangle {
    radius: 16
    color: Theme.containerHigh
    border.width: 1
    border.color: Theme.outlineVariant
  }
  contentItem: FocusScope {
    implicitHeight: col.implicitHeight
    focus: true
    Keys.onDownPressed: root.step(1)
    Keys.onUpPressed: root.step(-1)
    Keys.onReturnPressed: root.activate(root.shown[root.hi])
    Keys.onEnterPressed: root.activate(root.shown[root.hi])
    Keys.onRightPressed: { const it = root.shown[root.hi]; if (it && it.sub) root.activate(it) }
    Keys.onLeftPressed: if (root.subItems !== null) { root.subItems = null; root.hi = -1 }
    Column {
      id: col
      width: parent.width
      spacing: 0
      Rectangle {
        visible: root.subItems !== null
        width: parent.width; height: 32; radius: 8; color: "transparent"
        Row { anchors.verticalCenter: parent.verticalCenter; x: 8; spacing: 8
          Icon { text: "arrow_back"; size: 13; color: Theme.textDim }
          Txt { text: root.subTitle; color: Theme.textDim; font.pointSize: 10 } }
        StateLayer { onClicked: { root.subItems = null; root.hi = -1 } }
      }
      Repeater {
        model: root.shown
        Item {
          id: it
          required property var modelData
          required property int index
          readonly property bool sep: !!modelData.sep
          readonly property bool on: modelData.enabled === undefined || modelData.enabled
          width: root.availableWidth
          height: sep ? 9 : 32
          Rectangle { visible: it.sep; anchors.centerIn: parent; width: parent.width - 12; height: 1; color: Theme.outlineVariant }
          Rectangle {
            visible: !it.sep
            anchors.fill: parent
            radius: 8
            color: root.hi === it.index ? Qt.alpha(Theme.textMain, 0.08) : "transparent"
            opacity: it.on ? 1 : 0.4
            Icon { id: ic; x: 8; anchors.verticalCenter: parent.verticalCenter; text: it.modelData.icon || ""; size: 13; color: it.modelData.danger ? Theme.error : Theme.textDim; width: 20 }
            Txt { anchors.left: ic.right; anchors.leftMargin: 8; anchors.right: sc.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                  text: it.modelData.label || ""; elide: Text.ElideRight; font.pointSize: 10.5; color: it.modelData.danger ? Theme.error : Theme.textMain }
            Txt { id: sc; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                  text: it.modelData.sub ? "›" : (it.modelData.shortcut || ""); dim: true; font.pointSize: 9.5; mono: !it.modelData.sub }
            StateLayer { enabled: it.on; onClicked: root.activate(it.modelData); onContainsMouseChanged: if (containsMouse) root.hi = it.index }
          }
        }
      }
    }
  }
}
