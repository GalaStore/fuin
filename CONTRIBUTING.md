# Contributing to Fūin

Thanks for helping! A few guidelines keep the project easy to work on.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build build && ./build/fuin
```

The build must stay warning-free (`-Wall -Wextra -Wpedantic`); CI builds with warnings as errors on Arch Linux,
with and without Arrow/Parquet.

### Arrow soname bumps (Arch Linux)

Arch ships Arrow/Parquet as shared libraries whose soname changes with each major version (`libarrow.so.2500` →
`libarrow.so.2600`). A `pacman -Syu` that updates `arrow` breaks an existing `fuin` binary; rebuild it
(`cmake --build build && cmake --install build`) and bump `pkgrel` in `packaging/PKGBUILD` for AUR users.

## Tests

`-DFUIN_BUILD_TESTS=ON` builds `tests/`: `tst_foldermodel` (QTest, with a fatal `QAbstractItemModelTester`) covers
listing order, hidden/filter, refresh moves, renames, mass deletes and selection. `tst_inspector` checks the preview
parsers on small fixtures generated in the test (CSV delimiter sniffing and column statistics, JSON, JSON Lines,
safetensors, GGUF v2/v3 and ONNX headers, and Parquet footer statistics when built with Arrow, or the "built without
Parquet" error otherwise). `tst_secretguard` has a positive and a negative case for every secret file name and token
pattern; token-shaped strings are built at run time so no literal looks like a real key. `tst_config` covers the
`fuin.conf` accessors behind *Ajustes* (automatic values, empty lists, restoring defaults, change signals) against a
throwaway config under `~/.qttest`. `bench_foldermodel` times listing
folders in creation and shuffled order. The `smoke` tests (`tests/smoke.cmake`, a plain CMake script) run the built
`fuin` headless: `--version`, `--context` in Markdown and XML on a generated fixture folder whose `.env` must stay out
of the pack, and `--mcp` (initialize, the 11 `fuin_*` tools, `fuin_read_file` refusing the `.env`).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_COMPILE_WARNING_AS_ERROR=ON -DFUIN_BUILD_TESTS=ON
cmake --build build
dbus-run-session -- ctest --test-dir build --output-on-failure -LE bench   # what CI runs
ctest --test-dir build -L smoke --output-on-failure                        # smoke tests only
ctest --test-dir build -L bench --verbose                                  # benchmark, 1k and 10k
./build/tests/bench_foldermodel 100000                                     # any sizes as arguments
```

The rename test needs a D-Bus session bus (KIO's rename notifications) and is skipped without one. The smoke tests set
`QT_QPA_PLATFORM=offscreen` themselves and run with a throwaway `HOME`, so they need no display and never read your
Fūin config. Each test creates its own temporary directory. Changes to `FolderModel` should keep the tests green and
come with the benchmark numbers before and after.

## Code style

- C++20, Qt 6 idioms. `QT_NO_KEYWORDS` is on: use `Q_SIGNALS`, `Q_SLOTS`, `Q_EMIT`.
  `QT_NO_CAST_FROM_ASCII` is on: use `u"…"_s` / `QStringLiteral`.
- Anything that touches the file system goes through KIO (`KIO::copy`, `KCoreDirLister`, `KIO::PreviewJob`…) so
  remote URLs, the trash and undo keep working. Never block the GUI thread: use KIO jobs or `QtConcurrent`.
- QML: small components, colours only from `Theme` roles, durations through `Theme.dur(ms)` so the
  "no animations" setting is honoured. Avoid naming QML files like Qt Quick Controls types (`Pane`, `Page`…).
- Code and comments in English. UI strings are English source text wrapped in `qsTr()` (QML) or `tr()` /
  `QCoreApplication::translate()` (C++); see [Translations](#translations).
- Format C++ with `clang-format` (WebKit-ish, 4 spaces, 160 columns) if you have it.

## Translations

English is the source language. Translations live in `i18n/fuin_<lang>.ts`; the build compiles them with `lrelease`
(package `qt6-tools`) and Fūin loads the one that matches the system locale, falling back to English. Only the GUI is
translated: `fuin --mcp` and `fuin --context` always print English.

- Mark new strings with `qsTr("…")` in QML/JS and `tr("…")` in C++ (`QCoreApplication::translate("Context", "…")`
  outside a `QObject`). Use `%1` placeholders instead of concatenating pieces.
- After changing strings, refresh the `.ts` files with `cmake --build build --target update_translations`.
- Translate in Qt Linguist (`/usr/lib/qt6/bin/linguist i18n/fuin_es.ts`) or any `.ts` editor and rebuild.
- To add a language, add `i18n/fuin_<lang>.ts` to `TS_FILES` in `CMakeLists.txt`, run `update_translations` and
  translate the new file.
- Check the result with `LANG=es_ES.UTF-8 fuin` (or your locale).

## Developer automation

Set `FUIN_DEV=1` to enable the `dev` IPC command, used to script the UI and take screenshots:

```sh
FUIN_DEV=1 QT_QPA_PLATFORM=offscreen fuin ~/some/folder &
echo '{"cmd":"dev","view":"list","panel":"ai","grab":"/tmp/shot.png"}' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/fuin.sock
```

Screenshots in `docs/screenshots/` must not contain personal data — use a throwaway folder such as the
sample generated by the commands in `docs/DESIGN.md`.

## Commits & pull requests

- One topic per pull request; describe what changed and how you tested it.
- Commit messages: imperative mood, short subject (`preview: show Parquet row groups`).
- Add user-visible changes to the `Unreleased` section of [CHANGELOG.md](CHANGELOG.md)
  ([Keep a Changelog](https://keepachangelog.com/en/1.1.0/) format); releases move it under the new version.
- New MCP tools need a JSON schema, annotations (`readOnlyHint` / `destructiveHint`) and must respect the
  roots sandbox, the secret guard and the GUI confirmation for writes.

## Releases

Pushing a `v*` tag runs the [Release workflow](.github/workflows/release.yml): it checks that the tag matches the
version in `CMakeLists.txt` and the metainfo, builds and tests on Arch Linux, and publishes the GitHub Release with
the version's `CHANGELOG.md` section as notes. Checklist for `x.y.z`:

1. In a pull request:
   - `CHANGELOG.md`: rename `[Unreleased]` to `[x.y.z] - YYYY-MM-DD`, add an empty `[Unreleased]` above it and
     update the compare links at the bottom.
   - `CMakeLists.txt`: `project(fuin VERSION x.y.z …)`.
   - `packaging/io.github.galactuuus.fuin.metainfo.xml`: add `<release version="x.y.z" date="YYYY-MM-DD"/>` first.
   - Preview the notes with `.github/scripts/release-notes.sh x.y.z`.
2. Once it is merged and CI is green, tag `main` and push the tag:
   `git tag -a vx.y.z -m "Fūin x.y.z" && git push origin vx.y.z`.
3. Check the release on GitHub; edit its text there if you want a short introduction.
4. Packaging, in a follow-up pull request: set `pkgver=x.y.z` and `pkgrel=1` in `packaging/PKGBUILD`, run
   `updpkgsums` (from `pacman-contrib`) and `makepkg --printsrcinfo > .SRCINFO` in `packaging/`, then update the AUR
   package.
