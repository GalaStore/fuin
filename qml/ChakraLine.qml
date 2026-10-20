import QtQuick
import Fuin
// Chakra gradient accent (selected nav item, active tab, progress).
Rectangle {
  property bool vertical: false
  radius: Math.min(width, height) / 2
  gradient: Gradient {
    orientation: vertical ? Gradient.Vertical : Gradient.Horizontal
    GradientStop { position: 0; color: Theme.primary }
    GradientStop { position: 0.6; color: Theme.tertiary }
    GradientStop { position: 1; color: Theme.secondary }
  }
}
