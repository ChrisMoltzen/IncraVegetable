#!/usr/bin/env python3
"""Keeps the language files in assets/lang/ up to date with the game's text.

    python3 tools/lang/update.py

It finds every piece of English text the game shows (tr("..."), trf("..."),
trc("..."), N_("...") in src/ and include/, plus the tech tree's names and
descriptions and the crop names from include/TechTreeData.h), then:

  * writes assets/lang/template.txt: every piece of text, for starting a new
    language (copy it to assets/lang/<code>.txt and fill in @name and @locale);
  * rewrites each assets/lang/<code>.txt in the same order, keeping its
    translations. Text it hasn't translated yet is added as a "# TODO" line:
    take out the "# TODO " and type the translation after the " = ".
    Translations of text the game no longer has go to the end, commented out.

Run it after changing any text in the game. Needs only Python 3.
"""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LANG_DIR = os.path.join(ROOT, "assets", "lang")
SKIP = {"DebugMenu.cpp", "ArtCatalog.cpp"}  # developer-only text stays English

CALL = re.compile(r'\b(?:tr|trc|trf|N_)\(\s*"((?:[^"\\]|\\.)*)"\s*[,)]')


def c_unescape(s):
    """C string literal contents -> the language file's form (only \\n stays escaped)."""
    out, i = [], 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            out.append({"n": "\\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}.get(nxt, nxt))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def strings_in_code():
    found = []  # (source, text)
    for folder in ("src", "include"):
        for name in sorted(os.listdir(os.path.join(ROOT, folder))):
            if not name.endswith((".cpp", ".h", ".mm")) or name in SKIP or name == "TechTreeData.h":
                continue
            with open(os.path.join(ROOT, folder, name), encoding="utf-8") as f:
                code = f.read()
            # Leave out comments, so examples in them aren't picked up.
            code = re.sub(r"//[^\n]*", "", code)
            for m in CALL.finditer(code):
                found.append((folder + "/" + name, c_unescape(m.group(1))))
    return found


def strings_in_tree():
    path = os.path.join(ROOT, "include", "TechTreeData.h")
    with open(path, encoding="utf-8") as f:
        text = f.read()
    # The tree is in raw string literals: R"TECHTREE(...)TECHTREE".
    body = "".join(m[1] for m in re.findall(r'R"(\w*)\((.*?)\)\1"', text, re.S)) or text
    found = []
    for line in body.splitlines():
        line = line.strip()
        for key in ("name ", "desc "):
            if line.startswith(key):
                value = line[len(key):].strip()
                if key == "name ":
                    value = re.sub(r" \d+$", "", value)  # "Bigger Patch 12" -> "Bigger Patch"
                if value:
                    found.append(("tech tree", value))
    return found


def ordered_unique(pairs):
    seen, out = set(), []
    for src, text in pairs:
        if text and text not in seen:
            seen.add(text)
            out.append((src, text))
    return out


def read_language(path):
    header, table = [], {}
    with open(path, encoding="utf-8-sig") as f:
        lines = f.read().splitlines()
    in_header = True
    for line in lines:
        t = line.strip()
        if in_header and (t.startswith("@") or (t.startswith("#") and "TODO" not in t and " = " not in t) or not t):
            if t.startswith("# ---- ") or t.startswith("# Not in the game any more"):
                in_header = False
                continue
            header.append(line)
            continue
        in_header = False
        if t.startswith("#") or " = " not in t:
            continue
        en, tx = t.split(" = ", 1)
        if en.strip() and tx.strip():
            table[en.strip()] = tx.strip()
    while header and not header[-1].strip():
        header.pop()
    return header, table


def write_language(path, header, entries, table):
    out = list(header) + [""]
    last_src = None
    done = todo = 0
    for src, en in entries:
        if src != last_src:
            out.append("")
            out.append("# ---- " + src)
            last_src = src
        if en in table:
            out.append(en + " = " + table[en])
            done += 1
        else:
            out.append("# TODO " + en + " = ")
            todo += 1
    old = [en for en in table if en not in {e for _, e in entries}]
    if old:
        out.append("")
        out.append("# Not in the game any more (kept in case the text comes back):")
        for en in old:
            out.append("# " + en + " = " + table[en])
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out).rstrip() + "\n")
    return done, todo


def main():
    entries = ordered_unique(strings_in_code() + strings_in_tree())
    bad = [e for _, e in entries if " = " in e]
    for e in bad:
        print("warning: text contains ' = ', which the language files use as a separator:", e)
    os.makedirs(LANG_DIR, exist_ok=True)
    template_header = [
        "# IncraVegetable - language template. Every piece of text in the game, in English.",
        "# To add a language: copy this to <code>.txt (e.g. it.txt), set @name (the language's",
        "# own name, shown in Settings) and @locale (e.g. it, or pt-BR), then translate each",
        "# line: take out the \"# TODO \" and type the translation after the \" = \".",
        "# Keep every %d, %s, %.1f ... in the same order. See include/Lang.h.",
        "@name New Language",
        "@locale xx",
    ]
    write_language(os.path.join(LANG_DIR, "template.txt"), template_header, entries, {})
    print("template.txt: %d pieces of text" % len(entries))
    for name in sorted(os.listdir(LANG_DIR)):
        if not name.endswith(".txt") or name == "template.txt":
            continue
        path = os.path.join(LANG_DIR, name)
        header, table = read_language(path)
        done, todo = write_language(path, header, entries, table)
        print("%-8s %4d translated, %4d to do" % (name, done, todo))
    return 0


if __name__ == "__main__":
    sys.exit(main())
