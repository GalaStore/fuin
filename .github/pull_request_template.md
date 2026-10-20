## Summary

<!-- What does this change and why? Link the issue: "Closes #123". -->

## How was it tested?

<!-- Commands you ran, desktop/compositor, screenshots for UI changes (no personal data). -->

## Checklist

- [ ] One topic per pull request; commit messages in English, imperative mood (`preview: show Parquet row groups`).
- [ ] Builds without warnings: `cmake -S . -B build -G Ninja -DCMAKE_COMPILE_WARNING_AS_ERROR=ON && cmake --build build`.
- [ ] File system access goes through KIO and does not block the GUI thread.
- [ ] New or changed MCP tools have a JSON schema and annotations, and respect the roots sandbox, the secret guard and the GUI confirmation for writes.
- [ ] Docs updated if behaviour changed (README.md, README.es.md, wiki).
