#!/usr/bin/env python3
"""Build catalog/themes.json from Caelestia's named scheme tree.

The bundled catalog is deliberately data-only: HyperThemes can be built and
run without Caelestia installed, and local imports add whatever the user has.
Run from the repository root:

    python3 scripts/generate_catalog.py
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re

NAMES = {
    "catppuccin": "Catppuccin",
    "dracula": "Dracula",
    "everblush": "Everblush",
    "everforest": "Everforest",
    "gruvbox": "Gruvbox",
    "nord": "Nord",
    "oldworld": "Old World",
    "onedark": "One Dark",
    "redtorii": "Red Torii",
    "rosepine": "Rosé Pine",
    "shadotheme": "Shado",
    "solarized": "Solarized",
    "tokyonight": "Tokyo Night",
    "darkgreen": "Dark Green",
    "caelestia": "Caelestia",
}

DESCRIPTIONS = {
    "catppuccin": "Soft pastels on a low-contrast dark base.",
    "dracula": "Purple, pink and cyan on a deep charcoal base.",
    "everblush": "Muted rose and teal on a warm near-black base.",
    "everforest": "Forest greens and warm earth tones.",
    "gruvbox": "Retro warm browns with bold red and yellow accents.",
    "nord": "Cool arctic blues on a quiet slate base.",
    "oldworld": "A warm, aged palette with parchment highlights.",
    "onedark": "The familiar Atom One Dark palette.",
    "redtorii": "Deep lacquer reds with a torii-gate accent.",
    "rosepine": "Dusty rose, iris and pine on a soft base.",
    "shadotheme": "A high-contrast shadowed palette.",
    "solarized": "The classic Solarized dark balance.",
    "tokyonight": "Neon blue and violet over midnight.",
    "darkgreen": "Deep green surfaces with a restrained accent.",
    "caelestia": "The Caelestia house palette.",
}


def read_palette(path: pathlib.Path) -> dict[str, str]:
    palette: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 2 and re.fullmatch(r"[0-9a-fA-F]{6}", parts[1]):
            palette[parts[0]] = parts[1].lower()
    return palette


def pick_scheme(root: pathlib.Path, name: str) -> tuple[pathlib.Path, str, str]:
    candidates = sorted(root.glob(f"{name}/*/*.txt"))
    if not candidates:
        raise FileNotFoundError(f"no scheme files for {name}")

    def rank(path: pathlib.Path) -> tuple[int, int, str]:
        flavour, mode = path.parent.name, path.stem
        return (0 if flavour == "default" else 1, 0 if mode == "dark" else 1, str(path))

    path = min(candidates, key=rank)
    return path, path.parent.name, path.stem


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        type=pathlib.Path,
        default=pathlib.Path.home() / ".local/lib/caelestia/schemes",
        help="Caelestia scheme tree",
    )
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("catalog/themes.json"))
    args = parser.parse_args()

    themes = []
    for name in sorted(NAMES):
        path, flavour, mode = pick_scheme(args.source, name)
        palette = read_palette(path)
        if not palette:
            continue
        themes.append(
            {
                "id": f"bundled-{name}",
                "name": NAMES.get(name, name.title()),
                "description": DESCRIPTIONS.get(name, f"The {name} palette."),
                "author": "Caelestia contributors",
                "license": "GPL-3.0-or-later",
                "source": "bundled",
                "sourceUrl": "https://github.com/caelestia-dots/cli",
                "kind": "static",
                "mode": mode,
                "flavour": flavour,
                "variant": "tonalspot",
                "targets": [
                    "kde",
                    "caelestia",
                    "waybar",
                    "gtk",
                    "qt",
                    "konsole",
                    "kitty",
                    "spicetify",
                    "vesktop",
                ],
                "palette": palette,
            }
        )

    document = {"schema": 1, "themes": themes}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(themes)} themes to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
