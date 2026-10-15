#!/usr/bin/env python3
"""Generates the Pastel Drum Sampler UI mockup SVG (1280x800, matches drumsynth palette)."""
import math, random, sys

W, H = 1280, 800
BG, PANEL, BTN, BTN_EDGE = "#101014", "#17171d", "#232330", "#34343f"
TXT, LAB, DIM = "#e8e8ee", "#9a9aa8", "#6a6a78"
ACCENT, GREEN, AMBER = "#f49ac1", "#a3e635", "#fbbf24"

COLS = ["#f49ac1", "#f87171", "#fda4af", "#fb923c", "#fbbf24", "#fde047", "#a3e635", "#34d399",
        "#2dd4bf", "#22d3ee", "#38bdf8", "#60a5fa", "#a78bfa", "#c4b5fd", "#f0abfc", "#f9a8d4"]
# generic slots: label = user-editable name (defaults to first layer's file name), or "PAD n" when empty
SLOTS = [  # (name, kind for fake waveform, layers loaded, layer mode)
    ("Kick_808_03", "kick", 1, ""), ("Snare_Tight_04", "snare", 3, "RR"), ("Clap_Stack", "snare", 4, "STACK"), ("Rim_Dry", "hat", 1, ""),
    ("Hat_Closed_02", "hat", 4, "RR"), ("Hat_Pedal", "hat", 1, ""), ("Hat_Open_Long", "cym", 2, "RR"), ("Ride_Bell", "cym", 1, ""),
    ("Crash_Wash", "cym", 2, "STACK"), ("Tom_Hi", "tom", 1, ""), ("Tom_Mid", "tom", 1, ""), ("Tom_Floor", "tom", 1, ""),
    ("Conga_Slap", "snare", 2, "RR"), (None, None, 0, ""), ("Vox_Chop", "cym", 1, ""), ("Riser_FX", "cym", 1, ""),
]
NOTES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
def note(n): return f"{NOTES[n % 12]}{n // 12 - 2}"   # 36 -> C1 (GM drum convention)
SEL = 1          # pad 2 selected
HIT = 4          # pad 5 currently flashing (being hit)
out = []
def a(s): out.append(s)

def esc(s): return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def text(x, y, s, size=11, fill=LAB, weight="normal", anchor="start", extra=""):
    a(f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill}" font-weight="{weight}" text-anchor="{anchor}" {extra}>{esc(s)}</text>')

def rect(x, y, w, h, fill, rx=5, stroke=None, sw=1, extra=""):
    st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
    a(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}"{st} {extra}/>')

def button(x, y, w, h, label, on=False, col=ACCENT, size=13, rx=4):
    rect(x, y, w, h, col if on else BTN, rx, None if on else "#2a2a36")
    text(x + w / 2, y + h / 2 + size * 0.36, label, size, "#101014" if on else TXT, anchor="middle")

def combo(x, y, w, h, label, size=13):
    rect(x, y, w, h, "#1c1c22", 3, BTN_EDGE)
    text(x + 7, y + h / 2 + size * 0.36, label, size, TXT)
    a(f'<path d="M{x+w-14},{y+h/2-2} l4,4 l4,-4" stroke="{LAB}" stroke-width="1.4" fill="none"/>')

def arc(cx, cy, r, a0, a1):
    p0 = (cx + r * math.sin(a0), cy - r * math.cos(a0))
    p1 = (cx + r * math.sin(a1), cy - r * math.cos(a1))
    large = 1 if abs(a1 - a0) > math.pi else 0
    sweep = 1 if a1 > a0 else 0
    return f"M{p0[0]:.2f},{p0[1]:.2f} A{r},{r} 0 {large} {sweep} {p1[0]:.2f},{p1[1]:.2f}"

