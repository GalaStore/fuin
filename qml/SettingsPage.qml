import QtQuick
import QtQuick.Controls
import Fuin
// Scrollable settings page: title, one-line description and a column of settings.
Item {
  id: page
  property string title
  property string icon
  property string description
  default property alias content: col.data

  Column {
    id: header
    width: parent.width
    spacing: 4
    Row {
      spacing: 10
      Icon { text: page.icon; color: Theme.primary; size: 16; anchors.verticalCenter: parent.verticalCenter }
      Txt { text: page.title; font.pointSize: 14; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
    }
    Txt { visible: text !== ""; width: parent.width; dim: true; font.pointSize: 9; wrapMode: Text.Wrap; text: page.description }
  }
  Flickable {
    anchors.top: header.bottom; anchors.topMargin: 14
    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
    contentHeight: col.height + 12
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: Scroll {}
    Column { id: col; width: parent.width - 12; spacing: 18 }
  }
}
