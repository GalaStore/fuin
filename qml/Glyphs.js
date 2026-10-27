.pragma library
// Material Symbols glyph + accent role for a file, and localized date/size helpers.

function icon(kind, ext, name, isDir) {
  if (isDir) {
    const n = (name || "").toLowerCase()
    if (n === ".git") return "commit"
    if (n === "node_modules" || n === ".venv" || n === "venv") return "inventory_2"
    if (n === "descargas" || n === "downloads") return "download"
    if (n === "imágenes" || n === "pictures" || n === "wallpapers") return "image"
    if (n === "música" || n === "music") return "music_note"
    if (n === "vídeos" || n === "videos") return "movie"
    if (n === "documentos" || n === "documents") return "description"
    if (n === "escritorio" || n === "desktop") return "desktop_windows"
    if (n === "dev" || n === "projects" || n === "src" || n === "code") return "terminal"
    return "folder"
  }
  switch (kind) {
  case "image": return "image"
  case "video": return "movie"
  case "audio": return "music_note"
  case "pdf": return "picture_as_pdf"
  case "data": return ext === "json" || ext === "jsonl" || ext === "ndjson" ? "data_object" : ext === "sqlite" || ext === "db" || ext === "duckdb" ? "database" : "table_chart"
  case "model": return "neurology"
  case "code": return ext === "sh" || ext === "bash" || ext === "zsh" || ext === "fish" ? "terminal" : "code"
  case "markdown": return "article"
  case "archive": return "folder_zip"
  case "text": return "description"
  }
  return "draft"
}

// role name on Theme used for the glyph colour
function tone(kind, isDir, isRepo) {
  if (isDir) return isRepo ? "tertiary" : "primary"
  if (kind === "image" || kind === "video" || kind === "audio") return "secondary"
  if (kind === "code" || kind === "data" || kind === "model" || kind === "markdown") return "tertiary"
  return "textDim"
}

function month(i) {
  return [qsTr("Jan"), qsTr("Feb"), qsTr("Mar"), qsTr("Apr"), qsTr("May"), qsTr("Jun"),
          qsTr("Jul"), qsTr("Aug"), qsTr("Sep"), qsTr("Oct"), qsTr("Nov"), qsTr("Dec")][i]
}
function pad(n) { return n < 10 ? "0" + n : "" + n }
function date(ms) {
  if (!ms || ms <= 0) return ""
  const d = new Date(ms), now = new Date()
  const hm = pad(d.getHours()) + ":" + pad(d.getMinutes())
  const day = 86400000
  const startToday = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
  if (ms >= startToday) return qsTr("today %1").arg(hm)
  if (ms >= startToday - day) return qsTr("yesterday %1").arg(hm)
  if (d.getFullYear() === now.getFullYear()) return d.getDate() + " " + month(d.getMonth()) + " " + hm
  return d.getDate() + " " + month(d.getMonth()) + " " + d.getFullYear()
}

function size(bytes) {
  const b = Number(bytes || 0)
  if (b < 1024) return b + " B"
  const u = ["KB", "MB", "GB", "TB"]
  let v = b / 1024, i = 0
  while (v >= 1024 && i < u.length - 1) { v /= 1024; i++ }
  return (v >= 100 ? v.toFixed(0) : v.toFixed(1)).replace(".", Qt.locale().decimalPoint) + " " + u[i]
}

function count(n) {
  return Number(n || 0).toLocaleString(Qt.locale(), "f", 0)
}

// Display name for an editor/terminal command ("cursor" -> "Cursor").
function appName(cmd) {
  const known = { cursor: "Cursor", code: "VS Code", codium: "VSCodium", zed: "Zed", kate: "Kate", nvim: "Neovim", kitty: "kitty", foot: "foot" }
  if (!cmd) return qsTr("editor")
  const base = String(cmd).split("/").pop().split(" ")[0]
  return known[base] || (base.charAt(0).toUpperCase() + base.slice(1))
}
