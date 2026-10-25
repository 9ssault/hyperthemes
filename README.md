# HyperThemes

HyperThemes is a native Qt 6 / QML theme browser and applier for Linux desktops. It keeps one catalog for KDE Plasma, Caelestia / Quickshell, Waybar, GTK, Qt, terminals, Spotify and Discord, and applies a chosen palette through small, inspectable templates.

It is local-first: every file it changes is copied to a timestamped backup before the write, and every apply can be rolled back.

## What works

- **Catalog** — bundled palettes, local imports, KDE Store browsing, and GitHub browsing in one searchable grid.
- **Palette previews** — every entry exposes a full Material-style palette, not just one accent color.
- **Dynamic wallpapers** — `matugen image` reads a wallpaper and returns a complete palette; HyperThemes applies that palette and can set the wallpaper too.
- **Targets** — KDE color scheme, Caelestia / Quickshell scheme, Waybar, GTK 3/4, Qt 5/6, Konsole, kitty, foot, Spicetify and Vesktop.
- **User or system-wide** — KDE color schemes can be written to `~/.local/share/color-schemes` or `/usr/share/color-schemes` through `pkexec`.
- **Backups** — `~/.local/state/hyperthemes/backups/<timestamp>-<theme>/` contains the previous files and a manifest. Restore from the Settings page.

Community sources are deliberately **browse-only** in this release. KDE Store and GitHub entries open their source page; install those themes with their own tooling, then import the files locally.

## Build

Requirements:

- Qt 6.5 or newer with `Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`
- CMake 3.21+, Ninja
- A C++20 compiler

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/hyperthemes
```

Install to your user prefix:

```bash
cmake --install build --prefix ~/.local
```

The app reads its catalog and templates from embedded resources, so the binary is self-contained apart from the optional tools below.

## Optional tools

| Tool | Used for |
| --- | --- |
| `matugen` | Wallpaper palette generation |
| `plasma-apply-colorscheme` | Switching the KDE scheme after writing it |
| `plasma-apply-wallpaperimage` | Setting a generated wallpaper on Plasma |
| `spicetify` | Spotify palette |
| `pkexec` | System-wide KDE writes |
| `plasma-apply-colorscheme`, `kwriteconfig` | KDE integration |

HyperThemes skips targets whose tools or config directories are missing and reports why in the inspector.

## Local imports

Use **Import folder** or the shortcuts on the Settings page. Folders are scanned recursively for:

- Caelestia scheme files: lines of `role 6-digit-hex`
- KDE `.colors` files
- HyperThemes JSON manifests with a `palette` or `colours` object

Two built-in shortcuts point at:

- `~/.local/lib/caelestia/schemes`
- `~/.local/share/color-schemes`

Imported themes live for the current session; the app does not modify the source folder.

## Catalog data

`catalog/themes.json` is generated from a Caelestia scheme tree:

```bash
python3 scripts/generate_catalog.py
# or
python3 scripts/generate_catalog.py --source /path/to/caelestia/schemes --out catalog/themes.json
```

Each entry has this shape:

```json
{
  "id": "bundled-dracula",
  "name": "Dracula",
  "description": "Purple, pink and cyan on a deep charcoal base.",
  "author": "Caelestia contributors",
  "license": "GPL-3.0-or-later",
  "source": "bundled",
  "sourceUrl": "https://github.com/caelestia-dots/cli",
  "kind": "static",
  "mode": "dark",
  "flavour": "default",
  "targets": ["kde", "caelestia", "waybar", "gtk", "qt", "konsole", "kitty", "spicetify", "vesktop"],
  "palette": {
    "background": "1e0d0f",
    "onBackground": "f9d5d7",
    "surface": "1e0d0f",
    "primary": "e62833"
  }
}
```

Missing palette roles are filled with sensible Material fallbacks by the renderer. The full list is in `src/palette.cpp`.

## Templates

The generated files are small and readable:

- `templates/kde.colors`
- `templates/caelestia.json`
- `templates/waybar.css`
- `templates/gtk.css`
- `templates/qt.conf`
- `templates/konsole.colorscheme`
- `templates/kitty.conf`
- `templates/foot.ini`
- `templates/spicetify.ini`
- `templates/spicetify-user.css`
- `templates/vesktop.css`

Placeholders use `{{role}}`, `{{role.rgb}}` and `{{role.stripped}}`. Add a template and a corresponding target in `src/applier.cpp` to extend the app.

## Current limits

- Remote themes are not downloaded or unpacked; they are browsable cards with a source link.
- There is no layout preset apply yet. This release covers colors and app themes.
- Spicetify patches Spotify's client files; use `spicetify restore` if you want stock Spotify back.
- Konsole uses the scheme after you select **HyperThemes** in its profile settings.
- Vesktop needs a restart (or a theme toggle) after a color change.
- A Flatpak update can reset the Spotify patch; re-apply the Spicetify target after updating.
- System-wide writes only cover the KDE color scheme. Everything else stays in XDG user directories.

## Development

```bash
cmake --build build
QT_QPA_PLATFORM=offscreen ./build/hyperthemes
```

Warnings and QML errors go to stderr. The backup manifest is plain JSON, so an apply can be audited after the fact.
