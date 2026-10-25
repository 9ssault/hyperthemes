#!/usr/bin/env python3
"""Build catalog/themes.json from published palette collections.

Supported sources (all optional, all offline once downloaded):

* Caelestia's named scheme tree (default: ~/.local/lib/caelestia/schemes).
* tinted-theming/schemes ``base16`` and ``base24`` directories.
* Gogh-Co/Gogh ``themes`` directory (``.yml``).
* mbadolato/iTerm2-Color-Schemes ``windowsterminal`` directory (``.json``).

Every scheme is written as its own file under ``catalog/<category>/<id>.json``
so the collection can be browsed, diffed and extended one theme at a time.
HyperThemes ships the generated files, so building the app never needs the
sources or a network connection. Run from the repository root:

    python3 scripts/generate_catalog.py --base16 /path/base16 --base24 /path/base24
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil

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

# Families are matched against the scheme name so the app can browse the
# catalog by collection instead of one flat list. Order matters: the first
# matching family wins, so put the specific rules before the broad ones.
CATEGORY_RULES = [
    ("Catppuccin", r"catppuccin"),
    ("Dracula", r"dracula"),
    ("Gruvbox", r"gruvbox"),
    ("Nord", r"\bnord\b|nordic|nordfox"),
    ("Solarized", r"solarized"),
    ("Tokyo Night", r"tokyo\s*(night|city)"),
    ("Atom One", r"\bone\s+(dark|light|half|space)\b|atom\s*one|onedark|doom\s*one"),
    ("Everforest", r"everforest"),
    ("Rosé Pine", r"ros[eé]\s*pine|rosepine"),
    ("Monokai", r"monokai"),
    ("Kanagawa", r"kanagawa"),
    ("Ayu", r"\bayu\b"),
    ("Material", r"material"),
    ("GitHub", r"github"),
    ("Nightfox", r"(night|day|nord|dawn|dusk|tera|carbon)fox"),
    ("Everblush", r"everblush"),
    ("Atelier", r"atelier"),
    ("Base2Tone", r"base2tone"),
    ("Base4Tone", r"base4tone"),
    ("Tempus", r"tempus"),
    ("Noctis", r"noctis"),
    ("Modus", r"modus"),
    ("Horizon", r"horizon"),
    ("Flexoki", r"flexoki"),
    ("Penumbra", r"penumbra"),
    ("Tomorrow", r"tomorrow"),
    ("Windows Terminal", r"\bwindows\b"),
    ("Iceberg", r"iceberg"),
    ("PaperColor", r"papercolor|paper\s*color"),
    ("Selenized", r"selenized"),
    ("Zenburn", r"zenburn|zenbones"),
    ("Ocean", r"\bocean\b|oceanic"),
    ("Synthwave", r"synthwave|synth\s*midnight"),
    ("Cyberpunk", r"cyberpunk"),
    ("Black Metal", r"black\s*metal"),
    ("Spacegray", r"space\s*gray"),
    ("Snazzy", r"snazzy"),
    ("Rebecca", r"rebecca"),
    ("Sakura", r"sakura"),
    ("Chalk", r"chalk"),
    ("Brogrammer", r"brogrammer"),
    ("Desert", r"desert"),
    ("Apprentice", r"apprentice"),
    ("Cobalt", r"cobalt"),
    ("Palenight", r"palenight"),
    ("Night Owl", r"night\s*owl"),
    ("Oxocarbon", r"oxocarbon"),
    ("Poimandres", r"poimandres"),
    ("Unikitty", r"unikitty"),
    ("Wild Cherry", r"wild\s*cherry"),
    ("Moonlight", r"moonlight"),
    ("Gotham", r"gotham"),
    ("Risottonero", r"risotto"),
    ("Shades of Purple", r"shades\s*of\s*purple"),
    ("Velvet", r"velvet"),
    ("Pastel", r"pastel"),
    ("Monochrome", r"monochrome"),
]

SOURCE_CATEGORIES = {
    "bundled": "Caelestia",
    "base16": "Base16",
    "base24": "Base24",
    "gogh": "Gogh",
    "iterm2": "iTerm2",
}


def slugify(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-") or "themes"


def assign_categories(themes: list[dict]) -> None:
    for theme in themes:
        name = theme["name"].lower()
        theme["category"] = SOURCE_CATEGORIES.get(theme["source"], theme["source"])
        for label, pattern in CATEGORY_RULES:
            if re.search(pattern, name):
                theme["category"] = label
                break


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


def strip_hex(value: str) -> str:
    return value.strip().lstrip("#").lower()


def luma(value: str) -> float:
    value = strip_hex(value)
    if len(value) != 6:
        return 0.0
    r, g, b = (int(value[i : i + 2], 16) / 255.0 for i in (0, 2, 4))
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a: str, b: str) -> float:
    la, lb = luma(a), luma(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


def mix(a: str, b: str, weight: float) -> str:
    a, b = strip_hex(a), strip_hex(b)
    result = []
    for index in (0, 2, 4):
        av = int(a[index : index + 2], 16)
        bv = int(b[index : index + 2], 16)
        result.append(f"{round(av * (1.0 - weight) + bv * weight):02x}")
    return "".join(result)


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

        themes.append(
            {
                "id": f"bundled-{family}-{flavour}-{mode}",
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


def terminal_palette(
    background: str,
    foreground: str,
    terms: list[str],
    mode: str,
    surfaces: list[str] | None = None,
) -> dict[str, str]:
    bg, fg = strip_hex(background), strip_hex(foreground)
    t = [strip_hex(value) for value in terms]

    if surfaces and len(surfaces) >= 5:
        lowest, low, container, high, highest = (strip_hex(value) for value in surfaces[:5])
    else:
        lowest = bg
        low = mix(bg, fg, 0.05)
        container = mix(bg, fg, 0.08)
        high = mix(bg, fg, 0.12)
        highest = mix(bg, fg, 0.16)

    primary, secondary, tertiary, error = t[4], t[5], t[6], t[1]

    def on(colour: str) -> str:
        return bg if contrast(bg, colour) >= contrast(fg, colour) else fg

    palette = {
        "background": bg,
        "onBackground": fg,
        "surface": bg,
        "surfaceDim": bg,
        "surfaceBright": high,
        "surfaceContainerLowest": lowest,
        "surfaceContainerLow": low,
        "surfaceContainer": container,
        "surfaceContainerHigh": high,
        "surfaceContainerHighest": highest,
        "onSurface": fg,
        "surfaceVariant": high,
        "onSurfaceVariant": mix(fg, bg, 0.25),
        "outline": mix(fg, bg, 0.5),
        "outlineVariant": mix(fg, bg, 0.75),
        "inverseSurface": fg,
        "inverseOnSurface": bg,
        "shadow": bg,
        "scrim": bg,
        "surfaceTint": primary,
        "primary": primary,
        "onPrimary": on(primary),
        "primaryContainer": mix(bg, primary, 0.22),
        "onPrimaryContainer": fg,
        "inversePrimary": mix(primary, fg, 0.4),
        "primaryFixed": primary,
        "primaryFixedDim": primary,
        "onPrimaryFixed": on(primary),
        "onPrimaryFixedVariant": on(primary),
        "secondary": secondary,
        "onSecondary": on(secondary),
        "secondaryContainer": mix(bg, secondary, 0.22),
        "onSecondaryContainer": fg,
        "secondaryFixed": secondary,
        "secondaryFixedDim": secondary,
        "onSecondaryFixed": on(secondary),
        "onSecondaryFixedVariant": on(secondary),
        "tertiary": tertiary,
        "onTertiary": on(tertiary),
        "tertiaryContainer": mix(bg, tertiary, 0.22),
        "onTertiaryContainer": fg,
        "tertiaryFixed": tertiary,
        "tertiaryFixedDim": tertiary,
        "onTertiaryFixed": on(tertiary),
        "onTertiaryFixedVariant": on(tertiary),
        "error": error,
        "onError": on(error),
        "errorContainer": error,
        "onErrorContainer": on(error),
    }

    for index, colour in enumerate(t[:16]):
        palette[f"term{index}"] = colour

    palette["mode"] = mode
    return palette


BASE16_TERMS = [
    "base00", "base08", "base0B", "base0A", "base0D", "base0E", "base0C", "base05",
    "base03", "base08", "base0B", "base0A", "base0D", "base0E", "base0C", "base07",
]

BASE24_TERMS = [
    "base00", "base08", "base0B", "base0A", "base0D", "base0E", "base0C", "base05",
    "base03", "base12", "base14", "base13", "base16", "base17", "base15", "base07",
]


def read_base_yaml(path: pathlib.Path) -> dict | None:
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
        match = re.match(r"^(base[0-9a-fA-F]{2}):\s*[\"']?(#[0-9a-fA-F]{6})", stripped)
        if match:
            palette[match.group(1)] = match.group(2)
    if not name or not palette:
        return None
    return {
        "name": name,
        "author": author or "tinted-theming contributors",
        "variant": variant,
        "palette": palette,
    }


def base_schemes(root: pathlib.Path, system: str, term_keys: list[str], prefix: str) -> list[dict]:
    themes = []
    term_keys = [key.lower() for key in term_keys]
    for path in sorted(root.glob("*.yaml")):
        data = read_base_yaml(path)
        if data is None:
            continue
        base = {key.lower(): strip_hex(value) for key, value in data["palette"].items()}
        if not all(key in base for key in term_keys):
            continue
        mode = "light" if data["variant"] == "light" else "dark"
        terms = [base[key] for key in term_keys]
        surfaces = [base.get("base00", terms[0]), base.get("base01", terms[0]),
                    base.get("base01", terms[0]), base.get("base02", terms[0]),
                    base.get("base02", terms[0])]
        themes.append(
            {
                "id": f"{prefix}-{path.stem}",
                "name": data["name"],
                "description": f"{system} palette from the tinted-theming collection.",
                "author": data["author"],
                "license": "MIT",
                "source": prefix,
                "sourceUrl": f"https://github.com/tinted-theming/schemes/blob/spec-0.11/{prefix}/{path.name}",
                "kind": "static",
                "mode": mode,
                "flavour": prefix,
                "variant": prefix,
                "targets": TARGETS,
                "palette": terminal_palette(
                    base.get("base00", terms[0]),
                    base.get("base05", terms[7]),
                    terms,
                    mode,
                    surfaces,
                ),
            }
        )
    return themes


def yaml_value(raw: str) -> str:
    value = raw.strip()
    if value[:1] in ("'", '"'):
        quote = value[0]
        end = value.find(quote, 1)
        return value[1:end] if end > 0 else value[1:]
    return value.split("#", 1)[0].strip()


def read_gogh_yaml(path: pathlib.Path) -> dict | None:
    name = author = variant = background = foreground = ""
    colours: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        match = re.match(r"^(name|author|variant|background|foreground|color_\d{2}):\s*(.*)$", stripped)
        if not match:
            continue
        key, raw = match.groups()
        value = yaml_value(raw)
        if key == "name":
            name = value
        elif key == "author":
            author = value
        elif key == "variant":
            variant = value
        elif key == "background":
            background = value
        elif key == "foreground":
            foreground = value
        else:
            colours[key] = value
    if not name or not background or not foreground or len(colours) < 16:
        return None
    if variant not in ("dark", "light"):
        variant = "light" if luma(background) > 0.5 else "dark"
    return {
        "name": name,
        "author": author or "Gogh contributors",
        "variant": variant,
        "background": background,
        "foreground": foreground,
        "colours": [colours[f"color_{index:02d}"] for index in range(1, 17)],
    }


def gogh_themes(root: pathlib.Path) -> list[dict]:
    themes = []
    for path in sorted(root.glob("*.yml")):
        data = read_gogh_yaml(path)
        if data is None:
            continue
        themes.append(
            {
                "id": f"gogh-{path.stem}",
                "name": data["name"],
                "description": "Gogh terminal palette.",
                "author": data["author"],
                "license": "MIT",
                "source": "gogh",
                "sourceUrl": f"https://github.com/Gogh-Co/Gogh/blob/master/themes/{path.name}",
                "kind": "static",
                "mode": data["variant"],
                "flavour": "gogh",
                "variant": "gogh",
                "targets": TARGETS,
                "palette": terminal_palette(
                    data["background"], data["foreground"], data["colours"], data["variant"]
                ),
            }
        )
    return themes


ITERM_KEYS = [
    "black", "red", "green", "yellow", "blue", "purple", "cyan", "white",
    "brightBlack", "brightRed", "brightGreen", "brightYellow", "brightBlue",
    "brightPurple", "brightCyan", "brightWhite",
]


def iterm_themes(root: pathlib.Path) -> list[dict]:
    themes = []
    for path in sorted(root.glob("*.json")):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            continue
        background = data.get("background")
        foreground = data.get("foreground")
        if not background or not foreground or not all(key in data for key in ITERM_KEYS):
            continue
        mode = "light" if luma(background) > 0.5 else "dark"
        terms = [data[key] for key in ITERM_KEYS]
        themes.append(
            {
                "id": f"iterm2-{path.stem}",
                "name": data.get("name") or path.stem,
                "description": "iTerm2 terminal palette.",
                "author": "iTerm2-Color-Schemes contributors",
                "license": "MIT",
                "source": "iterm2",
                "sourceUrl": f"https://github.com/mbadolato/iTerm2-Color-Schemes/blob/master/windowsterminal/{path.name}",
                "kind": "static",
                "mode": mode,
                "flavour": "iterm2",
                "variant": "iterm2",
                "targets": TARGETS,
                "palette": terminal_palette(background, foreground, terms, mode),
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
    parser.add_argument("--base16", type=pathlib.Path, default=None, help="tinted-theming/schemes base16 directory")
    parser.add_argument("--base24", type=pathlib.Path, default=None, help="tinted-theming/schemes base24 directory")
    parser.add_argument("--gogh", type=pathlib.Path, default=None, help="Gogh-Co/Gogh themes directory")
    parser.add_argument("--iterm2", type=pathlib.Path, default=None, help="iTerm2-Color-Schemes windowsterminal directory")
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("catalog"),
                        help="catalog directory; one file per theme is written below it")
    args = parser.parse_args()

    themes: list[dict] = []
    seen_names: set[str] = set()

    def append(entries: list[dict], unique_names: bool) -> None:
        for entry in entries:
            key = entry["name"].strip().lower()
            if unique_names and key in seen_names:
                continue
            seen_names.add(key)
            themes.append(entry)

    if args.source.is_dir():
        append(caelestia_themes(args.source), False)
    else:
        print(f"skipping Caelestia tree: {args.source} does not exist")
    if args.base16 and args.base16.is_dir():
        append(base_schemes(args.base16, "Base16", BASE16_TERMS, "base16"), False)
    if args.base24 and args.base24.is_dir():
        append(base_schemes(args.base24, "Base24", BASE24_TERMS, "base24"), False)
    if args.gogh and args.gogh.is_dir():
        append(gogh_themes(args.gogh), True)
    if args.iterm2 and args.iterm2.is_dir():
        append(iterm_themes(args.iterm2), True)

    seen_ids: set[str] = set()
    unique = []
    for theme in themes:
        if theme["id"] in seen_ids:
            continue
        seen_ids.add(theme["id"])
        unique.append(theme)

    assign_categories(unique)

    if args.out.exists():
        for stale in args.out.rglob("*.json"):
            stale.unlink()
        for folder in sorted(args.out.rglob("*"), reverse=True):
            if folder.is_dir() and not any(folder.iterdir()):
                shutil.rmtree(folder)

    written = 0
    for theme in unique:
        folder = args.out / slugify(theme["category"])
        folder.mkdir(parents=True, exist_ok=True)
        (folder / f"{slugify(theme['id'])}.json").write_text(
            json.dumps(theme, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
        )
        written += 1

    print(f"wrote {written} themes to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
