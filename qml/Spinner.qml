import QtQuick
import Fuin
// Loading indicator: a spinning tomoe (static when animations are off).
Tomoe {
  id: root
  property bool running: visible
  width: 22; height: 22
  RotationAnimation on rotation { running: root.running && Theme.animations; loops: Animation.Infinite; from: 0; to: 360; duration: 900 }
}
