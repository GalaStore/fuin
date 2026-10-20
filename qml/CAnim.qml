import QtQuick
import Fuin
ColorAnimation { property int ms: 200; duration: Theme.dur(ms); easing.type: Easing.BezierSpline; easing.bezierCurve: [0.2, 0, 0, 1, 1, 1] }
