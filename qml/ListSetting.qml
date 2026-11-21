import QtQuick
import QtQuick.Controls
import Fuin
// Editable list of fuin.conf entries. Each entry has one or more parts joined with "|"
// (parts: [{ placeholder, weight }]); pathPart marks the part checked as an existing folder.
Column {
  id: root
  property var model: []
  property var parts: [{ placeholder: "~/folder", weight: 1 }]
  property int pathPart: 0
  property string addLabel: qsTr("Add")
  property bool canReset: true
  property var rows: []
  signal edited(var list)
  signal reset()
  width: parent ? parent.width : 0
  spacing: 6

  property bool editing: false
  // Rebuilding the rows drops keyboard focus, so only do it when the list really differs.
  function sync(list) { if (JSON.stringify(list) !== JSON.stringify(rows)) rows = list.slice() }
  onModelChanged: if (!editing) sync(model)
  Component.onCompleted: rows = model.slice()

  function commit() {
    const out = []
    for (let i = 0; i < rep.count; ++i) {
      const r = rep.itemAt(i)
      if (r && r.joined().replace(/\|/g, "").trim() !== "") out.push(r.joined())
    }
    editing = false
    sync(out)
    edited(out)
  }

  Repeater {
    id: rep
    model: root.rows
    Row {
      id: entry
      required property int index
      required property var modelData
      readonly property var values: String(modelData).split("|")
      readonly property real totalWeight: root.parts.reduce((a, p) => a + p.weight, 0)
      readonly property real fieldsWidth: root.width - del.width - (root.parts.length) * spacing
      function joined() {
        const v = []
        for (let i = 0; i < fields.count; ++i) v.push(fields.itemAt(i).text.trim())
        while (v.length > 1 && v[v.length - 1] === "") v.pop()
        return v.join("|")
      }
      spacing: 6
      Repeater {
        id: fields
        model: root.parts
        Field {
          required property int index
          required property var modelData
          readonly property bool missing: index === root.pathPart && text.trim() !== "" && !Config.isDir(text)
          width: entry.fieldsWidth * modelData.weight / entry.totalWeight
          mono: index === root.pathPart
          text: entry.values[index] || ""
          placeholderText: modelData.placeholder
          onTextEdited: root.editing = true
          onEditingFinished: if (root.editing) root.commit()
          Icon { visible: parent.missing; text: "folder_off"; color: Theme.textDim; size: 13
                 anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter }
          ToolTip.visible: missing && hovered
          ToolTip.text: qsTr("This folder does not exist; it is hidden until it does.")
          Component.onCompleted: if (entry.modelData === "" && index === 0) forceActiveFocus()
        }
      }
      IconBtn { id: del; size: 36; icon: "close"; tip: qsTr("Remove")
                onClicked: { const r = root.rows.slice(); r.splice(entry.index, 1); root.rows = r; root.commit() } }
    }
  }
  Row {
    spacing: 8
    Btn { kind: "outline"; compact: true; icon: "add"; text: root.addLabel
          onClicked: { root.editing = true; root.rows = root.rows.concat([""]) } }
    Btn { visible: root.canReset; kind: "text"; compact: true; icon: "restart_alt"; text: qsTr("Restore defaults"); onClicked: root.reset() }
  }
}
