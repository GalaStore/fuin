# Changelog

All notable changes to Fūin are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `CHANGELOG.md` ([#5](https://github.com/GalaStore/fuin/issues/5)).
- `ROADMAP.md`, linked from the READMEs.
- GitHub issue forms, a pull request template and a security policy (`SECURITY.md`).
- `FUIN_BUILD_TESTS` CMake option: `tst_foldermodel` (QTest with a fatal `QAbstractItemModelTester`) and the
  `bench_foldermodel` listing benchmark. CI runs the tests on both Parquet configurations
  ([#38](https://github.com/GalaStore/fuin/pull/38)).

### Changed

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
- Packaging: CMake project, desktop entry, AppStream metadata, icons and a PKGBUILD in `packaging/`.
- GitHub Actions CI on Arch Linux, with and without Parquet, including headless smoke tests.
- README (English and Spanish), contributing guide and design notes.

### Fixed

- Opening applications that run in a terminal (such as `vim` for text files) outside Plasma: when KIO has no usable
  terminal configured, Fūin provides the terminal it resolves (preferring `xdg-terminal-exec`) through its own
  `~/.config/fuinrc`, never `kdeglobals`. The error toast names the file and the application.

[Unreleased]: https://github.com/GalaStore/fuin/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/GalaStore/fuin/releases/tag/v0.1.0
