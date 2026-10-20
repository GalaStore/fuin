import QtQuick
import Fuin
// Standard M3 easing; duration follows the user's animation settings (0 when disabled).
NumberAnimation { property int ms: 200; duration: Theme.dur(ms); easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0, 0, 1, 1, 1] }
