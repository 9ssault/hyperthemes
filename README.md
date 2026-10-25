# HyperThemes

[![CI](https://github.com/9ssault/hyperthemes/actions/workflows/ci.yml/badge.svg)](https://github.com/9ssault/hyperthemes/actions/workflows/ci.yml)

HyperThemes is a native Qt 6 / QML theme browser and applier for Linux desktops. It keeps one catalog for KDE Plasma, Caelestia / Quickshell, Waybar, GTK, Qt, terminals, Spotify and Discord, and applies a chosen palette through small, inspectable templates.

It is local-first: every file it changes is copied to a timestamped backup before the write, and every apply can be rolled back.

## What works

- **Catalog** — 1,655 bundled themes in 61 categories: Caelestia flavours, the tinted-theming Base16 and Base24 collections, Gogh and iTerm2 terminal schemes. Local imports, KDE Store, GitHub and GitLab sit in the same searchable gallery.
- **Categories** — a dedicated shelf that groups the catalog into collections (Caelestia, Base16, Base24, Gogh, iTerm2) and theme families (Catppuccin, Gruvbox, Tokyo Night, Nord, Dracula and 47 more), with palette strips and counts.
- **Picker** — live palette previews, readability badges, target filters (KDE, Quickshell, Waybar, GTK, terminals, apps), and keyboard selection.
- **Remote installs** — download a KDE Store or GitHub archive, validate and extract it, discover its color scheme or Plasma package, and turn it into an applyable theme.
- **Self-theming** — the app restyles its own surfaces, text, accents and controls from the selected palette. Toggle it in Settings if you prefer the default shell.
- **Palette previews** — every installed entry exposes its real values; remote cards say "no palette yet" instead of inventing colours from a name.
- **Dynamic wallpapers** — pick any image, let `matugen` derive a complete palette, apply that palette, or set the custom wallpaper on its own. The chosen image is remembered between launches.
- **Targets** — KDE color scheme, Caelestia / Quickshell scheme, Waybar, GTK 3/4, Qt 5/6, Konsole, kitty, foot, Spicetify and Vesktop.
- **Layout presets** — three Caelestia bar layouts and three KDE Plasma panel layouts, applied with a backup and a restore path.
- **User or system-wide** — KDE color schemes can be written to `~/.local/share/color-schemes` or `/usr/share/color-schemes` through `pkexec`.
- **Backups** — `~/.local/state/hyperthemes/backups/<timestamp>-<theme>/` contains the previous files and a manifest. Restore from the Settings page. Layouts use a separate `layout-backups/` tree.

Remote sources are no longer browse-only for color content. Selecting a remote card offers **Download & inspect**; HyperThemes saves the discovered palette file under `~/.local/share/hyperthemes/themes/` and then uses the same apply and rollback pipeline as local themes. If an archive contains only layout files or unsupported formats, the app says so and keeps the source link available.

## Build

Requirements:

- Qt 6.5 or newer with `Core`, `Gui`, `Network`, `Qml`, `Quick`, `QuickControls2`
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

Tagged releases (`v*`) also publish a `hyperthemes-<tag>-linux-x86_64.tar.gz` artifact from CI. It installs into a `usr/` prefix inside the tarball and expects the system Qt 6.5+ libraries.

## Optional tools

| Tool | Used for |
| --- | --- |
| `matugen` | Wallpaper palette generation |
| `bsdtar` | Inspecting and extracting remote theme archives |
| `plasma-apply-colorscheme` | Switching the KDE scheme after writing it |
| `plasma-apply-wallpaperimage` | Setting a generated wallpaper on Plasma |
| `qdbus6` / `plasmashell` | Applying KDE panel layout presets |
| `kpackagetool6` | Installing downloaded Plasma look-and-feel, style and color-scheme packages |
| `plasma-apply-lookandfeel` | Switching to an installed global theme |
| `caelestia` | Reloading Caelestia after a layout preset |
| `spicetify` | Spotify palette |
| `pkexec` | System-wide KDE writes |

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

## Remote installs

Remote cards keep the source page and also carry a download link. KDE Store entries use the OCS `downloadlink1`; GitHub entries use the repository's default-branch ZIP; GitLab entries use the project archive URL.

The downloader:

- caps the archive at 128 MB and extracted content at 512 MB;
- rejects absolute paths, `..` entries, Windows drive paths, and symbolic links;
- extracts with `bsdtar` into a cache directory, then scans for `.colors`, `.json` palette manifests, and Caelestia-style `.txt` schemes;
- copies supported files into `~/.local/share/hyperthemes/themes/<id>/`, so a later apply does not depend on the cache;
- picks a KDE `.colors` file first, then JSON, then text schemes.

A downloaded `.colors` file is applied through the KDE color scheme target. A palette JSON or Caelestia scheme gets the normal multi-target target list. Nothing is written to desktop configuration until **Apply** is pressed.

## Catalog data

The catalog ships 1,655 themes in 61 category folders under `catalog/`, one JSON file per theme:

- 30 Caelestia schemes — every flavour and light/dark mode.
- 360 Base16 and 240 Base24 palettes from [tinted-theming/schemes](https://github.com/tinted-theming/schemes) (MIT).
- 807 Gogh terminal schemes from [Gogh-Co/Gogh](https://github.com/Gogh-Co/Gogh) (MIT).
- 218 iTerm2 schemes from [mbadolato/iTerm2-Color-Schemes](https://github.com/mbadolato/iTerm2-Color-Schemes) (MIT).

Base16 and Base24 roles are mapped to the Material roles HyperThemes uses, including terminal colours from `base00`-`base0F`. Family rules in `scripts/generate_catalog.py` tag each file with a `category` (Catppuccin, Gruvbox, Tokyo Night, Nord and so on) while the `source` keeps the collection it came from.

Regenerate the catalog from the upstream checkouts:

```bash
python3 scripts/generate_catalog.py
python3 scripts/generate_catalog.py --base16 /path/to/schemes/base16
python3 scripts/generate_catalog.py --source /path/to/caelestia/schemes --base16 /path/to/schemes/base16 \
    --base24 /path/to/schemes/base24 --gogh /path/to/Gogh/themes --iterm2 /path/to/windowsterminal
```

The script wipes stale `*.json` files in the output directory before writing, so `catalog/` always matches the sources. Each entry has this shape:

```json
{
  "id": "bundled-dracula-medium-dark",
  "name": "Dracula Medium Dark",
  "description": "Purple, pink and cyan on a deep charcoal base.",
  "author": "Caelestia contributors",
  "license": "GPL-3.0-or-later",
  "source": "bundled",
  "category": "Dracula",
  "sourceUrl": "https://github.com/caelestia-dots/cli",
  "kind": "static",
  "mode": "dark",
  "flavour": "default",
  "targets": ["kde", "caelestia", "waybar", "gtk", "qt", "konsole", "kitty", "foot", "spicetify", "vesktop"],
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

## Layout presets

The interface uses a small bundled SVG icon set, compact controls and the selected palette for its own surfaces, text, accents and controls. **Settings → Appearance** turns that self-theming off if you prefer the default dark shell.

The **Layouts** page ships three Caelestia presets and three KDE Plasma presets:

- Caelestia top bar, bottom dock, and minimal top bar. These merge only the `bar` object into `~/.config/caelestia/shell.json`; general, service and lock settings are preserved. HyperThemes asks the running `caelestia` shell to restart when available.
- KDE top bar, auto-hide dock, and minimal top bar. These use Plasma's `org.kde.PlasmaShell.evaluateScript` to build the new panel first, then replace the old ones. Widgets are checked against `knownWidgetTypes` with fallbacks (Kickoff/Kicker, Pager/Activity Pager, Icons Tasks/Task Manager, and so on). The previous `plasma-org.kde.plasma.desktop-appletsrc` is backed up first.

Every layout apply creates a manifest under `~/.local/state/hyperthemes/layout-backups/` and offers **Restore**. Restoring KDE stops `plasma-plasmashell.service`, writes the backed-up file while Plasma cannot flush over it, then starts the service again; Caelestia is asked to restart through its CLI. A live end-to-end test of this flow is available with `HYPER_THEMES_LIVE_PLASMA=1 ./build/test_core kdeLayoutLive`.

## Current limits

- Remote installs understand color-scheme payloads and Plasma packages with a `metadata.json`. Archives that only contain panel layouts, icon packs, or app-specific assets are downloaded and inspected but cannot be applied as a palette or package; the app reports that instead of pretending it succeeded.
- KDE layout presets replace the current panels. Restore exists, but a custom panel arrangement should be backed up externally if it matters.
- Spicetify patches Spotify's client files; use `spicetify restore` if you want stock Spotify back.
- Konsole uses the scheme after you select **HyperThemes** in its profile settings.
- Vesktop needs a restart (or a theme toggle) after a color change.
- A Flatpak update can reset the Spotify patch; re-apply the Spicetify target after updating.
- System-wide writes only cover the KDE color scheme. Everything else stays in XDG user directories.

## Development

```bash
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/hyperthemes
```

The tests cover palette fallbacks, every template, apply/rollback, Caelestia layout merge/restore, KDE package handling, direct remote palette downloads, archive extraction, symlink rejection, per-file catalog loading (1,655 themes in well under a second), category filtering, target filters, palette-driven app colours, wallpaper persistence, and a QML smoke test. Set `HYPER_THEMES_LIVE_PLASMA=1` to also script the running Plasma session. `matugen` tests skip automatically when it is not installed. Warnings and QML errors go to stderr. The backup manifests are plain JSON, so an apply can be audited after the fact.
