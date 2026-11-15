#!/usr/bin/env python3
"""Draws a 64 x 64 pixel-art badge for every achievement, from the game's own sprites.

    python tools/AchievementArt/make_achievement_art.py

Writes assets/achievements/<id>.png and <id>_locked.png (the game and the Steam /
Game Center / Google Play icon export pick them up; see README, "Achievements").
Needs Pillow. The frame colour shows the difficulty: bronze, silver or gold.
To change a badge, edit BADGES below and run it again (or paint over the PNG).
"""
import os

from PIL import Image, ImageDraw, ImageEnhance

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
OUT = os.path.join(ROOT, "assets", "achievements")
SIZE = 64


def sprite(path):
    im = Image.open(os.path.join(ROOT, "assets", path + ".png")).convert("RGBA")
    return im.crop(im.getbbox())


def shrink(im):
    """Half size (exactly 2:1, so the pixels stay crisp) for the 64 px sprites; smaller ones are left alone."""
    if max(im.size) <= 40:
        return im
    return im.resize((im.width // 2, im.height // 2), Image.NEAREST)


TIERS = {
    "bronze": ((176, 112, 64), (120, 72, 44), (222, 160, 104)),
    "silver": ((190, 198, 190), (112, 122, 116), (236, 242, 236)),
    "gold": ((232, 192, 84), (160, 118, 40), (252, 232, 150)),
}
FILL = (44, 62, 46, 255)
GLINT = (252, 240, 170, 255)

# 3 x 5 digits, for the badges that show a number.
DIGITS = {
    "0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111",
    "4": "101101111001001", "5": "111100111001111", "6": "111100111101111", "7": "111001001001001",
    "8": "111101111101111", "9": "111101111001111",
}
CROWN = [
    "..X...XXX...X..",
    ".XXX..XXX..XXX.",
    ".XXXX.XXX.XXXX.",
    ".XXXXXXXXXXXXX.",
    ".XXXXXXXXXXXXX.",
    ".XXXXXXXXXXXXX.",
    ".XXXXXXXXXXXXX.",
]


def paste(base, im, cx, cy):
    base.alpha_composite(im, (round(cx - im.width / 2), round(cy - im.height / 2)))


def sparkle(base, cx, cy, r=2):
    d = ImageDraw.Draw(base)
    d.line((cx - r, cy, cx + r, cy), fill=GLINT)
    d.line((cx, cy - r, cx, cy + r), fill=GLINT)
    d.point((cx, cy), fill=(255, 255, 255, 255))


def number(base, text, right, bottom, col=(255, 250, 225, 255)):
    """A small number, bottom-right, with a dark outline so it reads on any art."""
    scale = 2
    w = len(text) * 4 * scale - scale
    layer = Image.new("RGBA", (w, 5 * scale), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i, ch in enumerate(text):
        for k, bit in enumerate(DIGITS[ch]):
            if bit == "1":
                x, y = (i * 4 + k % 3) * scale, (k // 3) * scale
                d.rectangle((x, y, x + scale - 1, y + scale - 1), fill=col)
    x0, y0 = right - w, bottom - 5 * scale
    outline = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    mask = layer.getchannel("A")
    shadow = Image.new("RGBA", layer.size, (30, 36, 30, 255))
    for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1), (1, 1)):
        base.paste(shadow, (x0 + dx, y0 + dy), mask)
    base.alpha_composite(layer, (x0, y0))


def crown(base, cx, top):
    d = ImageDraw.Draw(base)
    for y, row in enumerate(CROWN):
        for x, c in enumerate(row):
            if c == "X":
                shade = (252, 218, 96, 255) if y < 5 else (196, 142, 48, 255)
                d.point((cx - 7 + x, top + y), fill=shade)
    for x, y in ((3, 1), (7, 0), (11, 1)):
        d.point((cx - 7 + x, top + y + 2), fill=(214, 72, 72, 255))


def plaque(tier):
    main, dark, light = TIERS[tier]
    im = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, SIZE - 1, SIZE - 1), 9, fill=dark + (255,))
    d.rounded_rectangle((2, 2, SIZE - 3, SIZE - 3), 7, fill=main + (255,))
    d.rounded_rectangle((5, 5, SIZE - 6, SIZE - 6), 5, fill=FILL)
    d.line((8, 3, SIZE - 9, 3), fill=light + (255,))     # a highlight along the top
    d.line((3, 8, 3, SIZE - 9), fill=light + (255,))
    return im


# --- the badges --------------------------------------------------------------
def carrot():   return sprite("crops/carrot")
def turnip():   return sprite("crops/turnip")
def lettuce():  return sprite("crops/lettuce")
def pumpkin():  return sprite("crops/pumpkin")
def coin():     return sprite("ui/coin")
def barn():     return sprite("tree/icons/barn")
def farmer():   return sprite("farm/farmer")