START, END = -0.75 * math.pi, 0.75 * math.pi
def knob(cx, cy, d, v, col, label=None, value=None, bipolar=False, lock=False, lsize=10):
    r = d / 2
    ang = START + v * (END - START)
    a(f'<circle cx="{cx}" cy="{cy}" r="{r-3}" fill="#1e1e26" stroke="#2c2c38" stroke-width="1.5"/>')
    a(f'<path d="{arc(cx, cy, r-1, START, END)}" stroke="#33333f" stroke-width="{max(2.5, d*0.05):.1f}" fill="none" stroke-linecap="round"/>')
    a0 = 0 if bipolar else START
    if abs(ang - a0) > 0.01:
        lo, hi = sorted((a0, ang))
        a(f'<path d="{arc(cx, cy, r-1, lo, hi)}" stroke="{col}" stroke-width="{max(2.5, d*0.05):.1f}" fill="none" stroke-linecap="round"/>')
    px, py = cx + (r * 0.62) * math.sin(ang), cy - (r * 0.62) * math.cos(ang)
    ix, iy = cx + (r * 0.12) * math.sin(ang), cy - (r * 0.12) * math.cos(ang)
    a(f'<line x1="{ix:.1f}" y1="{iy:.1f}" x2="{px:.1f}" y2="{py:.1f}" stroke="white" stroke-opacity="0.92" stroke-width="{max(1.6, d*0.03):.1f}" stroke-linecap="round"/>')
    if label:
        text(cx, cy + r + 17, label, lsize, LAB, "bold", "middle")
    if value:
        text(cx, cy + r + 31, value, lsize, col, anchor="middle", extra='fill-opacity="0.95"')
    if lock:
        padlock(cx + r + 4, cy - r - 4)

def padlock(x, y):
    a(f'<rect x="{x-5}" y="{y-1}" width="10" height="8" rx="1.5" fill="{AMBER}"/>')
    a(f'<path d="M{x-3},{y-1} v-2.5 a3,3 0 0 1 6,0 v2.5" stroke="{AMBER}" stroke-width="1.6" fill="none"/>')

def wave_points(seed, n, kind):
    rnd = random.Random(seed)
    pts = []
    for i in range(n):
        t = i / n
        if kind == "kick":   env = math.exp(-t * 4.2); body = 0.95
        elif kind == "hat":  env = math.exp(-t * 14); body = 0.8
        elif kind == "cym":  env = math.exp(-t * 2.2); body = 0.7
        elif kind == "tom":  env = math.exp(-t * 5); body = 0.9
        else:                env = math.exp(-t * 7); body = 0.9
        atk = min(1, t * n / 3)
        jitter = 0.65 + 0.35 * rnd.random()
        pts.append(body * env * atk * jitter)
    return pts

def kind_of(name):
    if name in ("KICK",): return "kick"
    if name in ("CHH", "PHH", "SHAKER", "RIM"): return "hat"
    if name in ("OHH", "RIDE", "CRASH", "SFX"): return "cym"
    if "TOM" in name: return "tom"
    return "snare"

def mirrored_wave(x, y, w, h, pts, col, opacity=0.55):
    mid = y + h / 2
    top = [(x + i * w / (len(pts) - 1), mid - p * h / 2) for i, p in enumerate(pts)]
    bot = [(px, 2 * mid - py) for px, py in reversed(top)]
    d = "M" + " L".join(f"{px:.1f},{py:.1f}" for px, py in top + bot) + " Z"
    a(f'<path d="{d}" fill="{col}" fill-opacity="{opacity}"/>')

# ---------------------------------------------------------------- document
a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
  'font-family="Inter, \'DejaVu Sans\', \'Segoe UI\', sans-serif">')
a('<defs>'
  '<filter id="glow" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="6"/></filter>'
  '<linearGradient id="padHit" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fbbf24" stop-opacity="0.32"/><stop offset="1" stop-color="#fbbf24" stop-opacity="0.08"/></linearGradient>'
  '<linearGradient id="padSel" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f87171" stop-opacity="0.16"/><stop offset="1" stop-color="#f87171" stop-opacity="0.03"/></linearGradient>'
  '<pattern id="hatch" width="6" height="6" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><line x1="0" y1="0" x2="0" y2="6" stroke="#2a2a34" stroke-width="2"/></pattern>'
  '</defs>')
rect(0, 0, W, H, BG, 0)

# ---------------------------------------------------------------- top bar (identical to synth)
button(3, 3, 25, 46, "<", size=14)
rect(33, 3, 225, 46, "#1c1c22", 4, "#2a2a36"); text(145, 31, "Steel Roller", 14, TXT, anchor="middle")
button(263, 3, 25, 46, ">", size=14)
x = 301
for lab, w in (("UNDO", 64), ("INIT", 54), ("PART", 75), ("RANDOMIZE", 104), ("MUTATE", 94), ("KIT RND", 64)):
    if lab == "PART":
        rect(x, 3, w, 46, "#1c1c22", 4, "#2a2a36"); text(x + 7, 31, "PART", 14, TXT)
    else:
        button(x, 3, w, 46, lab, size=14)
    x += w + 6
