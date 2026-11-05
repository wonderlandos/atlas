#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Static checks of the Atlas new tab page (no browser needed).

usage: python3 -I tools/check.py
  1. index.html parses, tags balance, ids are unique, every id reference resolves,
     every button and input has a name, the document has lang and title.
  2. No network: no URL scheme, protocol-relative URL, @import, url() to a file,
     fetch/XHR/WebSocket/beacon/EventSource in the page files.
  3. No colour literal outside theme.css and the brand badge; type sizes on the HIG scale;
     durations only from the motion tokens.
  4. No upstream project or other browser named in the page or its code.
  5. Contrast of the token pairs the page uses, on both built-in themes (from theme.css).
"""
import html.parser
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PAGE = ["index.html", "newtab.css", "newtab.js", "host.js"]
GENERATED = ["theme.css", "categories.css"]
ENGINE = ["host.engine.js"]
VOID = {"meta", "link", "br", "img", "input", "hr", "source", "wbr", "col", "area", "base", "embed", "track"}
SVG_VOID = {"path", "circle", "rect", "stop", "use"}
errors = []


class Page(html.parser.HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.stack, self.ids, self.refs, self.buttons, self.inputs = [], {}, [], [], []
        self.lang = self.title = False
        self.in_title = self.cur_button = None
        self.label_depth = 0

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "html" and a.get("lang"):
            self.lang = True
        if "id" in a:
            if a["id"] in self.ids:
                errors.append(f"duplicate id {a['id']} (line {self.getpos()[0]})")
            self.ids[a["id"]] = tag
        for k in ("aria-labelledby", "aria-describedby", "aria-controls", "for"):
            if k in a:
                self.refs += [(k, r, self.getpos()[0]) for r in a[k].split()]
        if tag == "use":
            href = a.get("href", "")
            if not href.startswith("#"):
                errors.append(f"<use> outside the document: {href}")
            self.refs.append(("use", href[1:], self.getpos()[0]))
        if tag == "label":
            self.label_depth += 1
        if tag == "input":
            self.inputs.append((a, self.label_depth > 0, self.getpos()[0]))
        if tag == "button":
            self.cur_button = {"attrs": a, "text": "", "line": self.getpos()[0]}
        if tag == "title":
            self.in_title = True
        if tag in VOID or tag in SVG_VOID:
            return
        self.stack.append((tag, self.getpos()[0]))

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in VOID and tag not in SVG_VOID:
            self.stack.pop()

    def handle_endtag(self, tag):
        if tag in VOID or tag in SVG_VOID:
            return
        if not self.stack or self.stack[-1][0] != tag:
            errors.append(f"unbalanced </{tag}> at line {self.getpos()[0]} (open: {self.stack[-1] if self.stack else None})")
            return
        self.stack.pop()
        if tag == "label":
            self.label_depth -= 1
        if tag == "button" and self.cur_button:
            self.buttons.append(self.cur_button)
            self.cur_button = None
        if tag == "title":
            self.in_title = False

    def handle_data(self, data):
        if self.cur_button is not None:
            self.cur_button["text"] += data
        if self.in_title and data.strip():
            self.title = True


def check_html():
    src = (ROOT / "index.html").read_text()
    if not src.lower().startswith("<!doctype html>"):
        errors.append("index.html: no <!doctype html>")
    p = Page()
    p.feed(src)
    p.close()
    if p.stack:
        errors.append(f"unclosed tags: {p.stack}")
    for kind, ref, line in p.refs:
        if ref not in p.ids:
            errors.append(f"{kind} -> #{ref} does not exist (line {line})")
    for b in p.buttons:
        a = b["attrs"]
        if not (b["text"].strip() or a.get("aria-label") or a.get("aria-labelledby")):
            errors.append(f"button without a name (line {b['line']})")
        if a.get("type") != "button":
            errors.append(f"button without type=button (line {b['line']})")
    for a, labelled, line in p.inputs:
        if not (labelled or a.get("aria-label") or a.get("aria-labelledby")):
            errors.append(f"input without a label (line {line})")
    if not p.lang:
        errors.append("html has no lang")
    if not p.title:
        errors.append("no <title>")
    if re.search(r"\son[a-z]+=", src):
        errors.append("inline event handler (blocked by the page's CSP)")
    if re.search(r'\sstyle="', src):
        errors.append("style attribute (blocked by style-src 'self')")
    if re.search(r"<script>(?!\s*</script>)", src):
        errors.append("inline script (blocked by script-src 'self')")
    print(f"html: {len(p.ids)} ids, {len(p.refs)} references, {len(p.buttons)} buttons, {len(p.inputs)} inputs")



def check_structure():
    html_text = (ROOT / "index.html").read_text()
    css = (ROOT / "newtab.css").read_text()
    js = (ROOT / "newtab.js").read_text()
    if "innerHTML" in js or "outerHTML" in js or "insertAdjacentHTML" in js or "document.write" in js:
        errors.append("newtab.js writes markup (Trusted Types forbids it)")
    if len(re.findall(r"<h1\b", html_text)) != 1:
        errors.append("the page needs exactly one h1")
    if "role=\"search\"" not in html_text:
        errors.append("the search field is not inside role=search")
    for needle, what in (
        ("width: min(560px, 100%)", "search field 560 wide"),
        ("height: 40px", "search field 40 high"),
        ("width: 96px; height: 88px", "pinned cards 96 x 88"),
        ("height: 64px", "recent cards 64 high"),
        ("minmax(0, 320px)", "recent cards 320 wide"),
        ("30ms", "30 ms stagger"),
    ):
        if needle not in css:
            errors.append(f"newtab.css lacks {what} ({needle})")
    # DNT wording: one string, the same as the window strip (IDS_ATLAS_DNT_IS_ON)
    if ">Do Not Track is on<" not in html_text:
        errors.append("the status line text is not 'Do Not Track is on'")
    # every key the page reads through data-s has an English default in the markup
    keys = set(re.findall(r'data-s="([a-zA-Z]+)"', html_text))
    # the contract fixture carries every key the page reads
    import json
    fx = json.loads((ROOT / "testdata/state-filled.json").read_text())
    for key in ("pinned", "recent", "doNotTrack", "statusStrip", "firstInWindow", "strings"):
        if key not in fx:
            errors.append(f"testdata/state-filled.json lacks {key}")
        if key != "strings" and f"state.{key}" not in js:
            errors.append(f"newtab.js does not read state.{key}")
    cats = {p["category"] for p in fx["pinned"]}
    if cats != {"web", "talk", "files", "media", "store", "system"}:
        errors.append(f"testdata/state-filled.json covers {sorted(cats)}")
    print(f"structure: one h1, role=search, {len(keys)} string keys in the markup, no markup written by the script")


def without_mark(text):
    return re.sub(r"<!-- mark:begin -->.*?<!-- mark:end -->", "", text, flags=re.S)


def check_network():
    pats = [
        (r"\b[a-z][a-z0-9+.-]*://", "URL with a scheme"),
        (r"""(?:src|href)\s*=\s*["']//""", "protocol-relative URL"),
        (r"@import", "@import"),
        (r"url\(\s*['\"]?(?!#)", "url() to a file"),
        (r"\bfetch\s*\(|XMLHttpRequest|WebSocket|sendBeacon|EventSource|importScripts|\bimport\s*\(", "network API"),
    ]
    for f in PAGE + ENGINE + GENERATED:
        text = without_mark((ROOT / f).read_text())
        for rx, what in pats:
            for m in re.finditer(rx, text):
                line = text[: m.start()].count("\n") + 1
                # the preview host carries sample pages on the reserved ".example" names
                if f == "host.js" and what == "URL with a scheme" and re.match(r"https://[a-z]+\.example/", text[m.start():]):
                    continue
                errors.append(f"{f}:{line}: {what}: {text[m.start():m.start() + 40]!r}")
    # the brand badge: only same-document fragment references
    mark = re.search(r"<!-- mark:begin -->(.*?)<!-- mark:end -->", (ROOT / "index.html").read_text(), re.S).group(1)
    if re.search(r"://|url\((?!#)", mark):
        errors.append("the inlined badge references something outside the document")
    links = re.findall(r'(?:src|href)="([^"]+)"', without_mark((ROOT / "index.html").read_text()))
    bad = [h for h in links if not (h.startswith("#") or h == "data:," or re.fullmatch(r"[a-z]+\.(css|js)", h))]
    if bad:
        errors.append(f"index.html links outside the page files: {bad}")
    print(f"network: page files reference only {sorted(set(links))}")


