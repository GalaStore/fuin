import QtQuick
import Fuin
SettingsPage {
  title: qsTr("General")
  icon: "tune"
  description: qsTr("Programs Fūin starts for you. Leave a field empty to detect one automatically.")

  SettingGroup {
    label: qsTr("Terminal")
    hint: qsTr("Used by «Open terminal here» and for applications that run in a terminal. A program name or a full path, without arguments.")
    SettingField {
      value: Config.terminalSetting
      placeholder: Config.terminalDetected !== "" ? qsTr("Automatic: %1").arg(Config.terminalDetected) : qsTr("Automatic: none found")
      status: value !== "" && !Config.hasProgram(value) ? qsTr("Not found in PATH.") : ""
      onEdited: v => Config.terminalSetting = v
    }
  }
  SettingGroup {
    label: qsTr("Editor")
    hint: qsTr("Used by «Open in…» for folders and to open fuin.conf. A program name or a full path, without arguments.")
    SettingField {
      value: Config.editorSetting
      placeholder: Config.editorDetected !== "" ? qsTr("Automatic: %1").arg(Config.editorDetected) : qsTr("Automatic: none found")
      status: value !== "" && !Config.hasProgram(value) ? qsTr("Not found in PATH.") : ""
      onEdited: v => Config.editorSetting = v
    }
  }
}
