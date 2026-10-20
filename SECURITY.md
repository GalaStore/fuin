# Security policy

## Supported versions

Only the latest release of Fūin (currently 0.1.x) and the `main` branch receive security fixes.

## Reporting a vulnerability

Please **do not open a public issue**. Report it privately through GitHub's
[private vulnerability reporting](https://github.com/GalaStore/fuin/security/advisories/new)
(*Security* tab → *Report a vulnerability*).

Include the Fūin version (`fuin --version`), how to reproduce it, and the impact you expect. We aim to
acknowledge reports within a week and will coordinate a fix and disclosure with you.

Areas that are especially relevant:

- the **MCP server** (`fuin --mcp`): escaping the `mcp/roots` sandbox (for example through symlinks), writes
  without the GUI confirmation, or writes while `mcp/allowWrite=false`;
- the **secret guard** (`src/ai/SecretGuard.cpp`): secrets that end up in context packs or in MCP results;
- the single-instance IPC socket (`$XDG_RUNTIME_DIR/fuin.sock`).

A secret-looking file that the guard does not detect is a bug worth reporting privately too.
