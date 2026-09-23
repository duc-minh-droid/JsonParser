# Generates docs/media/grammar-railroad.svg with a tiny hand-rolled railroad layout.
#   python scripts/gen-railroad.py
import html
import os

CW = 7.8   # char width for 13px monospace
R = 10     # arc radius
GAP = 14   # vertical gap between choice branches

def esc(s): return html.escape(s, quote=True)

class Node:
    pass

class Term(Node):
    def __init__(self, text, nt=False, note=False):
        self.text, self.nt, self.note = text, nt, note
        self.w = len(text) * CW + 22
        self.up = self.down = 13
    def draw(self, x, y):
        cls = "nt" if self.nt else "t"
        rx = 3 if self.nt else 13
        return (f'<rect class="{cls}" x="{x:.1f}" y="{y-13}" width="{self.w:.1f}" height="26" rx="{rx}"/>'
                f'<text class="{cls}x" x="{x+self.w/2:.1f}" y="{y+4.5}" text-anchor="middle">{esc(self.text)}</text>')

def NT(t): return Term(t, nt=True)

class Skip(Node):
    def __init__(self): self.w, self.up, self.down = 0, 0, 0
    def draw(self, x, y): return ""

class Seq(Node):
    def __init__(self, *items, gap=14):
        self.items, self.gap = items, gap
        self.w = sum(i.w for i in items) + gap * (len(items) - 1)
        self.up = max(i.up for i in items)
        self.down = max(i.down for i in items)
    def draw(self, x, y):
        out = []
        for k, it in enumerate(self.items):
            out.append(it.draw(x, y))
            x += it.w
            if k < len(self.items) - 1:
                out.append(f'<path d="M{x:.1f} {y}h{self.gap}"/>')
                x += self.gap
        return "".join(out)

class Choice(Node):
    def __init__(self, *items):
        self.items = items
        self.inner = max(i.w for i in items)
        self.w = self.inner + 4 * R
        self.up = items[0].up
        self.offs = [0]
        off = 0
        for a, b in zip(items, items[1:]):
            off += max(a.down, R) + GAP + max(b.up, R)
            self.offs.append(off)
        self.down = off + items[-1].down if len(items) > 1 else items[0].down
    def draw(self, x, y):
        out = []
        L, Rr = x, x + self.w
        for it, off in zip(self.items, self.offs):
            yi = y + off
            ix = x + 2 * R
            if off == 0:
                out.append(f'<path d="M{L:.1f} {y}h{2*R}"/>')
            else:
                out.append(f'<path d="M{L:.1f} {y}q{R} 0 {R} {R}v{off-2*R}q0 {R} {R} {R}"/>')
            out.append(it.draw(ix, yi))
            ex = ix + it.w
            if off == 0:
                out.append(f'<path d="M{ex:.1f} {y}H{Rr:.1f}"/>')
            else:
                out.append(f'<path d="M{ex:.1f} {yi}H{Rr-2*R:.1f}q{R} 0 {R} {-R}v{-(off-2*R)}q0 {-R} {R} {-R}"/>')
        return "".join(out)

def Opt(item): return Choice(Skip(), item)

class Loop(Node):
    """item one or more times, with optional separator on the return track"""
    def __init__(self, item, sep=None):
        self.item, self.sep = item, sep or Skip()
        self.inner = max(item.w, self.sep.w)
        self.w = self.inner + 4 * R
        self.up = item.up
        self.back = max(item.down, R) + GAP + max(self.sep.up, R)
        self.down = self.back + self.sep.down
    def draw(self, x, y):
        out = []
        ix = x + 2 * R + (self.inner - self.item.w) / 2
        out.append(f'<path d="M{x:.1f} {y}H{ix:.1f}"/>')
        out.append(self.item.draw(ix, y))
        out.append(f'<path d="M{ix+self.item.w:.1f} {y}H{x+self.w:.1f}"/>')
        yb = y + self.back
        right = x + self.w - 2 * R
        left = x + 2 * R
        # return track (right to left) with separator
        sx = x + 2 * R + (self.inner - self.sep.w) / 2
        out.append(f'<path class="back" d="M{right:.1f} {y}q{R} 0 {R} {R}v{self.back-2*R}q0 {R} {-R} {R}H{sx+self.sep.w:.1f}"/>')
        out.append(self.sep.draw(sx, yb))
        out.append(f'<path class="back" d="M{sx:.1f} {yb}H{left:.1f}q{-R} 0 {-R} {-R}v{-(self.back-2*R)}q0 {-R} {R} {-R}"/>')
        # arrow on the return track
        ax = (sx + left) / 2 if self.sep.w else (left + right) / 2
        out.append(f'<path class="arrow" d="M{ax+4:.1f} {yb-4}l-6 4 6 4"/>')
        return "".join(out)

