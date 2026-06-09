# guandao

Minimal book and manga reader in pure C11. Raylib for rendering, SQLite for
the library, miniz for archives — all dependencies vendored in `lib/` and
built from source, no system packages needed beyond a C compiler, cmake and
OpenGL.

## Formats

- `.txt` books — paginated to the window, optional explicit `<PAGE_BREAK>` markers
- image folders (manga) — png/jpg/jpeg, sorted by name
- `.cbz` / `.cbt` comic archives

Reading progress, the library and a translation cache live in a single SQLite
database next to the binary. The most recently opened entry reopens on launch.

## Build

```sh
make          # builds vendored deps, downloads the UI font, builds guandao
./build/guandao
```

```sh
make test     # db + cbz + cbt tests
make samples  # download public-domain sample comics into build/samples
```

## Controls

- `>` toggle — side panel: open book / manga folder, library list
- drag-and-drop a file or folder onto the window to open it
- left/right arrows or on-screen buttons — page turn
- library: left click opens, right click removes the entry
- mouse wheel scrolls the library list

## License

MIT, see [LICENSE](LICENSE). Vendored libraries keep their own licenses.
