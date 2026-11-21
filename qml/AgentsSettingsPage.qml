import QtQuick
import Fuin
SettingsPage {
  title: qsTr("Agents (MCP)")
  icon: "hub"
  description: qsTr("What AI agents connected through «fuin --mcp» may see and do. Changes apply to the next request, without restarting the agent.")

  SettingGroup {
    label: qsTr("Allow agents to write")
    Row {
      spacing: 10; width: parent.width
      TomoeSwitch { checked: Config.mcpAllowWrite; onToggled: v => Config.mcpAllowWrite = v; anchors.verticalCenter: parent.verticalCenter }
      Txt { width: parent.width - 60; wrapMode: Text.Wrap; dim: true; font.pointSize: 8.5; anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Create, copy, move and move to the trash. Every change asks for your confirmation and can be undone.") }
    }
  }
  SettingGroup {
    label: qsTr("Roots")
    hint: Config.mcpRootsSetting.length ? qsTr("Agents can only reach files inside these folders. Secret files stay hidden either way.")
                                        : qsTr("No roots: agents cannot reach any file.")
    ListSetting {
      model: Config.mcpRootsSetting
      parts: [{ placeholder: "~", weight: 1 }]
      addLabel: qsTr("Add folder")
      onEdited: list => Config.mcpRootsSetting = list
      onReset: Config.resetToDefault("mcp/roots")
    }
  }
}
