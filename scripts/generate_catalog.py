#!/usr/bin/env python3
"""Build catalog/themes.json from local scheme trees.

Two data sources are supported:

* Caelestia's named scheme tree (default: ~/.local/lib/caelestia/schemes).
* A checkout of tinted-theming/schemes' ``base16`` directory, which adds
  hundreds of published Base16 palettes.

Both are optional. HyperThemes ships the generated JSON, so building the app
never needs the sources or a network connection. Run from the repository root:

    python3 scripts/generate_catalog.py
    python3 scripts/generate_catalog.py --base16 /path/to/schemes/base16
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
    "catppuccin": "Soft pastels on a low-contrast base.",
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
    "solarized": "The classic Solarized balance.",
    "tokyonight": "Neon blue and violet over midnight.",
    "darkgreen": "Deep green surfaces with a restrained accent.",
    "caelestia": "The Caelestia house palette.",
}

FLAVOURS = {
    "default": "Default",
    "medium": "Medium",
    "hard": "Hard",
    "soft": "Soft",
    "mocha": "Mocha",
    "macchiato": "Macchiato",
    "frappe": "Frappé",
    "latte": "Latte",
    "dawn": "Dawn",
    "main": "Main",
    "moon": "Moon",
}

TARGETS = [
    "kde",
    "caelestia",
    "waybar",
    "gtk",
    "qt",
    "konsole",
    "kitty",
    "foot",
    "spicetify",
    "vesktop",
]


def read_palette(path: pathlib.Path) -> dict[str, str]:
    palette: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 2 and re.fullmatch(r"[0-9a-fA-F]{6}", parts[1]):
            palette[parts[0]] = parts[1].lower()
    return palette


def caelestia_themes(root: pathlib.Path) -> list[dict]:
    themes = []
    for path in sorted(root.glob("*/*/*.txt")):
        family = path.parent.parent.name
        flavour = path.parent.name
        mode = path.stem
        palette = read_palette(path)
        if not palette:
            continue
        display = NAMES.get(family, family.title())
        flavour_label = FLAVOURS.get(flavour, flavour.title())
        if flavour_label not in ("Default", display):
            display = f"{display} {flavour_label}"
        if mode:
            display = f"{display} {'Light' if mode == 'light' else 'Dark'}"

        theme_id = f"bundled-{family}-{flavour}-{mode}"
        themes.append(
            {
                "id": theme_id,
                "name": display,
                "description": DESCRIPTIONS.get(family, f"The {family} palette."),
                "author": "Caelestia contributors",
                "license": "GPL-3.0-or-later",
                "source": "bundled",
                "sourceUrl": "https://github.com/caelestia-dots/cli",
                "kind": "static",
                "mode": mode,
                "flavour": flavour,
                "variant": "tonalspot",
                "targets": TARGETS,
                "palette": palette,
            }
        )
    return themes


def luma(value: str) -> float:
    value = value.lstrip("#")
    if len(value) != 6:
        return 0.0
    r, g, b = (int(value[i : i + 2], 16) / 255.0 for i in (0, 2, 4))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def base16_palette(base: dict[str, str], mode: str) -> dict[str, str]:
    b = {key: base.get(key, "000000").lstrip("#").lower() for key in (f"base0{i:X}" for i in range(16))}

    def on(colour: str) -> str:
        return b["base00"] if luma(colour) > 0.55 else b["base07"]

    primary, secondary, tertiary = b["base0D"], b["base0E"], b["base0C"]
    palette = {
        "background": b["base00"],
        "onBackground": b["base05"],
        "surface": b["base00"],
        "surfaceDim": b["base00"],
        "surfaceBright": b["base02"],
        "surfaceContainerLowest": b["base00"],
        "surfaceContainerLow": b["base01"],
        "surfaceContainer": b["base01"],
        "surfaceContainerHigh": b["base02"],
        "surfaceContainerHighest": b["base02"],
        "onSurface": b["base05"],
        "surfaceVariant": b["base02"],
        "onSurfaceVariant": b["base04"],
        "outline": b["base03"],
        "outlineVariant": b["base02"],
        "inverseSurface": b["base06"],
        "inverseOnSurface": b["base00"],
        "shadow": b["base00"],
        "scrim": b["base00"],
        "surfaceTint": primary,
        "primary": primary,
        "onPrimary": on(primary),
        "primaryContainer": b["base02"],
        "onPrimaryContainer": b["base06"],
        "inversePrimary": b["base04"],
        "primaryFixed": primary,
        "primaryFixedDim": primary,
        "onPrimaryFixed": on(primary),
        "onPrimaryFixedVariant": on(primary),
        "secondary": secondary,
        "onSecondary": on(secondary),
        "secondaryContainer": b["base02"],
        "onSecondaryContainer": b["base06"],
        "secondaryFixed": secondary,
        "secondaryFixedDim": secondary,
        "onSecondaryFixed": on(secondary),
        "onSecondaryFixedVariant": on(secondary),
        "tertiary": tertiary,
        "onTertiary": on(tertiary),
        "tertiaryContainer": b["base02"],
        "onTertiaryContainer": b["base06"],
        "tertiaryFixed": tertiary,
        "tertiaryFixedDim": tertiary,
        "onTertiaryFixed": on(tertiary),
        "onTertiaryFixedVariant": on(tertiary),
        "error": b["base08"],
        "onError": on(b["base08"]),
        "errorContainer": b["base08"],
        "onErrorContainer": on(b["base08"]),
    }

    terms = [
        "base00",
        "base08",
        "base0B",
        "base0A",
        "base0D",
        "base0E",
        "base0C",
        "base05",
        "base03",
        "base08",
        "base0B",
        "base0A",
        "base0D",
        "base0E",
        "base0C",
        "base07",
    ]
    for index, key in enumerate(terms):
        palette[f"term{index}"] = b[key]

    if mode == "light":
        # Light Base16 schemes already provide light base00 surfaces; keep
        # the names meaningful for filtering without inventing colours.
        palette["mode"] = "light"
    return palette


def read_base16_yaml(path: pathlib.Path) -> dict | None:
    name = author = variant = ""
    palette: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        match = re.match(r"^(name|author|variant):\s*[\"']?(.*?)[\"']?$", stripped)
        if match:
            key, value = match.groups()
            if key == "name":
                name = value
            elif key == "author":
                author = value
            else:
                variant = value
            continue
        match = re.match(r"^(base0[0-9a-fA-F]):\s*[\"']?(#[0-9a-fA-F]{6})", stripped)
        if match:
            palette[match.group(1)] = match.group(2)
    if not name or not palette:
        return None
    return {"name": name, "author": author or "tinted-theming contributors", "variant": variant, "palette": palette}


def base16_themes(root: pathlib.Path) -> list[dict]:
    themes = []
    for path in sorted(root.glob("*.yaml")):
        data = read_base16_yaml(path)
        if data is None:
            continue
        mode = "light" if data["variant"] == "light" else "dark"
        themes.append(
            {
                "id": f"base16-{path.stem}",
                "name": data["name"],
                "description": "Base16 palette from the tinted-theming collection.",
                "author": data["author"],
                "license": "MIT",
                "source": "base16",
                "sourceUrl": f"https://github.com/tinted-theming/schemes/blob/spec-0.11/base16/{path.name}",
                "kind": "static",
                "mode": mode,
                "flavour": "base16",
                "variant": "base16",
                "targets": TARGETS,
                "palette": base16_palette(data["palette"], mode),
            }
        )
    return themes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        type=pathlib.Path,
        default=pathlib.Path.home() / ".local/lib/caelestia/schemes",
        help="Caelestia scheme tree (optional)",
    )
    parser.add_argument(
        "--base16",
        type=pathlib.Path,
        default=None,
        help="tinted-theming/schemes base16 directory (optional)",
    )
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("catalog/themes.json"))
    args = parser.parse_args()

    themes: list[dict] = []
    if args.source.is_dir():
        themes.extend(caelestia_themes(args.source))
    else:
        print(f"skipping Caelestia tree: {args.source} does not exist")
    if args.base16 and args.base16.is_dir():
        themes.extend(base16_themes(args.base16))
    elif args.base16:
        print(f"skipping Base16 tree: {args.base16} does not exist")

    seen: set[str] = set()
    unique = []
    for theme in themes:
        if theme["id"] in seen:
            continue
        seen.add(theme["id"])
        unique.append(theme)

    document = {"schema": 1, "themes": unique}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(unique)} themes to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
