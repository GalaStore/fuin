# Archivos — design notes

## Goals

1. A file manager that is *faster to drive than Dolphin* for people who live in a terminal and an editor:
   keyboard first, instant filter, split panes, columns, good previews.
2. Never reinvent file system plumbing: **KIO** for listing, copying, trash, thumbnails, remote protocols and undo;
   **Solid** for drives. The app is "a new face on KIO".
3. Make the file manager a useful **bridge between the user and AI tools**: the place where you pick context for a
   chat, and the thing agents can ask "what is the user looking at?".
4. Calm visuals: rounded cards, Material 3 colour roles, no dashed or stitched lines, motion that can be turned off.

## Architecture

```
            ┌───────────────── QML (qml/) ─────────────────┐
            │ AppWindow · TabPage · FolderPane · Sidebar   │
            │ PathBar · Preview · DataTable · AiPanel      │
            └──────┬──────────────┬───────────────┬────────┘
   FolderModel ────┘   FileOps ───┘   Previewer ──┘  ContextPack, Ipc, Theme, Config, Places
 (KCoreDirLister)    (KIO jobs,       (QtConcurrent →
   git badges,        undo manager,    Inspector)
   fd search)         clipboard)
                                        Inspector ── Arrow/Parquet, safetensors, GGUF, ONNX, CSV/JSON
 McpServer (stdio, headless) ──IPC socket──► IpcServer (GUI): state, reveal, confirmations
```

- **One process per user session.** The GUI listens on `$XDG_RUNTIME_DIR/archivos.sock` (user-only). A second
  `archivos …` forwards its request (open / reveal / preview) and exits.
- **`archivos --mcp`** is a separate headless process started by the agent. Read-only tools work even when the GUI
  is closed (they use the same `Inspector` and `ContextPacker` code). `archivos_get_current_view` and all write
  tools talk to the GUI over the socket; writes are executed *by the GUI* through `FileOps`, so they show progress
  and land in the same undo history as the user's own actions.
- **Theme** exposes 30 Material 3 roles as properties. It reads an optional matugen-style JSON (path configurable),
  watches the file and its directory (atomic renames), and cross-fades colours over 600 ms.

## AI features — evaluated

| Idea | Value | Cost / risk | Decision |
|---|---|---|---|
| **Context pack** (selection → Markdown/XML, tree + files, token estimate) | Very high: the most common "AI + files" chore | Low | ✅ shipped (GUI, CLI, MCP) |
| **MCP server** (current view, list/read/search/inspect, reveal; confirmed writes) | High: agents stop guessing paths, user sees what agents touch | Medium: security | ✅ shipped, read-only by default, roots sandbox, GUI confirmation + undo |
| **Data-engineer previews** (Parquet/CSV/JSONL schema + stats, DuckDB SQL/open) | High for data work; also feeds context packs with schemas instead of raw data | Low–medium (Arrow optional) | ✅ shipped |
| **ML model inspector** (safetensors/GGUF/ONNX headers; HF cache & Ollama places) | Medium–high: "what is this 8 GB file?" | Low (header parsing only) | ✅ shipped |
| **Secret guard** (.env/keys/tokens badges; never sent to AI) | High as a safety net for the two features above | Low | ✅ shipped |
| **Ask your files** (semantic search, local embeddings) | High, but only once indexed | High: indexing, storage, battery | 🧪 designed below, flag off |
| Smart rename / organise with an LLM | Medium | Needs a model + careful UX | ❌ later, via MCP agents instead |
| Auto-tagging images with a VLM | Low–medium | Heavy | ❌ |
| Chat panel inside the file manager | Low: duplicates Claude/Cursor | Medium | ❌ — integrate with agents via MCP instead |

### Context pack rules

- Inputs: files and folders. Folders inside a git work tree use `git ls-files -co --exclude-standard`;
  otherwise a walker skips `node_modules`, `.venv`, `target`, `build`, `dist`, `__pycache__`, `.git`…
- Skipped: secret-looking names and contents, lockfiles, binaries, files > 256 KB, beyond 400 files / 2 MB.
- Parquet, safetensors, GGUF, ONNX and tables larger than 32 KB are included as a **schema summary**
  (columns, types, nulls, min/max, first rows / model header and tensors).
- Token estimate: characters / 3.6 (good enough for budgeting; shown before you paste).

### MCP security model

- Paths are canonicalised and must be inside `mcp/roots` (default `~`). Symlinks out of the roots are refused.
- `archivos_read_file` refuses binaries and anything the secret guard flags.
- Write tools are rejected unless `mcp/allowWrite=true` (toggle in the AI panel). Then each request opens a dialog
  in the GUI showing the client name and the exact operation; no answer in 120 s = rejected.
- Supports MCP protocol versions 2024-11-05 → 2025-11-25 (`initialize`) and the stateless 2026 draft
  (`server/discover`, per-result `ttlMs`/`cacheScope`).

## Semantic search ("Pregunta a tus archivos") — design, disabled

- **Embeddings**: local Ollama (`ai/ollamaUrl`, default `http://127.0.0.1:11434`) with `nomic-embed-text`
  (768-d) — nothing leaves the machine. Detected at start-up; the card in the AI panel explains what is missing.
- **What gets indexed**: only folders the user opts in (per-folder toggle), text-like files ≤ 256 KB, respecting
  `.gitignore` and the secret guard. Data/model files are indexed through their schema summary.
- **Chunking**: ~1 200 characters with 200 overlap, split on headings / blank lines / function boundaries.
- **Storage**: `$XDG_CACHE_HOME/archivos/index.sqlite` — tables `files(path, mtime, size, hash)` and
  `chunks(file_id, ord, text, vec BLOB)`; vectors stored as float16; brute-force cosine search in a worker thread
  (fine up to ~200 k chunks), sqlite-vec later if needed.
- **Freshness**: `KDirWatch` on opted-in folders + mtime/hash check on start; indexing at idle priority, paused on
  battery.
- **UI**: query box → ranked files with the best-matching snippet; Enter reveals the file; "Add to context" button.
- **MCP**: `archivos_semantic_search(query, k)` with the same results.
- Why it is off: needs Ollama and an index, and the indexing UX (opt-in, progress, disk use) deserves its own
  release.

## Sample data for screenshots/tests

```sh
mkdir -p /tmp/archivos-demo && cd /tmp/archivos-demo
python3 - <<'PY'
import pyarrow as pa, pyarrow.parquet as pq, random, datetime
n = 5000
pq.write_table(pa.table({
    "order_id": range(100000, 100000 + n),
    "order_date": [datetime.date(2026, 1, 1) + datetime.timedelta(days=random.randint(0, 280)) for _ in range(n)],
    "city": [random.choice(["Madrid", "Barcelona", "Valencia"]) for _ in range(n)],
    "amount_eur": [round(random.uniform(5, 450), 2) for _ in range(n)],
}), "ventas_2026.parquet", compression="zstd")
PY
```