def check_literals():
    for f in ["newtab.css", "newtab.js", "host.js", "host.engine.js", "index.html"]:
        text = without_mark((ROOT / f).read_text())
        for m in re.finditer(r"#[0-9a-fA-F]{3,8}\b(?![-\w])|\brgba?\(|\bhsla?\(|\boklch\(|\blab\(", text):
            # '#id' fragment references in index.html are not colours
            if f == "index.html" and re.match(r"#[0-9a-fA-F]{3,8}\"", text[m.start():m.start() + 10]):
                continue
            if f == "index.html" and text[m.start() - 6:m.start()].endswith('href="'):
                continue
            line = text[: m.start()].count("\n") + 1
            errors.append(f"{f}:{line}: colour literal {m.group(0)!r}")
    css = (ROOT / "newtab.css").read_text()
    sizes = set(re.findall(r"font-size:\s*(\d+)px", css)) | set(re.findall(r"font:\s*\d+\s+(\d+)px", css))
    off = sizes - {"11", "13", "15", "22", "26"}
    if off:
        errors.append(f"type sizes off the HIG scale: {sorted(off)}")
    durs = re.findall(r"\b\d+(?:\.\d+)?m?s\b", re.sub(r"calc\(var\(--i, 0\) \* 30ms\)", "", css))
    if durs:
        errors.append(f"durations not from the motion tokens: {durs}")
    print(f"literals: type sizes {sorted(sizes, key=int)}; durations from --motion-* only; stagger 30 ms (motion.md)")