text(x + 8, 30, "Steel Roller", 11, DIM, "bold")
# CPU / voices readout (new, small)
text(1010, 22, "VOICES  7 / 64", 9, DIM, "bold")
text(1010, 36, "DISK  0%   RAM  48 MB", 9, DIM, "bold")
text(1185, 29, "MASTER", 8, TXT, "bold", "middle")
knob(1243, 20, 34, 0.62, AMBER)


# ---------------------------------------------------------------- LEFT: pads only (4x4 grid, pad 1 bottom-left)
LX, LY, PW, PH, GAP = 8, 58, 136, 163, 6
for idx, (name, kind, nlay, mode) in enumerate(SLOTS):
    col = COLS[idx]
    row, colm = 3 - idx // 4, idx % 4
    px, py = LX + colm * (PW + GAP), LY + row * (PH + GAP)
    sel, hit, empty = idx == SEL, idx == HIT, name is None
    if sel:
        a(f'<rect x="{px}" y="{py}" width="{PW}" height="{PH}" rx="8" fill="{col}" fill-opacity="0.35" filter="url(#glow)"/>')
    rect(px, py, PW, PH, "#22222c" if sel else PANEL, 8, col, 1.6 if sel else 1, f'stroke-opacity="{0.95 if sel else 0.42}"')
    if sel: rect(px, py, PW, PH, "url(#padSel)", 8)
    if hit: rect(px, py, PW, PH, "url(#padHit)", 8)
    text(px + 10, py + 19, f"{idx+1}", 15, col, "bold")
    text(px + PW - 9, py + 18, note(36 + idx), 9, DIM, "bold", "end")
    text(px + 10, py + 38, name if name else "EMPTY", 11, TXT if name else DIM, "bold")
    wx, wy, ww, wh = px + 10, py + 50, PW - 20, 56
    if empty:
        rect(wx, wy, ww, wh, "url(#hatch)", 4, "#34343f", 1, 'stroke-dasharray="3 3"')
        text(px + PW / 2, wy + 32, "DROP SAMPLE", 9, DIM, "bold", "middle")
    else:
        mirrored_wave(wx, wy, ww, wh, wave_points(idx * 7 + 1, 50, kind), col, 0.75 if (sel or hit) else 0.45)
    # layer dots + layer mode tag
    for l in range(4):
        filled = l < nlay
        a(f'<circle cx="{px + 14 + l * 11}" cy="{py + 120}" r="3.5" fill="{col if filled else "none"}" '
          f'stroke="{col if filled else "#3a3a46"}" stroke-width="1" fill-opacity="{0.9 if filled else 0}"/>')
    if mode:
        text(px + PW - 10, py + 124, mode, 8, col, "bold", "end", 'fill-opacity="0.85"')
    # level meter
    lvl = 0.82 if hit else (0.35 if sel else 0.0)
    rect(px + 10, py + 131, PW - 20, 3, "#2a2a34", 1.5)
    if lvl: rect(px + 10, py + 131, (PW - 20) * lvl, 3, col, 1.5)
    # S / M
    mute_on = idx == 15
    rect(px + 10, py + 140, 24, 16, BTN, 3, "#2a2a36"); text(px + 22, py + 152, "S", 9, TXT, anchor="middle")
    rect(px + 38, py + 140, 24, 16, col if mute_on else BTN, 3, None if mute_on else "#2a2a36")
    text(px + 50, py + 152, "M", 9, "#101014" if mute_on else TXT, anchor="middle")
    rect(px + 10, py + PH - 4, PW - 20, 2, col, 1, extra=f'fill-opacity="{0.9 if sel else 0.55}"')

# ---------------------------------------------------------------- RIGHT: editor for the selected pad
RX, RW = 576, 696
col = COLS[SEL]
for i, (lab, on) in enumerate((("SAMPLE", True), ("PART FX", False), ("GLOBAL FX", False))):
    button(RX + i * 118, 58, 112, 26, lab, on, size=13)
button(RX + RW - 82, 58, 82, 26, "TRIGGER", size=12)

# pad header: name + routing (output / choke / poly live only here)
text(RX + 2, 112, "2", 16, col, "bold")
rect(RX + 18, 94, 150, 24, "#1c1c22", 3, "#2a2a36"); text(RX + 25, 111, "Snare_Tight_04", 12, TXT)
xx = RX + 182
for lab, val, w in (("OUT", "3/4", 66), ("CHOKE", "None", 72), ("VOICE", "Poly", 70)):
    text(xx, 110, lab, 9, LAB, "bold"); lw = 7 * len(lab) + 4
    combo(xx + lw, 95, w, 22, val, 11); xx += lw + w + 12

