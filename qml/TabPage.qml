import QtQuick
import QtQuick.Controls
import Archivos

// A tab: one pane, or two side-by-side panes (split view, F3). The active pane wears the chakra ring.
Item {
  id: page
  property var win
  property string startUrl: ""
  property bool split: false
  property int activeIndex: 0
  readonly property var pane: split && activeIndex === 1 ? paneB : paneA
  readonly property string title: pane ? pane.model.title : ""
  readonly property bool isTrash: pane ? pane.model.isTrash : false

  function otherPane(p) { return split ? (p === paneA ? paneB : paneA) : null }
  function toggleSplit() {
    if (!split) { paneB.model.navigate(pane.model.url); split = true; activeIndex = 1; Qt.callLater(() => paneB.focusView()) }
    else { const keep = pane; split = false; if (keep === paneB) paneA.model.navigate(paneB.model.url); activeIndex = 0; paneA.focusView() }
  }
  function takeFocus() { pane.focusView() }

  SplitView {
    anchors.fill: parent
    orientation: Qt.Horizontal
    handle: Item {
      implicitWidth: 10
      Rectangle {
        anchors.centerIn: parent
        width: 4; height: 44; radius: 2
        color: SplitHandle.pressed ? Theme.primary : SplitHandle.hovered ? Theme.outline : Theme.outlineVariant
        Behavior on color { CAnim {} }
      }
    }
    component Card: ChakraFrame {
      id: card
      property alias paneItem: inner.data
      property bool on: false
      radius: 16
      borderWidth: 1.5
      fill: Theme.containerLowest
      ring: page.split && on
      ringOpacity: 0.7
      clip: true
      Item { id: inner; anchors.fill: parent; anchors.margins: 1.5 }
    }
    Card {
      SplitView.fillWidth: true
      SplitView.minimumWidth: 260
      on: page.activeIndex === 0
      Watermark { kind: "leaf"; width: 300; height: 300; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: -50; rotation: -24; opacity: 0.03 }
      paneItem: FolderPane { id: paneA; anchors.fill: parent; win: page.win; tab: page; active: page.activeIndex === 0; onFocused: page.activeIndex = 0 }
    }
    Card {
      visible: page.split
      SplitView.preferredWidth: page.width / 2
      SplitView.minimumWidth: 260
      on: page.activeIndex === 1
      Watermark { kind: "tomoe"; width: 220; height: 220; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: -40; rotation: 30; opacity: 0.03 }
      paneItem: FolderPane { id: paneB; anchors.fill: parent; win: page.win; tab: page; active: page.activeIndex === 1; onFocused: page.activeIndex = 1 }
    }
  }
  Component.onCompleted: paneA.model.navigate(startUrl)
}
