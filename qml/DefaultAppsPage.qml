import QtQuick
import QtQuick.Controls
import Fuin
// "Aplicaciones por defecto": default application per file type. Everything comes from the system
// (MIME database, installed .desktop files, ~/.config/mimeapps.list); choosing an app writes there.
Item {
  id: page
  property string expanded: ""   // MIME type whose candidates are shown
  property string status: ""
  property bool statusError: false

  function ensureLoaded() { if (!DefaultApps.loaded) DefaultApps.reload() }
  function focusMime(mime) {
    search.text = ""
    const row = DefaultApps.focusMimeType(mime)
    if (row < 0) { ensureLoaded(); return }
    expanded = DefaultApps.mimeTypeAt(row)
    list.currentIndex = row
    Qt.callLater(() => list.positionViewAtIndex(row, ListView.Beginning))
  }
  function focusSearch() { search.forceActiveFocus() }

  Connections {
    target: DefaultApps
    function onDefaultChanged(mime, app) { page.statusError = false; page.status = "«" + mime + "» se abrirá con " + app }
    function onFailed(text) { page.statusError = true; page.status = text }
    function onFilterChanged() {
      if (search.text !== DefaultApps.filter) search.text = DefaultApps.filter
      list.currentIndex = -1
      list.positionViewAtBeginning()
    }
  }

  Column {
    id: header
    width: parent.width
    spacing: 4
    Row {
      spacing: 10
      Icon { text: "apps"; color: Theme.primary; size: 16; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: "Aplicaciones por defecto"; font.pointSize: 14; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
      Spinner { visible: DefaultApps.busy; width: 18; height: 18; anchors.verticalCenter: parent.verticalCenter }
    }
    Txt { width: parent.width; dim: true; font.pointSize: 9; wrapMode: Text.Wrap
          text: "Se guardan en ~/.config/mimeapps.list y las usa todo el escritorio. El icono de terminal marca las aplicaciones que se abren en un terminal." }
    Item { width: 1; height: 6 }
    Field {
      id: search
      width: parent.width
      icon: "search"
      placeholderText: "Buscar por tipo o extensión (md, py, image/png, vídeo…)"
      onTextChanged: filterDelay.restart()
      Keys.onDownPressed: { list.forceActiveFocus(); if (list.currentIndex < 0) list.currentIndex = 0 }
      Keys.onReturnPressed: if (list.count) { list.currentIndex = 0; page.expanded = DefaultApps.mimeTypeAt(0) }
    }
    Timer { id: filterDelay; interval: 150; onTriggered: DefaultApps.filter = search.text }
    Txt { visible: page.status !== ""; width: parent.width; text: page.status; font.pointSize: 9; elide: Text.ElideRight
          color: page.statusError ? Theme.error : Theme.primary }
  }

  ListView {
    id: list
    anchors.top: header.bottom; anchors.topMargin: 8
    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
    clip: true
    spacing: 2
    model: DefaultApps
    currentIndex: -1
    keyNavigationEnabled: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: Scroll {}
    Keys.onReturnPressed: if (currentItem) page.expanded = currentItem.open ? "" : currentItem.mimeType
    Keys.onSpacePressed: if (currentItem) page.expanded = currentItem.open ? "" : currentItem.mimeType
    // Section titles are part of the delegate (sectionStart role) so they always match the rows.
    delegate: Item {
      id: row
      required property int index
      required property string mimeType
      required property string comment
      required property string iconName
      required property string patterns
      required property string section
      required property bool sectionStart
      required property string defaultId
      required property string defaultName
      required property string defaultIcon
      required property bool defaultTerminal
      required property var apps
      readonly property bool open: page.expanded === mimeType
      width: list.width - 12
      height: card.y + card.height

      Txt { id: sectionTitle; visible: row.sectionStart; x: 6; height: visible ? 30 : 0; verticalAlignment: Text.AlignBottom; bottomPadding: 4
            text: row.section; dim: true; font.pointSize: 9; font.weight: Font.DemiBold }

      Rectangle {
        id: card
        y: sectionTitle.height
        width: parent.width
        height: head.height + (row.open ? chips.height + 12 : 0)
        radius: 14
        color: row.open ? Theme.containerHighest : row.ListView.isCurrentItem || hl.containsMouse ? Theme.containerHigh : "transparent"
        Behavior on color { CAnim {} }

        Item {
          id: head
          width: parent.width; height: 54
          StateLayer { id: hl; radius: 14; onClicked: { list.currentIndex = row.index; page.expanded = row.open ? "" : row.mimeType } }
          SysIcon { id: mic; x: 12; anchors.verticalCenter: parent.verticalCenter; name: row.iconName; glyph: "description"; size: 26 }
          Column {
            anchors.left: mic.right; anchors.leftMargin: 12; anchors.right: pill.left; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Txt { width: parent.width; text: row.comment; font.pointSize: 10.5; elide: Text.ElideRight }
            Txt { width: parent.width; text: row.mimeType + (row.patterns ? "   " + row.patterns : ""); dim: true; mono: true; font.pointSize: 8.5; elide: Text.ElideRight }
          }
          Rectangle {
            id: pill
            anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
            height: 32; radius: 16
            width: prow.implicitWidth + 24
            color: row.defaultId !== "" ? Theme.secondaryContainer : "transparent"
            border.width: row.defaultId !== "" ? 0 : 1
            border.color: Theme.outlineVariant
            Row {
              id: prow
              anchors.centerIn: parent
              spacing: 6
              SysIcon { visible: row.defaultId !== ""; name: row.defaultIcon; glyph: "apps"; size: 16; color: Theme.textOnSecondaryContainer; anchors.verticalCenter: parent.verticalCenter }
              Txt { width: Math.min(implicitWidth, 200); text: row.defaultId !== "" ? row.defaultName : "Sin aplicación"; elide: Text.ElideRight; font.pointSize: 10
                    color: row.defaultId !== "" ? Theme.textOnSecondaryContainer : Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
              Icon { visible: row.defaultTerminal; text: "terminal"; size: 12; color: Theme.textOnSecondaryContainer; anchors.verticalCenter: parent.verticalCenter }
              Icon { text: row.open ? "expand_less" : "expand_more"; size: 13; color: Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
            }
          }
        }

        Flow {
          id: chips
          visible: row.open
          x: 14; y: head.height
          width: parent.width - 28
          spacing: 6
          Repeater {
            model: row.open ? row.apps : []
            Rectangle {
              id: chip
              required property var modelData
              height: 32; radius: 16
              width: crow.implicitWidth + 22
              color: modelData.isDefault ? Theme.secondaryContainer : Theme.containerHigh
              border.width: modelData.isDefault ? 0 : 1
              border.color: Theme.outlineVariant
              opacity: DefaultApps.busy && !modelData.isDefault ? 0.5 : 1
              Row {
                id: crow
                anchors.centerIn: parent
                spacing: 6
                Icon { visible: chip.modelData.isDefault; text: "check"; size: 12; color: Theme.textOnSecondaryContainer; anchors.verticalCenter: parent.verticalCenter }
                SysIcon { name: chip.modelData.icon; glyph: "apps"; size: 16; anchors.verticalCenter: parent.verticalCenter }
                Txt { text: chip.modelData.name; font.pointSize: 10; anchors.verticalCenter: parent.verticalCenter
                      color: chip.modelData.isDefault ? Theme.textOnSecondaryContainer : Theme.textMain }
                Icon { visible: chip.modelData.terminal; text: "terminal"; size: 12; color: Theme.textDim; anchors.verticalCenter: parent.verticalCenter }
              }
              StateLayer {
                id: cs
                radius: 16
                enabled: !DefaultApps.busy && !chip.modelData.isDefault
                onClicked: DefaultApps.setDefault(row.mimeType, chip.modelData.id)
              }
              ToolTip.visible: cs.containsMouse
              ToolTip.delay: 600
              ToolTip.text: chip.modelData.id + (chip.modelData.terminal ? " · se abre en un terminal" : "") + (chip.modelData.isDefault ? " · actual" : "")
            }
          }
          Txt { visible: row.apps.length === 0; text: "No hay aplicaciones instaladas para este tipo."; dim: true; font.pointSize: 9.5; height: 32 }
        }
      }
    }
  }

  EmptyState {
    anchors.centerIn: list
    visible: DefaultApps.loaded && DefaultApps.count === 0
    icon: "search_off"
    title: "Sin resultados"
    hint: "Prueba con una extensión (md, py, png) o un tipo MIME (text/plain)."
  }
  Spinner { anchors.centerIn: list; visible: !DefaultApps.loaded; width: 28; height: 28 }
}
