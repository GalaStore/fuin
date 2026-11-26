import QtQuick

// Drag source for files. It is its own Drag.source: a DropArea ignores drags whose source is one of
// its ancestors, and the pane (the former source) contains every folder's drop area.
Item {
  id: root
  property var pane
  property int dragType: Drag.Automatic
  Drag.dragType: root.dragType
  Drag.supportedActions: Qt.CopyAction | Qt.MoveAction | Qt.LinkAction
  Drag.proposedAction: Qt.MoveAction
  Drag.source: root
}