def b_first_harvest(im):
    paste(im, carrot(), 32, 33)

def b_green_thumb(im):
    paste(im, carrot(), 21, 30)
    paste(im, turnip(), 43, 30)
    paste(im, lettuce(), 32, 43)

def b_bumper_crop(im):
    paste(im, sprite("crops/corn"), 19, 28)
    paste(im, sprite("crops/eggplant"), 45, 28)
    paste(im, pumpkin(), 32, 42)
    sparkle(im, 12, 12)
    sparkle(im, 52, 14)

def b_pocket_money(im):
    paste(im, coin(), 32, 33)

def b_market_trader(im):
    paste(im, sprite("tree/icons/value"), 32, 33)

def b_vegetable_tycoon(im):
    paste(im, coin(), 32, 38)
    crown(im, 32, 8)

def b_market_day(im):
    paste(im, shrink(sprite("tree/icons/daylength")), 30, 28)
    paste(im, coin(), 40, 42)

def b_harvest_festival(im):
    paste(im, pumpkin(), 32, 36)
    for x, y in ((11, 14), (53, 12), (14, 50), (52, 49)):
        sparkle(im, x, y)

def b_into_the_barn(im):
    paste(im, barn(), 32, 33)

def b_well_equipped(im):
    paste(im, sprite("tree/icons/farmerpick"), 32, 28)
    number(im, "25", 54, 54)

def b_barn_raiser(im):
    paste(im, barn(), 32, 30)
    number(im, "100", 54, 54)

def b_lend_a_hand(im):
    paste(im, shrink(sprite("tree/icons/pickspeed")), 32, 33)

def b_extra_hands(im):
    paste(im, farmer(), 32, 33)

def b_full_crew(im):
    paste(im, shrink(sprite("tree/icons/farmcrew")), 32, 33)
    sparkle(im, 12, 12)
    sparkle(im, 52, 13)

def b_mixed_veg(im):
    paste(im, sprite("crops/broccoli"), 21, 21)
    paste(im, sprite("crops/chilli"), 43, 21)
    paste(im, sprite("crops/corn"), 21, 43)
    paste(im, sprite("crops/eggplant"), 43, 43)

def b_week(im):
    paste(im, shrink(sprite("tree/icons/daylength")), 32, 28)
    number(im, "7", 54, 54)

def b_seasoned(im):
    paste(im, shrink(sprite("tree/icons/daylength")), 32, 28)
    number(im, "50", 54, 54)

def b_barn_full(im):
    paste(im, barn(), 32, 40)
    crown(im, 32, 8)
    sparkle(im, 12, 28)
    sparkle(im, 52, 28)


BADGES = {  # id: (tier, drawing)
    "first_harvest": ("bronze", b_first_harvest),
    "green_thumb": ("silver", b_green_thumb),
    "bumper_crop": ("gold", b_bumper_crop),
    "pocket_money": ("bronze", b_pocket_money),
    "market_trader": ("silver", b_market_trader),
    "vegetable_tycoon": ("gold", b_vegetable_tycoon),
    "market_day": ("silver", b_market_day),
    "harvest_festival": ("gold", b_harvest_festival),
    "into_the_barn": ("bronze", b_into_the_barn),
    "well_equipped": ("silver", b_well_equipped),
    "barn_raiser": ("gold", b_barn_raiser),
    "lend_a_hand": ("silver", b_lend_a_hand),
    "extra_hands": ("bronze", b_extra_hands),
    "full_crew": ("gold", b_full_crew),
    "mixed_veg": ("silver", b_mixed_veg),
    "week_on_the_farm": ("bronze", b_week),
    "seasoned_farmer": ("silver", b_seasoned),
    "the_barn_is_full": ("gold", b_barn_full),
}


def locked(im):
    """Grey and dim, so it's clearly 'not yet'."""
    a = im.getchannel("A")
    g = ImageEnhance.Brightness(im.convert("RGB").convert("L").convert("RGB")).enhance(0.62)
    g = ImageEnhance.Contrast(g).enhance(0.9).convert("RGBA")
    g.putalpha(a)
    return g


def main():
    os.makedirs(OUT, exist_ok=True)
    for ident, (tier, draw) in BADGES.items():
        im = plaque(tier)
        draw(im)
        im.save(os.path.join(OUT, ident + ".png"))
        locked(im).save(os.path.join(OUT, ident + "_locked.png"))
    print(f"Wrote {len(BADGES) * 2} badges to {os.path.relpath(OUT)}")


if __name__ == "__main__":
    main()