# sub-pages
sub = (("SAMPLE", 58), ("AMP", 42), ("PITCH", 48), ("FILTER", 52), ("FX SEND", 58))
xx = RX
for i, (lab, w) in enumerate(sub):
    button(xx, 126, w, 22, lab, i == 0, col, 11, 3); xx += w + 4
text(xx + 8, 141, "RIGHT-CLICK = LOCK", 8, DIM, "bold")

# layer strip: mode toggle + 4 layers
LYY = 158
text(RX + 2, LYY + 16, "LAYERS", 9, LAB, "bold")
rect(RX + 50, LYY + 2, 128, 20, "#1c1c22", 3, "#2a2a36")
rect(RX + 51, LYY + 3, 46, 18, BTN, 2); text(RX + 74, LYY + 16, "STACK", 9, DIM, "bold", "middle")
rect(RX + 98, LYY + 3, 79, 18, col, 2); text(RX + 137, LYY + 16, "ROUND ROBIN", 9, "#101014", "bold", "middle")
LAYERS = [("Snare_Tight_04.wav", True), ("Snare_Tight_05.wav", False), ("Snare_Tight_06.wav", False), (None, False)]
lx = RX + 186
for i, (fn, on) in enumerate(LAYERS):
    w = 124
    rect(lx, LYY, w, 24, "#22222c" if on else "#18181e", 3, col if on else "#2a2a36", 1.2 if on else 1)
    text(lx + 7, LYY + 16, f"L{i+1}", 9, col if fn else DIM, "bold")
    text(lx + 25, LYY + 16, (fn[:-4] if fn else "+ add layer"), 9, TXT if fn else DIM)
    if fn and i == 1:   # round-robin "next up" indicator
        a(f'<circle cx="{lx + w - 9}" cy="{LYY + 12}" r="3" fill="{GREEN}"/>')
    lx += w + 4

# waveform of selected layer, with file nav in its header
WX, WY, WW, WH = RX, 192, RW, 150
rect(WX, WY, WW, WH, "#16161c", 5, "#24242c")
text(WX + 8, WY + 15, "LAYER 1", 8, col, "bold")
text(WX + 52, WY + 15, "44.1 kHz · 24-bit · stereo · 412 ms", 8, DIM)
bx = WX + WW - 300
button(bx, WY + 4, 18, 16, "<", size=10); button(bx + 21, WY + 4, 18, 16, ">", size=10)
for lab, w in (("LOAD…", 50), ("REV", 34), ("NRM", 34), ("CLEAR", 44)):
    pass
fx = bx + 44
for lab, w in (("LOAD…", 54), ("REVERSE", 62), ("NORMALIZE", 72), ("CLEAR", 46)):
    rect(fx, WY + 4, w, 16, BTN, 3, "#2a2a36"); text(fx + w / 2, WY + 15, lab, 8, TXT, "bold", "middle"); fx += w + 4
pts = [0.01] * 12 + wave_points(99, 288, "snare")
s_m, e_m = 0.04, 0.78
GY = WY + 24
mirrored_wave(WX + 8, GY, WW - 16, WH - 32, pts, col, 0.75)
sx = WX + 8 + (WW - 16) * s_m; ex = WX + 8 + (WW - 16) * e_m
rect(WX + 1, GY - 2, sx - WX - 1, WH - 23, BG, 0, extra='fill-opacity="0.72"')
rect(ex, GY - 2, WX + WW - ex - 1, WH - 23, BG, 0, extra='fill-opacity="0.72"')
a(f'<path d="M{sx},{WY+WH-8} Q{sx+10},{GY+6} {sx+26},{GY}" stroke="{AMBER}" stroke-width="1.3" fill="none" stroke-opacity="0.8"/>')
a(f'<path d="M{ex-80},{GY} Q{ex-18},{GY+4} {ex},{WY+WH-8}" stroke="{AMBER}" stroke-width="1.3" fill="none" stroke-opacity="0.8"/>')
for mx, lab in ((sx, "S"), (ex, "E")):
    a(f'<line x1="{mx}" y1="{GY-2}" x2="{mx}" y2="{WY+WH-2}" stroke="{TXT}" stroke-width="1.5"/>')
    a(f'<path d="M{mx-6},{GY-2} h12 l-6,8 z" fill="{TXT}"/>')
    text(mx + (8 if lab == "S" else -8), GY + 12, lab, 9, TXT, "bold", "start" if lab == "S" else "end")
