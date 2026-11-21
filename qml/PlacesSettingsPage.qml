import QtQuick
import Fuin
SettingsPage {
  title: qsTr("Places")
  icon: "bookmarks"
  description: qsTr("Folders shown in the sidebar. Folders that do not exist are skipped, so one list can serve several machines.")

  SettingGroup {
    label: qsTr("Project folders")
    hint: qsTr("Shown under «Projects». With a single folder the entry is just called «Projects».")
    ListSetting {
      model: Config.projectsSetting
      parts: [{ placeholder: "~/Dev", weight: 1 }]
      addLabel: qsTr("Add folder")
      onEdited: list => Config.projectsSetting = list
      onReset: Config.resetToDefault("places/projects")
    }
  }
  SettingGroup {
    label: qsTr("Extra places")
    hint: qsTr("Shown under «Places». The icon is a Material Symbols name such as «wallpaper» or «neurology»; empty means «folder».")
    ListSetting {
      model: Config.extraPlacesSetting
      parts: [{ placeholder: qsTr("Name"), weight: 2 }, { placeholder: "~/Pictures/Wallpapers", weight: 4 }, { placeholder: qsTr("Icon"), weight: 1.6 }]
      pathPart: 1
      addLabel: qsTr("Add place")
      onEdited: list => Config.extraPlacesSetting = list
      onReset: Config.resetToDefault("places/extra")
    }
  }
}
