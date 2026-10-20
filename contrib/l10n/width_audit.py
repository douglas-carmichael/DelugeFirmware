#!/usr/bin/env python3
"""Pixel-exact OLED width audit for translated l10n strings.

Replicates the Canvas glyph metrics (getCharIndex folding, per-glyph advance) for the
two title contexts that draw without clipping or scrolling, plus popup word-wrap limits.
Screen titles are measured like Canvas::getStringWidthInPixels(): pair kerning (read from
kKerningRules in canvas.cpp, so it follows upstream changes) and the 1px offset drawChar()
gives each glyph by centring it in its draw box.
Run from anywhere: python3 contrib/l10n/width_audit.py french.json german.json ...
Exits non-zero if any string breaks a budget English respects.
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
L10N = REPO / "src/deluge/gui/l10n"

CTX_TITLE_IDS = {
    "STRING_FOR_AUDIO_SOURCE",
    "STRING_FOR_CLEAR_SONG_QMARK",
    "STRING_FOR_CONFIGURE_SONG_MACROS_SHORT",
    "STRING_FOR_ARE_YOU_SURE_QMARK",
    "STRING_FOR_DELETE_QMARK",
    "STRING_FOR_LOAD_PRESET",
    "STRING_FOR_MIDI_LEARN",
    "STRING_FOR_OVERWRITE_QMARK",
    "STRING_FOR_SAMPLES",
    "STRING_FOR_LOAD_FILES",
    "STRING_FOR_OPTIONS",
    "STRING_FOR_STOP_EXPORT_STEMS_QMARK",
    "STRING_FOR_DONE_EXPORT_STEMS",
    "STRING_FOR_CLIP_MODE",
    "STRING_FOR_DEFAULT_NEW_CLIP_TYPE",
    "STRING_FOR_TITLE_CLIP_SETTINGS",
}
CTX_TITLE_BUDGET = 114 - 22  # ContextMenu title: x=22, dialog border at x=114
SCREEN_TITLE_BUDGET = 128  # drawScreenTitle: x=0, clipped at the screen edge
POPUP_MAX_WORD = 19  # breakStringIntoLines maxCharsPerLine


def parse_font_desc(name):
    src = (REPO / "src/deluge/gui/fonts/fonts.c").read_text()
    match = re.search(rf"{name}\[\]\s*=\s*\{{(.*?)\}};", src, re.DOTALL)
    return [int(w) for w in re.findall(r"\.w_px\s*=\s*(\d+)", match.group(1))]


FONT9 = parse_font_desc("font_metric_bold_9px_desc")
TITLE_TEXT_HEIGHT = 10  # kTextTitleSizeY, which Canvas draws in the bold 9px font


def parse_kerning_rules():
    src = (REPO / "src/deluge/hid/display/oled_canvas/canvas.cpp").read_text()
    rules = {}
    for height, prev, cur, adjustment in re.findall(
        r"\.text_height = (\d+), \.previous_char = '(\\.|[^'])', "
        r"\.current_char = '(\\.|[^'])', \.adjustment = (-?\d+)",
        src,
    ):
        rules[(int(height), prev[-1], cur[-1])] = int(adjustment)
    # A table this can't read would silently measure every title without kerning
    if not rules or len(rules) != src.count("{.text_height ="):
        sys.exit("width_audit: can't parse kKerningRules in canvas.cpp")
    return rules


KERNING = parse_kerning_rules()


def kerning(prev, cur):
    # getPreviousCharSpacingAdjustmentInPixels() at the title text height
    if prev == " " or cur == " ":
        return 0
    if prev == "-" or cur == "-":
        return 1
    return KERNING.get((TITLE_TEXT_HEIGHT, prev.upper(), cur.upper()), 0)


def char_index(c):
    o = ord(c)
    if o == 129:
        o = ord("~") + 1
    elif o > 0x7E:
        return 0
    if o >= ord("a"):
        o -= 32 if o <= ord("z") else 26
    return o - 0x20


def width_bold9(s):
    # Each glyph is drawn centred in its draw box, 1px right of the cursor, so the last
    # glyph ends 1px past the cursor advance
    advance = glyph_end = 0
    for i, c in enumerate(s):
        idx = char_index(c)
        w = 0 if idx <= 0 or idx >= len(FONT9) else FONT9[idx]
        draw_spacing = 5 if c == " " else 2
        if i:
            advance += kerning(s[i - 1], c)
        if w:
            glyph_end = max(glyph_end, advance + (draw_spacing >> 1) + w)
        advance += w + (0 if i == len(s) - 1 else draw_spacing)
    return max(advance, glyph_end)


def width_mono(s):
    return sum(0 if (c == " " and i == len(s) - 1) else 6 for i, c in enumerate(s))


def screen_title_ids(english_keys):
    ids = {
        k
        for k in english_keys
        if k.endswith("_MENU_TITLE") or k.startswith("STRING_FOR_TITLE_")
    }
    menus = (REPO / "src/deluge/gui/ui/menus.cpp").read_text()
    for m in re.finditer(
        r"^[\w:<>&\s]+?\s\w+\{\s*(STRING_FOR_\w+)\s*(?:,\s*(STRING_FOR_\w+))?",
        menus,
        re.MULTILINE,
    ):
        ids.add(m.group(2) or m.group(1))
    return ids


def audit(lang_file, english, titles):
    strings = json.loads((L10N / lang_file).read_text())["strings"]
    flags = []
    for k, v in strings.items():
        if not v or re.search(r"[^\x20-\x7e]", v) or '"' in v or "\\" in v:
            flags.append(("CHARSET", k, v))
        if k in CTX_TITLE_IDS and width_mono(v) > CTX_TITLE_BUDGET:
            flags.append(("CTX-TITLE", k, v, width_mono(v)))
        if k in titles and k in english:
            w, w_en = width_bold9(v), width_bold9(english[k])
            if w > SCREEN_TITLE_BUDGET and w > w_en:
                flags.append(("SCREEN-TITLE", k, v, w))
        if any(len(word) > POPUP_MAX_WORD for word in v.split()):
            flags.append(("LONG-WORD", k, v))
        if "*" in english.get(k, "") and "*" not in v:
            flags.append(("PLACEHOLDER", k, v))
    return flags


def main():
    english = json.loads((L10N / "english.json").read_text())["strings"]
    titles = screen_title_ids(english.keys())
    total = 0
    for lang_file in sys.argv[1:] or ["french.json", "spanish.json", "german.json"]:
        flags = audit(lang_file, english, titles)
        total += len(flags)
        print(f"{lang_file}: {len(flags)} flags")
        for flag in flags:
            print("   ", flag)
    sys.exit(1 if total else 0)


if __name__ == "__main__":
    main()
