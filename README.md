# Fūin

**A fast, keyboard-first file manager for Linux, built on Qt 6 Quick and KDE's KIO — with
first-class previews for data and ML files, one-key context packs for LLM chats, and an MCP
server so coding agents can see (and, with your permission, act on) the folder you are looking at.**

> 🇪🇸 [Leer en español](README.es.md)

*Fūin* (封印, "seal") is the sealing jutsu from Naruto used to store things inside scrolls — here, your files.

![Main view](docs/screenshots/main.png)

## Features

**File management (KIO everywhere)**
- Tabs, split view (F3) with copy/move to the other pane (F5/F6), grid · list · Miller columns.
- Breadcrumb path bar that turns into an editable field (Ctrl+L) with Tab completion; any KIO URL works:
  `sftp://`, `smb://`, `mtp:/`, `trash:/`, `recentlyused:/`, `remote:/`…
- Sidebar with places, project folders, removable drives (Solid, with usage bars and mount/eject), network and trash.
- Thumbnails through KIO's thumbnailers (images, video, PDF, … — whatever plugins are installed).
- Copy / move / trash / delete / rename / new folder with KIO progress, cancel, and **undo** (Ctrl+Z, KIO's undo manager).
- Drag & drop (same device → move, else copy), clipboard interoperable with Dolphin & co., *Open with…*.
- Type-to-filter, recursive search with `fd` (falls back to a built-in walker), hidden files toggle (Ctrl+H).
- Git awareness: branch + dirty count badges on repositories, repo chip in the path bar.
- Context-menu actions: terminal here (kitty, foot, alacritty, wezterm, konsole, gnome-terminal), open in your editor
  (Cursor, VS Code, Zed, Kate…), copy path.

**Previews**
- Code with syntax highlighting (KSyntaxHighlighting), Markdown rendered, images, video/PDF posters.
- **Tables**: CSV/TSV (delimiter sniffing), JSON, JSONL and **Parquet** — schema, row count, nulls, min/max
  (exact from the Parquet footer, no data scan), and the first rows. *Copy SQL*, *Open in DuckDB*, *Copy schema*.
- **ML models**: safetensors, GGUF (v2/v3: architecture, context length, quantization, tensors) and ONNX headers —
  parameter counts and dtypes without loading weights. Hugging Face cache and Ollama models appear in the sidebar.
- Folders: item counts, project type detection (Python, Node, Rust, dbt, Airflow, CMake…), README rendered.

**AI features**
- **Context for AI** (Ctrl+Shift+I): bundles the selection or current folder into one Markdown or XML document
  (tree + files) and copies it. Respects `.gitignore`, skips lockfiles/binaries, includes **schemas instead of raw
  data** for Parquet/large CSV/model files, and shows the token estimate. Also available as `fuin --context`.
- **MCP server** (`fuin --mcp`): agents can get your current view and selection, list, read, search (names or
  contents), inspect data/model files, build context packs and reveal files in the window. Writes (create folder,
  copy, move, trash) are **off by default**; when enabled, every change asks you in the GUI and is undoable.
- **Secret guard**: `.env`, keys, tokens, credentials, and files whose content looks like a secret get a 🔑 badge
  and are never put into context packs or returned to agents.
- *Ask your files* (semantic search with local Ollama embeddings) is designed but disabled in this release — see
  [docs/DESIGN.md](docs/DESIGN.md).

**Looks**
- Rounded, clean cards; Material 3 colour roles. Optional live theming from a [matugen](https://github.com/InioX/matugen)
  JSON file (cross-fades when your wallpaper changes); otherwise a built-in dark theme.
- Respects an optional shell settings file for animations on/off and speed.

## Install

### Arch Linux

```sh
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules qt6-base qt6-declarative qt6-svg \
  kio kio-extras solid kcoreaddons kservice syntax-highlighting ttf-material-symbols-variable
# optional but recommended
sudo pacman -S --needed arrow fd ripgrep git duckdb kdegraphics-thumbnailers ffmpegthumbs kimageformats plasma-activities-stats
```

Then either build the package (`cd packaging && makepkg -si`, once a release tag exists) or build from source:

### From source (any distro)

Requirements: CMake ≥ 3.22, a C++20 compiler, Qt ≥ 6.6 (Core, Gui, Widgets, Qml, Quick, QuickControls2,
Concurrent, Network, Svg), KDE Frameworks 6 (KIO, Solid, CoreAddons, Service; SyntaxHighlighting optional),
the *Material Symbols Rounded* font. Optional: Apache Arrow/Parquet C++ for Parquet previews.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$HOME/.local
cmake --build build
cmake --install build
```

| CMake option | Default | Effect |
|---|---|---|
| `FUIN_WITH_PARQUET` | `ON` | Parquet previews if Arrow/Parquet C++ is found |
| `FUIN_WITH_SYNTAX` | `ON` | Syntax highlighting if KSyntaxHighlighting is found |

> **Arch Linux and Arrow updates:** Fūin links against Apache Arrow/Parquet, whose soname changes with every major
> release (e.g. `libarrow.so.2500` → `libarrow.so.2600`). After such a `pacman -Syu` the binary fails with
> *"cannot open shared object file"* — rebuild and reinstall (the three commands above, or `makepkg -si` for the
> package). Build with `-DFUIN_WITH_PARQUET=OFF` if you prefer not to depend on Arrow.

## Usage

```sh
fuin [folder|file|url]        # open (reuses the running instance, new window)
fuin --reveal ~/file.csv      # open its folder and select it
fuin --preview ~/data.parquet # …and show the preview
fuin --new-window
fuin --context [--format markdown|xml] paths… | wl-copy   # context pack to stdout
fuin --mcp                    # MCP server on stdio
```

### Connect an agent (MCP)

Claude Code:

```sh
claude mcp add --scope user fuin -- fuin --mcp
```

Cursor (`~/.cursor/mcp.json`) and other clients:

```json
{ "mcpServers": { "fuin": { "command": "fuin", "args": ["--mcp"] } } }
```

Tools: `fuin_get_current_view`, `fuin_list_directory`, `fuin_read_file`, `fuin_search`,
`fuin_inspect`, `fuin_context_pack`, `fuin_reveal`, and (write, confirmation required)
`fuin_create_folder`, `fuin_copy`, `fuin_move`, `fuin_move_to_trash`.
Agents only see paths under `mcp/roots` (default: your home folder).

### Keyboard

| Keys | Action | Keys | Action |
|---|---|---|---|
| Ctrl+L | edit path | Ctrl+F | search (recursive) |
| *type* | filter as you type | Ctrl+H | hidden files |
| Ctrl+T / Ctrl+W | new / close tab | Ctrl+Tab | next tab |
| F3 | split view | F5 / F6 | copy / move to other pane |
| Space · F9 | preview pane | Ctrl+I | AI panel |
| Ctrl+Shift+I | copy context for AI | F2 | rename |
| Delete | move to trash | Shift+Delete | delete permanently (asks) |
| Ctrl+Z | undo | Ctrl+Shift+N | new folder |
| Ctrl+1/2/3 | grid / list / columns | Ctrl+ +/- | icon size |
| Alt+←/→/↑ | back / forward / up | Ctrl+Alt+T | terminal here |
| Alt+Enter | properties | F1 | all shortcuts |

## Configuration

`$XDG_CONFIG_HOME/fuin/fuin.conf` (INI, created on first change; settings saved under the
pre-release name are migrated automatically on first start):

```ini
[apps]
terminal=kitty          ; default: first found of kitty, foot, alacritty, wezterm, konsole, gnome-terminal
editor=cursor           ; default: first found of cursor, code, codium, zed, kate
[theme]
colorsFile=~/.cache/matugen/colors.json   ; optional Material 3 colours (matugen JSON); watched live
shellSettingsFile=~/.config/myshell/settings.json ; optional {"appearance":{"animations":true,"animationSpeed":1}}
[appearance]
animations=true
animationSpeed=1.0
[places]
projects=~/Dev, ~/Projects
extra="Wallpapers|~/Pictures/Wallpapers|wallpaper"
[mcp]
roots=~
allowWrite=false
```

The colours file may also be given with `FUIN_COLORS=/path/colors.json`. Both flat role maps
(`{"primary": "#…", "surface": "#…"}`, snake_case or camelCase) and matugen's `{"colors": {"dark": {…}}}` work.

## Architecture

C++ for everything that touches the file system (KIO jobs, `KCoreDirLister`, `PreviewJob`, Solid, KFileItem,
Arrow), QML for the UI. See [docs/DESIGN.md](docs/DESIGN.md).

```
src/app      Config, Theme (M3 roles + live reload), AppInfo
src/fs       FolderModel (KCoreDirLister), FileOps (KIO jobs + undo), PlacesModel (Solid), thumbnails
src/preview  Inspector (CSV/JSON/Parquet/safetensors/GGUF/ONNX/…), Previewer, CodeHighlighter
src/ai       ContextPacker, SecretGuard, McpServer
src/ipc      single-instance socket (CLI --reveal, MCP ↔ GUI confirmations)
qml/         the interface
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Bug reports and pull requests are welcome.

## License

[MIT](LICENSE). Material Symbols (used as an icon font, not bundled) are © Google, Apache-2.0.