phx = WX + 8 + (WW - 16) * 0.23
a(f'<line x1="{phx}" y1="{GY}" x2="{phx}" y2="{WY+WH-2}" stroke="{GREEN}" stroke-width="1.5" stroke-opacity="0.9"/>')

# knobs: row 1 = selected layer, row 2 = whole pad
KX0, KDX, KD = RX + 58, 116, 54
text(RX + 2, 362, "LAYER 1", 8, col, "bold")
for i, (lab, v, val, bip, lock) in enumerate((("TUNE", 0.56, "+1.0 st", True, True), ("FINE", 0.5, "0 ct", True, False),
        ("START", 0.04, "4 ms", False, False), ("END", 0.78, "322 ms", False, False), ("LEVEL", 0.78, "-2.1 dB", False, False), ("PAN", 0.5, "C", True, False))):
    knob(KX0 + i * KDX, 398, KD, v, col, lab, val, bipolar=bip, lock=lock)
a(f'<line x1="{RX}" y1="{464}" x2="{RX+RW}" y2="{464}" stroke="#22222a"/>')
text(RX + 2, 478, "PAD", 8, col, "bold")
for i, (lab, v, val, bip) in enumerate((("DRIVE", 0.25, "25%", False), ("COMP", 0.4, "40%", False), ("TONE", 0.7, "+40%", True),
        ("PUNCH", 0.8, "80%", False), ("VEL>AMP", 0.8, "80%", False), ("VEL>START", 0.1, "10%", False))):
    knob(KX0 + i * KDX, 512, KD, v, col, lab, val, bipolar=bip)

# MACROS — 4 per pad, generic, user-assignable
MY = 582
rect(RX, MY, RW, 148, PANEL, 7, col, 1, 'stroke-opacity="0.35"')
text(RX + 12, MY + 20, "MACROS", 11, TXT, "bold")
text(RX + 70, MY + 20, "PAD 2", 9, DIM, "bold")
text(RX + RW - 12, MY + 20, "RIGHT-CLICK ANY KNOB → ASSIGN TO MACRO 1-4", 8, DIM, "bold", "end")
MACROS = [("PITCH", 0.55, "TUNE (L1-4) · PITCH ENV", True), ("LENGTH", 0.62, "AMP DECAY · END", False),
          ("COLOR", 0.4, "CUTOFF · TONE · DRIVE", True), ("SPACE", 0.2, "SEND REV · SEND DLY", False)]
mw = (RW - 24) / 4
for i, (nm, v, tgt, bip) in enumerate(MACROS):
    cx = RX + 12 + mw * i + mw / 2
    if i: a(f'<line x1="{RX + 12 + mw * i}" y1="{MY + 34}" x2="{RX + 12 + mw * i}" y2="{MY + 136}" stroke="#26262f"/>')
    knob(cx - 42, MY + 80, 58, v, col, bipolar=bip)
    rect(cx - 6, MY + 46, 68, 20, "#1c1c22", 3, "#2a2a36"); text(cx + 1, MY + 60, nm, 10, TXT, "bold")
    text(cx - 6, MY + 82, f"{int(v*100)}%", 10, col)
    text(cx - 6, MY + 98, f"{tgt.count('·') + 1} TARGETS", 8, DIM, "bold")
    text(cx, MY + 128, tgt, 8, DIM, anchor="middle")

# ---------------------------------------------------------------- bottom bar (identical to synth)
rect(0, 736, W, 64, "#141418", 0)
text(15, 772, "OUTPUT", 13, LAB)
rect(99, 765, 1072, 5, "#2a2a34", 2.5)
rect(99, 765, 625, 5, ACCENT, 2.5, None, 1, 'fill-opacity="0.75"')
a(f'<circle cx="724" cy="767.5" r="6" fill="{ACCENT}"/>')
button(1197, 752, 60, 31, "LIMITER", True, size=12)
a(f'<path d="M1264,798 L1278,784 M1269,798 L1278,789 M1274,798 L1278,794" stroke="{DIM}" stroke-width="1.2"/>')

a('</svg>')
open(sys.argv[1], "w").write("\n".join(out))
