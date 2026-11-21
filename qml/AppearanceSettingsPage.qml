import QtQuick
import Fuin
SettingsPage {
  title: qsTr("Appearance")
  icon: "palette"
  description: qsTr("Motion and colours. Without values here, Fūin follows the desktop shell settings file, if there is one.")

  SettingGroup {
    label: qsTr("Animations")
    Row {
      spacing: 10; width: parent.width
      TomoeSwitch { checked: Theme.animations; onToggled: v => Config.setAnimations(v); anchors.verticalCenter: parent.verticalCenter }
      Txt { text: Theme.animations ? qsTr("On") : qsTr("Off"); font.pointSize: 9.5; anchors.verticalCenter: parent.verticalCenter }
    }
  }
  SettingGroup {
    label: qsTr("Animation speed")
    hint: qsTr("Higher is faster.")
    Segmented {
      enabled: Theme.animations
      opacity: enabled ? 1 : 0.45
      value: Theme.animationSpeed
      options: [0.5, 0.75, 1, 1.5, 2].map(s => ({ value: s, label: s + "×" }))
      onPicked: v => Config.setAnimationSpeed(v)
    }
    Btn { visible: Config.motionOverridden; kind: "text"; compact: true; icon: "restart_alt"; text: qsTr("Use the desktop shell values")
          onClicked: { Config.resetToDefault("appearance/animations"); Config.resetToDefault("appearance/animationSpeed") } }
  }
  SettingGroup {
    label: qsTr("Colours file")
    hint: qsTr("A Material 3 palette in JSON, such as the one matugen writes. It is watched, so new wallpapers recolour Fūin live.")
    SettingField {
      value: Config.colorsFileSetting
      placeholder: qsTr("Automatic: %1").arg(Config.colorsFile)
      status: Config.isFile(Config.colorsFile) ? "" : (value !== "" ? qsTr("File not found; the built-in colours are used.") : "")
      onEdited: v => Config.colorsFileSetting = v
    }
  }
  SettingGroup {
    label: qsTr("Desktop shell settings file")
    hint: qsTr("Optional JSON with «appearance.animations», «appearance.animationSpeed» and default «apps». Fūin only reads it.")
    SettingField {
      value: Config.shellSettingsFileSetting
      placeholder: qsTr("Automatic: %1").arg(Config.shellSettingsFile)
      status: value !== "" && !Config.isFile(value) ? qsTr("File not found.") : ""
      onEdited: v => Config.shellSettingsFileSetting = v
    }
  }
}
