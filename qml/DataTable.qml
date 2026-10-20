import QtQuick
import QtQuick.Controls
import Archivos
import "Glyphs.js" as G

// Table preview with a schema strip (type, nulls, min/max) for CSV/TSV/JSON/JSONL/Parquet.
Item {
  id: root
  property var info: ({})
  readonly property var cols: info.columns || []
  readonly property var rows: info.rows || []
  property bool showSchema: true
  readonly property real cw: 128

  Column {
    id: head
    width: parent.width
    spacing: 8
    Flow {
      width: parent.width
      spacing: 6
      Repeater {
        model: [
          { t: root.info.totalRows !== undefined && root.info.totalRows !== null ? G.count(root.info.totalRows) + " filas" : "", i: "table_rows" },
          { t: root.cols.length + " columnas", i: "view_column" },
          { t: root.info.engine || "", i: "bolt" }
        ].filter(x => x.t !== "")
        Rectangle {
          required property var modelData
          height: 24; radius: 12; width: cr.implicitWidth + 16
          color: Theme.secondaryContainer
          Row { id: cr; anchors.centerIn: parent; spacing: 4
            Icon { text: modelData.i; size: 10; color: Theme.textOnSecondaryContainer; anchors.verticalCenter: parent.verticalCenter }
            Txt { text: modelData.t; font.pointSize: 8.5; color: Theme.textOnSecondaryContainer; anchors.verticalCenter: parent.verticalCenter } }
        }
      }
    }
    Segmented {
      options: [{ value: true, label: "Esquema", icon: "schema" }, { value: false, label: "Datos", icon: "table" }]
      value: root.showSchema
      onPicked: v => root.showSchema = v
    }
    Txt { visible: !!root.info.warning; text: root.info.warning || ""; color: Theme.error; font.pointSize: 9; width: parent.width; wrapMode: Text.Wrap }
  }

  // schema: one rounded row per column
  ListView {
    visible: root.showSchema
    anchors.top: head.bottom; anchors.topMargin: 10
    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
    clip: true
    spacing: 4
    model: root.cols
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: Scroll {}
    footer: Txt {
      width: ListView.view ? ListView.view.width : 0; topPadding: 8; dim: true; font.pointSize: 8; wrapMode: Text.Wrap
      text: root.info.exactStats ? "Estadísticas exactas del pie del Parquet (sin leer los datos)." : root.info.statsSample ? "Tipos y estadísticas inferidos de las primeras " + root.info.statsSample + " filas." : ""
    }
    delegate: Rectangle {
      required property var modelData
      required property int index
      width: ListView.view.width; height: sc.implicitHeight + 14; radius: 12
      color: Theme.containerHigh
      Column {
        id: sc
        x: 10; y: 7; width: parent.width - 20; spacing: 3
        Row {
          spacing: 8; width: parent.width
          Txt { text: modelData.name; mono: true; font.pointSize: 9.5; font.weight: Font.DemiBold; color: Theme.primary; elide: Text.ElideRight; width: Math.min(implicitWidth, parent.width - tb.width - 12) }
          Rectangle { id: tb; height: 18; radius: 9; width: tt.implicitWidth + 12; color: Theme.tertiaryContainer; anchors.verticalCenter: parent.verticalCenter
            Txt { id: tt; anchors.centerIn: parent; text: String(modelData.type || "").slice(0, 28); mono: true; font.pointSize: 7.5; color: Theme.textOnTertiaryContainer } }
        }
        Txt {
          visible: text !== ""
          width: parent.width; elide: Text.ElideRight; dim: true; mono: true; font.pointSize: 8
          text: [modelData.nulls !== undefined ? "nulos " + G.count(modelData.nulls) : "",
                 modelData.min !== undefined ? "min " + modelData.min : "",
                 modelData.max !== undefined ? "max " + modelData.max : ""].filter(s => s).join("  ·  ")
        }
      }
    }
  }

  // data grid
  Flickable {
    id: hflick
    visible: !root.showSchema
    anchors.top: head.bottom; anchors.topMargin: 10
    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
    contentWidth: root.cols.length * root.cw + 44
    clip: true
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.horizontal: Scroll {}
    ListView {
      id: rowsView
      width: Math.max(hflick.width, hflick.contentWidth); height: hflick.height
      clip: true
      model: root.showSchema ? [] : root.rows
      headerPositioning: ListView.OverlayHeader
      boundsBehavior: Flickable.StopAtBounds
      ScrollBar.vertical: Scroll {}
      header: Rectangle {
        z: 2; width: rowsView.width; height: 28; radius: 10; color: Theme.containerHigh
        Row { x: 44
          Repeater { model: root.cols
            Txt { required property var modelData; width: root.cw; height: 28; leftPadding: 6; text: modelData.name; elide: Text.ElideRight; font.pointSize: 8.5; font.weight: Font.DemiBold; mono: true; color: Theme.primary } } }
      }
      delegate: Rectangle {
        required property var modelData
        required property int index
        width: rowsView.width; height: 24; radius: 6
        color: index % 2 ? Qt.alpha(Theme.containerHigh, 0.45) : "transparent"
        Txt { width: 38; height: 24; text: index + 1; dim: true; mono: true; font.pointSize: 7.5; horizontalAlignment: Text.AlignRight }
        Row { x: 44
          Repeater { model: modelData
            Txt { required property var modelData; width: root.cw; height: 24; leftPadding: 6; text: modelData === null || modelData === undefined ? "∅" : String(modelData)
                  elide: Text.ElideRight; font.pointSize: 8.5; mono: true; dim: modelData === null || modelData === undefined } } }
      }
    }
  }
}