rules = [
    ("value", "parseValue()", Choice(NT("object"), NT("array"), NT("string"), NT("number"), Term("true"), Term("false"), Term("null"))),
    ("object", "parseObject()", Seq(Term("{"), Choice(Term("}"), Seq(Loop(NT("member"), Term(",")), Term("}"))))),
    ("member", "parseObject() loop", Seq(NT("string"), Term(":"), NT("value"))),
    ("array", "parseArray()", Seq(Term("["), Choice(Term("]"), Seq(Loop(NT("value"), Term(",")), Term("]"))))),
    ("string", "parseStringLiteral()", Seq(Term('"'), Opt(Loop(Choice(NT("any char except \" \\ or U+0000-001F"),
                                                              Seq(Term("\\"), Choice(NT('one of " \\ / b f n r t'), Seq(Term("u"), NT("4 hex digits"))))))), Term('"'))),
    ("number", "parseNumber()", Seq(Opt(Term("-")), Choice(Term("0"), Seq(NT("1-9"), Opt(Loop(NT("0-9"))))),
                                     Opt(Seq(Term("."), Loop(NT("0-9")))),
                                     Opt(Seq(Choice(Term("e"), Term("E")), Opt(Choice(Term("+"), Term("-"))), Loop(NT("0-9")))), gap=10)),
]

LABEL_W = 150
X0 = 24
parts = []
maxw = 0

def place(rule, x0, y):
    global maxw
    name, fn, d = rule
    base = y + d.up + 26
    parts.append(f'<text class="rule" x="{x0}" y="{base-2}">{name}</text>')
    parts.append(f'<text class="fn" x="{x0}" y="{base+15}">{esc(fn)}</text>')
    x = x0 + LABEL_W
    parts.append(f'<path class="end" d="M{x} {base-8}v16M{x+4} {base-8}v16"/>')
    parts.append(f'<path d="M{x+4} {base}h16"/>')
    parts.append(d.draw(x + 20, base))
    ex = x + 20 + d.w
    parts.append(f'<path d="M{ex:.1f} {base}h16"/>')
    parts.append(f'<path class="end" d="M{ex+16:.1f} {base-8}v16M{ex+20:.1f} {base-8}v16"/>')
    maxw = max(maxw, ex + 20)
    return base + max(d.down, 20) + 30

def sep(y):
    parts.append(f'<line class="sep" x1="{X0}" x2="__W__" y1="{y-14}" y2="{y-14}"/>')

byname = {r[0]: r for r in rules}
top = 40
yl = place(byname["value"], X0, top)
RX = 470
yr = place(byname["object"], RX, top)
yr = place(byname["member"], RX, yr)
yr = place(byname["array"], RX, yr)
y = max(yl, yr)
sep(y)
y = place(byname["string"], X0, y)
sep(y)
y = place(byname["number"], X0, y)

W = int(maxw + 30)
H = int(y)
body = "".join(parts).replace("__W__", str(W - 24))

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Consolas, 'JetBrains Mono', ui-monospace, monospace">
<style>
  .bg {{ fill: #ffffff; }}
  path {{ fill: none; stroke: #57606a; stroke-width: 1.6; }}
  path.back {{ stroke: #8c959f; }}
  path.arrow {{ stroke: #8c959f; }}
  path.end {{ stroke: #24292f; stroke-width: 2; }}
  rect.t {{ fill: #fff4d6; stroke: #b7791f; stroke-width: 1.4; }}
  rect.nt {{ fill: #e6efff; stroke: #3f6fd8; stroke-width: 1.4; }}
  text {{ font-size: 13px; }}
  .tx {{ fill: #7a4b00; font-weight: 700; }}
  .ntx {{ fill: #1f3f8a; }}
  .rule {{ fill: #24292f; font-size: 17px; font-weight: 700; }}
  .fn {{ fill: #6e7781; font-size: 12px; }}
  .title {{ fill: #24292f; font-size: 15px; font-weight: 700; font-family: system-ui, sans-serif; }}
  .sub {{ fill: #6e7781; font-size: 12px; font-family: system-ui, sans-serif; }}
  line.sep {{ stroke: #d0d7de; stroke-dasharray: 3 5; }}
  @media (prefers-color-scheme: dark) {{
    .bg {{ fill: #0d1117; }}
    path {{ stroke: #8b949e; }}
    path.back, path.arrow {{ stroke: #6e7681; }}
    path.end {{ stroke: #c9d1d9; }}
    rect.t {{ fill: #3a2f14; stroke: #e0af68; }}
    rect.nt {{ fill: #17233b; stroke: #7aa2f7; }}
    .tx {{ fill: #f5c77e; }}
    .ntx {{ fill: #b4c9ff; }}
    .rule, .title {{ fill: #e6edf3; }}
    .fn, .sub {{ fill: #8b949e; }}
    line.sep {{ stroke: #30363d; }}
  }}
</style>
<rect class="bg" width="{W}" height="{H}" rx="10"/>
<text class="title" x="{X0}" y="26">JSON grammar as parsed by JsonParser.h</text>
<text class="sub" x="{W-24}" y="26" text-anchor="end">rounded = literal token, square = rule / character class; whitespace allowed between tokens</text>
{body}
</svg>
'''
open(os.path.join(os.path.dirname(__file__), "..", "docs", "media", "grammar-railroad.svg"), "w", encoding="utf-8", newline="").write(svg)
print(W, H)