def check_names():
    banned = r"\b(helium|chromium|chrome|google|ungoogled|brave|firefox|mozilla|safari|edge|opera|vivaldi|imput)\b"
    for f in PAGE + ENGINE + GENERATED:
        text = (ROOT / f).read_text()
        if f == "host.engine.js":
            # the engine's WebUI channel is a global of that name: the one identifier that has to stay
            text = text.replace("chrome.send(", "").replace("window.chrome", "")
        for m in re.finditer(banned, text, re.I):
            line = text[: m.start()].count("\n") + 1
            errors.append(f"{f}:{line}: names {m.group(0)!r}")
    print("names: no upstream project or other browser named in the page files")


# contrast on the theme tokens
def rgba(v):
    v = v.strip()
    if v.startswith("#"):
        h = v[1:]
        return [int(h[i:i + 2], 16) for i in (0, 2, 4)] + [1.0]
    n = [float(x) for x in re.findall(r"[\d.]+", v)]
    return n[:3] + [n[3] if len(n) > 3 else 1.0]


def over(top, bot):
    a = top[3]
    return [top[i] * a + bot[i] * (1 - a) for i in range(3)] + [1.0]


def mix(a, b, pa):
    return [a[i] * pa + b[i] * (1 - pa) for i in range(3)] + [1.0]


def lum(c):
    def f(v):
        v /= 255
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])


def ratio(a, b):
    x, y = lum(a), lum(b)
    return (max(x, y) + 0.05) / (min(x, y) + 0.05)


def check_contrast():
    css = (ROOT / "theme.css").read_text()
    cats = {k: rgba(v) for k, v in re.findall(r"--cat-([a-z]+): (#[0-9A-Fa-f]{6});", (ROOT / "categories.css").read_text())}
    light_block, dark_block = css.split("@media (prefers-color-scheme: dark)")
    for name, block in (("light", light_block), ("dark", dark_block)):
        t = {k: rgba(v) for k, v in re.findall(r"--([a-z-]+): (#[0-9a-f]{6}|rgba\([^)]*\));", block)}
        text_pairs = {
            "text on surface (title, headings)": (t["text"], t["surface"]),
            "dim on surface (status line)": (t["dim"], t["surface"]),
            "text on card (recent title, field)": (t["text"], t["card"]),
            "dim on card (labels, placeholder, when)": (t["dim"], t["card"]),
            "dim on surface (empty hint)": (t["dim"], t["surface"]),
            "text on control (empty action)": (t["text"], t["control"]),
        }
        ui_pairs = {
            "accent glyph on card (page glyph)": (t["accent"], t["card"]),
            "accent focus ring on surface": (t["accent"], t["surface"]),
            "dim search glyph on card": (t["dim"], t["card"]),
        }
        out = []
        for k, (fg, bg) in text_pairs.items():
            r = ratio(over(fg, bg), bg)
            out.append(f"{k} {r:.2f}")
            if r < 4.5:
                errors.append(f"{name}: {k} is {r:.2f}:1 (< 4.5)")
        for k, (fg, bg) in ui_pairs.items():
            r = ratio(over(fg, bg), bg)
            out.append(f"{k} {r:.2f}")
            if r < 3:
                errors.append(f"{name}: {k} is {r:.2f}:1 (< 3)")
        print(f"contrast {name}: " + "; ".join(out))
        # The white glyph on a category tile is decoration (the label names the card and the
        # glyph is aria-hidden); the ratios are listed so the review sees them.
        tile = ", ".join(f"{c} {ratio(t['accent-text'], v):.2f}" for c, v in cats.items())
        print(f"  tile glyph (accentText on the category colour, decorative): {tile}")


check_html()
check_structure()
check_network()
check_literals()
check_names()
check_contrast()
if errors:
    print("FAIL")
    for e in errors:
        print(" -", e)
    sys.exit(1)
print("PASS")
