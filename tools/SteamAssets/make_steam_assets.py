#!/usr/bin/env python3
"""Makes the Steam store graphics from the game's own art.

    python tools/SteamAssets/make_steam_assets.py [out_folder]     (default: steam/store_assets)

Needs Pillow (pip install pillow). Re-run it whenever the logo, background or
crop art changes. Sizes are Steamworks' "Graphical Assets" requirements; see
steam/README.md for where each file is uploaded.
"""
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
A = lambda p: Image.open(os.path.join(ROOT, "assets", p)).convert("RGBA")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "steam", "store_assets")

BG = A("menu/background.png")          # 1280 x 720 sunset
LOGO = A("menu/logo.png")              # 900 x 112, with tagline
TITLE = LOGO.crop((0, 0, 900, 70))     # the name alone, for small formats
SPRITES = {n: A(f"crops/{n}.png") for n in ("carrot", "pumpkin", "lettuce", "turnip", "corn", "eggplant", "chilli", "broccoli")}
FARMER = A("farm/farmer.png")
SPRITE_ORDER = ["carrot", "pumpkin", "farmer", "lettuce", "turnip"]

PANEL = (246, 234, 202, 255)       # parchment, so the logo's green, orange and purple all read
PANEL_EDGE = (110, 78, 52, 255)


def tight(im):
    return im.crop(im.getbbox())


def scaled(im, factor):
    """Pixel art scaled with hard edges (factor may be fractional)."""
    return im.resize((max(1, round(im.width * factor)), max(1, round(im.height * factor))), Image.NEAREST)


def backdrop(w, h, sun_x=0.80, zoom=1.0, top=None):
    """The sunset, covering w x h, kept so the sun and hills stay in view."""
    s = max(w / BG.width, h / BG.height) * zoom
    big = BG.resize((round(BG.width * s), round(BG.height * s)), Image.NEAREST if s >= 1 else Image.LANCZOS)
    cx = min(max(round(big.width * sun_x - w / 2), 0), big.width - w)
    cy = big.height - h if h < big.height else 0   # keep the hills, trim the sky
    # Nudge up a little so there's grass for the crops but sky for the sun.
    cy = max(0, cy - round(0.06 * big.height)) if h < big.height else 0
    if top is not None:
        cy = min(max(top, 0), big.height - h)
    return big.crop((cx, cy, cx + w, cy + h))


def grass_line(im, frac):
    """Fill from frac*height down with the hill green, so sprites stand on grass."""
    d = ImageDraw.Draw(im)
    d.rectangle((0, round(im.height * frac), im.width, im.height), fill=(98, 128, 80, 255))


