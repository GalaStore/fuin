# Changelog

All notable changes to Fūin are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- The *CLI* button of the AI panel copies `fuin --context <paths>` without `| wl-copy`, so it no longer depends on a
  Wayland clipboard tool; pipe it to your own clipboard command if you want it on the clipboard.
- Sizes below 1 KiB are shown as `1023 B` (not `1,023 B`) and larger ones through Qt's locale formatting instead of KIO.
- The git branch and dirty count of a folder are read with plain `git` calls instead of through `sh`.
- Terminal launching moved to a per-OS launcher (`src/platform/Terminal.cpp`); Linux behaviour is unchanged.
- Part of the v1.0 portability work ([#15](https://github.com/GalaStore/fuin/issues/15)): MSVC gets `/W4` instead of the
  GCC/Clang warning flags, the desktop file, icon and metainfo are installed on Linux only, and the IPC endpoint is a
  named pipe (`fuin-<user>`) on Windows.

## [0.3.0] - 2026-11-21

### Added

- Unit tests for the preview parsers (`tst_inspector`: CSV/TSV, JSON, JSON Lines, safetensors, GGUF v2/v3, ONNX and,
  with Arrow, Parquet) and for the secret guard (`tst_secretguard`: every file name and token pattern, positive and
  negative). CI runs them with and without Parquet ([#7](https://github.com/GalaStore/fuin/issues/7)).
- *Ajustes* has *General*, *Places*, *Appearance* and *Agents (MCP)* sections for every documented `fuin.conf`
  key: terminal and editor (showing the auto-detected default), project folders and extra places, animations and
  speed, the colours and shell settings files, and the MCP roots and write permission. Changes apply without a
  restart ([#8](https://github.com/GalaStore/fuin/issues/8)).
- Spanish translation for the file chooser (portal) strings.

### Fixed

- `fuin --portal` no longer delays login by about 25 s. Qt's start-up called the portal (app ID registration and
  portal-aware platform themes such as qt6ct), which was itself waiting for Fūin, so both hung until D-Bus activation
  timed out. Portal mode now keeps Qt away from the portal, takes its bus name before loading the picker (built on
  the first request) and reads the qt6ct icons, style, font and palette itself
  ([#39](https://github.com/GalaStore/fuin/issues/39)).

## [0.2.0] - 2026-11-19

### Added

- `CHANGELOG.md` ([#5](https://github.com/GalaStore/fuin/issues/5)).
- `ROADMAP.md`, linked from the READMEs.
- GitHub issue forms, a pull request template and a security policy (`SECURITY.md`).
- `FUIN_BUILD_TESTS` CMake option: `tst_foldermodel` (QTest with a fatal `QAbstractItemModelTester`) and the
  `bench_foldermodel` listing benchmark. CI runs the tests on both Parquet configurations
  ([#38](https://github.com/GalaStore/fuin/pull/38)).
- CTest smoke tests for the command line: `fuin --version`, `--context` in Markdown and XML (a fixture `.env` stays
  out of the pack) and the MCP server (initialize, the 11 `fuin_*` tools, `fuin_read_file` refusing a secret). They
  run headless and replace the inline smoke step in CI ([#3](https://github.com/GalaStore/fuin/issues/3)).
- Translations: the UI uses English source text with `qsTr()` / `tr()` and ships a Spanish translation
  (`i18n/fuin_es.ts`) built with `qt_add_translations()`. Fūin follows the system locale and falls back to English;
  building now needs `qt6-tools` ([#2](https://github.com/GalaStore/fuin/issues/2)).
- System file chooser: `fuin --portal` implements the xdg-desktop-portal FileChooser backend in Fūin windows (open
  files or a folder, save with overwrite confirmation, save several files, filters). D-Bus starts it on demand and it
  exits when idle. The install adds `fuin.portal` and a D-Bus service file (CMake option `FUIN_WITH_PORTAL`, `ON` on
  Linux); select it with `org.freedesktop.impl.portal.FileChooser=fuin` in `portals.conf`
  ([#39](https://github.com/GalaStore/fuin/issues/39)).
- GitHub Releases: pushing a `v*` tag builds and tests on Arch Linux and publishes the release with the version's
  `CHANGELOG.md` section as notes (`.github/scripts/release-notes.sh`). The release checklist is in
  `CONTRIBUTING.md` ([#6](https://github.com/GalaStore/fuin/issues/6)).

### Changed

- `fuin --mcp` and `fuin --context` output is always English: MCP tool titles, approval titles, inspector fields,
  context pack headings and the `--context` summary line. Tool names and descriptions are unchanged
  ([#2](https://github.com/GalaStore/fuin/issues/2)).
- The PKGBUILD pins the sha256 of the v0.1.0 source tarball and comes with a generated `.SRCINFO`.

### Fixed

- Listing large folders is no longer quadratic: KIO batches are merged into the sorted view instead of resetting
  the model and re-sorting every item, so 100 000 files list in well under a second (about 0.6 s in the full GUI,
  down from about 52 s). Large refreshes and deletions are linear, and the selection and current item survive the
  listing ([#10](https://github.com/GalaStore/fuin/issues/10), [#38](https://github.com/GalaStore/fuin/pull/38)).

## [0.1.0] - 2026-10-20

First public release.

### Added

- Files: tabs, split view (F3) with copy/move to the other pane (F5/F6), and grid, list and column views.
- Editable path bar (Ctrl+L) with completion and any KIO URL (`sftp://`, `smb://`, `mtp:/`, `trash:/`…).
- Sidebar with places, removable drives (Solid, with usage bars and mount/eject), network and trash; thumbnails,
  drag and drop and *Open with…*.
- Copy, move and trash with progress and undo (Ctrl+Z).
- Type-to-filter, recursive search with `fd`, and git badges (branch and changes).
- Default applications settings page (Ctrl+,): shows and changes the system's default application per file type,
  written to `~/.config/mimeapps.list`, with a context menu entry that opens it for the selected file's type.
- Previews: code with syntax highlighting, Markdown, images, video and PDF.
- Table inspector for CSV/TSV/JSON/JSONL/Parquet (schema, rows, nulls, min/max, *Copy SQL*, *Open in DuckDB*);
  Parquet support is optional (`FUIN_WITH_PARQUET`, Apache Arrow).
- Model inspector for safetensors, GGUF and ONNX (parameters, dtypes, quantization) without loading the weights.
- AI context packs (Ctrl+Shift+I or `fuin --context`): the selection or folder as one Markdown/XML pack, respecting
  `.gitignore`, with schemas instead of data for large tables and models, and a token estimate.
- MCP server (`fuin --mcp`) for coding agents. Writing is off by default; when enabled, every change asks for
  confirmation in the GUI and can be undone.
- Secret guard: `.env` files, keys and tokens are marked and never sent to an AI.
- Single-instance GUI with `--reveal` and `--preview`.
- Material 3 theme with optional live colours from matugen; XDG configuration under `$XDG_CONFIG_HOME/fuin`.
- Packaging: desktop entry, AppStream metadata, icons and a PKGBUILD in `packaging/`, with optional
  `plasma-activities-stats` for the Recent place.
- GitHub Actions CI on Arch Linux, with and without Parquet, including headless smoke tests.
- README (English and Spanish), contributing guide and design notes.

### Fixed

- Opening applications that run in a terminal (such as `vim` for text files) outside Plasma: when KIO has no usable
  terminal configured, Fūin provides the terminal it resolves (preferring `xdg-terminal-exec`) through its own
  `~/.config/fuinrc`, never `kdeglobals`. The error toast names the file and the application.

[Unreleased]: https://github.com/GalaStore/fuin/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/GalaStore/fuin/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/GalaStore/fuin/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/GalaStore/fuin/releases/tag/v0.1.0