def sprite_row(im, y_bottom, unit, order=SPRITE_ORDER, cx=None, gap=0.15):
    """Crops and the farmer standing in a line, bottoms at y_bottom."""
    imgs = [FARMER if n == "farmer" else SPRITES[n] for n in order]
    imgs = [scaled(tight(i), unit) for i in imgs]
    total = sum(i.width for i in imgs) + round(unit * 32 * gap) * (len(imgs) - 1)
    x = (im.width - total) // 2 if cx is None else round(cx - total / 2)
    for i in imgs:
        sh = Image.new("RGBA", (i.width, max(2, i.height // 8)), (0, 0, 0, 0))
        ImageDraw.Draw(sh).ellipse((0, 0, i.width - 1, sh.height - 1), fill=(30, 45, 30, 90))
        im.alpha_composite(sh, (x, y_bottom - sh.height // 2))
        im.alpha_composite(i, (x, y_bottom - i.height))
        x += i.width + round(unit * 32 * gap)


def logo_on_panel(im, logo, cx, cy, width_frac, max_h_frac=None):
    """Logo scaled in 0.25 steps (its pixels are 4px), on a dark rounded panel so it reads on the sky."""
    f = im.width * width_frac / logo.width
    if max_h_frac:
        f = min(f, im.height * max_h_frac / logo.height)
    f = max(0.25, int(f * 4) / 4)
    lg = scaled(logo, f)
    pad = max(6, round(lg.height * 0.22))
    pw, ph = lg.width + pad * 2, lg.height + pad * 2
    panel = Image.new("RGBA", (pw, ph), (0, 0, 0, 0))
    d = ImageDraw.Draw(panel)
    r = max(4, pad // 2)
    d.rounded_rectangle((0, 0, pw - 1, ph - 1), r, fill=PANEL, outline=PANEL_EDGE, width=max(2, pad // 6))
    x, y = round(cx - pw / 2), round(cy - ph / 2)
    im.alpha_composite(panel, (x, y))
    im.alpha_composite(lg, (x + pad, y + pad))
    return (x, y, pw, ph)


def save(im, name, jpg=False):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name)
    if jpg:
        im.convert("RGB").save(path, quality=95)
    else:
        im.save(path)
    print(f"{name:34s} {im.width} x {im.height}")


def capsule(w, h, logo=LOGO, logo_w=0.80, logo_cy=0.34, sprites=True, unit=None, max_h=None):
    im = backdrop(w, h)
    grass_line(im, 0.62)
    if sprites:
        u = unit or (h * 0.30 / 32)
        sprite_row(im, round(h * 0.93), u)
    logo_on_panel(im, logo, w / 2, h * logo_cy, logo_w, max_h)
    return im


# --- Store page capsules -----------------------------------------------------
save(capsule(920, 430, logo_w=0.78, logo_cy=0.30, max_h=0.30).convert("RGB"), "header_capsule_920x430.png")
save(capsule(462, 174, logo=TITLE, logo_w=0.86, logo_cy=0.40, unit=2.2, max_h=0.34).convert("RGB"), "small_capsule_462x174.png")
save(capsule(1232, 706, logo_w=0.74, logo_cy=0.30, unit=5.2, max_h=0.26).convert("RGB"), "main_capsule_1232x706.png")

# Vertical: a tall crop with the sun up top, the logo in the middle, the crew below.
v = backdrop(748, 896, sun_x=0.82, zoom=1.0)
grass_line(v, 0.58)
logo_on_panel(v, LOGO, 748 / 2, 896 * 0.22, 0.88)
sprite_row(v, round(896 * 0.90), 6.0, order=["pumpkin", "farmer", "carrot"], gap=0.25)
save(v.convert("RGB"), "vertical_capsule_748x896.png")

# --- Library (the player's own Steam library) --------------------------------
lib = backdrop(600, 900, sun_x=0.82)
grass_line(lib, 0.58)
logo_on_panel(lib, LOGO, 300, 900 * 0.22, 0.90)
sprite_row(lib, round(900 * 0.90), 5.4, order=["pumpkin", "farmer", "carrot"], gap=0.25)
save(lib.convert("RGB"), "library_capsule_600x900.png")

save(capsule(920, 430, logo_w=0.78, logo_cy=0.30, max_h=0.30).convert("RGB"), "library_header_920x430.png")

# Hero: a wide picture with no logo (Steam lays the logo over it).
hero = backdrop(3840, 1240, sun_x=0.80, zoom=1.0, top=300)  # whole sun in view
sprite_row(hero, round(1240 * 0.92), 9.0, cx=3840 * 0.30, order=["carrot", "pumpkin", "farmer", "lettuce", "turnip"])
save(hero.convert("RGB"), "library_hero_3840x1240.png")

# Logo: transparent, no panel; Steam sets it over the hero.
lw = 1280
f = int(lw / LOGO.width * 4) / 4
lg = scaled(LOGO, f)
canvas = Image.new("RGBA", (1280, 720), (0, 0, 0, 0))
canvas.alpha_composite(lg, ((1280 - lg.width) // 2, (720 - lg.height) // 2))
save(canvas, "library_logo_1280x720.png")

# Page background (faded behind the store page): the sunset, plain.
save(backdrop(1438, 810).convert("RGB"), "page_background_1438x810.png")

# --- Icons -------------------------------------------------------------------
icon = Image.open(os.path.join(ROOT, "icon", "IncraVegetable_1024.png")).convert("RGBA")
bg = Image.new("RGBA", icon.size, (98, 128, 80, 255))
bg.alpha_composite(icon)
save(bg.resize((184, 184), Image.LANCZOS), "community_icon_184x184.jpg", jpg=True)
save(bg.resize((256, 256), Image.LANCZOS), "client_icon_256x256.png")
print("\nAlso upload icon/IncraVegetable.ico as the client icon, and 5+ screenshots at 1920 x 1080.")
