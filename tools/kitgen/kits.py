"""The 10 factory kits. Same pad layout in every kit (muscle memory):

   13 Bass/Stab  14 Vox/Stab  15 FX 1      16 FX 2
    9 Perc 1     10 Perc 2    11 Perc 3    12 Crash/Ride
    5 Kick 2      6 Snare 2    7 Open Hat   8 Ride/Shaker
    1 Kick        2 Snare      3 Clap       4 Closed Hat

Hats share choke group 1. Repeated hits get round-robin variations
(different seed + small parameter jitter); a few pads STACK separate
layers to show off layering. Pad LEVEL pre-balances the kit (samples
themselves are peak-normalised like any sample pack).
"""
from sounds import *  # noqa: F401,F403


class P:
    def __init__(self, name, fn, kw=None, var=1, jit=None, mode="rr", choke=0, voice="poly",
                 level=0.0, params=None, post=None, layers=None):
        self.name, self.fn, self.kw = name, fn, dict(kw or {})
        self.var, self.jit, self.mode = var, jit, mode
        self.choke, self.voice, self.level = choke, voice, level
        self.params = dict(params or {})
        self.post = post or []
        self.layers = layers   # STACK: list of (fn, kw, post) rendered as separate layers


# default per-recipe variation jitter (relative)
JIT = {
    "snare": {"body_hz": 0.03, "pitch_hz": 0.06, "burst_decay": 0.1, "tail_decay": 0.08, "snap": 0.15, "click": 0.15},
    "clap": {"spread": 0.15, "tail": 0.1, "tone": 0.06},
    "snare_snappy": {"tune_hz": 0.02, "snappy_decay": 0.08, "noise_lp": 0.06, "grit_hz": 0.08},
    "hat": {"decay": 0.12, "base": 0.015, "tone": 0.05},
    "shaker": {"length": 0.1, "tone": 0.08},
    "tambourine": {"length": 0.12, "jingle_lo": 0.05, "spread": 0.2},
    "rimshot": {"tone": 0.03},
    "snap": {"tone": 0.06},
    "conga": {"f0": 0.015, "slap": 0.2},
    "wood": {"f": 0.02, "decay": 0.1},
    "bell_perc": {"f": 0.01, "decay": 0.1},
    "clang": {"base": 0.04, "decay": 0.1},
    "blip": {"f_start": 0.08, "decay": 0.1},
    "noise_hit": {"decay": 0.15},
    "ride": {"decay": 0.08},
    "acid_pluck": {"env_hz": 0.25, "accent": 0.2},
    "yoi_stab": {"length": 0.1},
    "laser": {"f_hi": 0.15, "length": 0.1},
    "kick": {"tau": 0.08, "decay": 0.05},
}

HAT = dict(choke=1, params={"velamp": 55})

# Snare families, fitted to reference snares by tools/kitgen/fit_snare.py (envelopes per band +
# octave balance only - no reference audio is used). A: low thump on the hit, hard click, noise
# swelling in, long tail. B: pitch drops slower from higher up, soft attack, roomy dark noise.
SN_A = dict(body_hz=155, pitch_hz=776, pitch_tau=0.0057, body_decay=0.176, body_drv=3.3, noise_amt=1.04,
            noise_slope=-1.42, noise_hp=824, noise_attack=0.07, click=1.25, burst_decay=0.124, tail_db=-4.0,
            tail_decay=0.8, snap=1.08, snap_hz=1492, sat_db=0.0, clip_db=5.3, room=0.0)
SN_B = dict(body_hz=230, pitch_hz=900, pitch_tau=0.0112, body_decay=0.2845, body_attack=0.025, body_drv=0.0,
            noise_amt=1.91, noise_slope=-2.04, noise_hp=1172, noise_attack=0.0245, click=0.148, burst_decay=0.03,
            tail_db=-4.75, tail_decay=0.484, snap=0.0, snap_hz=3462, sat_db=6.0, clip_db=0.0, room=0.5)
# C: light snare for kick-led grooves - small high body, no clipping, short crack, clap does the work
SN_C = dict(body_hz=245, pitch_hz=520, pitch_tau=0.004, body_decay=0.09, body_amt=0.9, body_drv=0.0,
            noise_amt=1.0, noise_slope=-1.0, noise_hp=1500, noise_attack=0.002, click=0.25, burst_decay=0.09,
            tail_db=-16.0, tail_decay=0.25, snap=0.6, snap_hz=2200, sat_db=0.0, clip_db=0.0, room=0.2)
# 909-style (house / techno): recipe snare_snappy fitted to a 909 reference by fit_snare.py, pitch
# pinned to its measured partials (236 / 314 Hz) - statistics only, no reference audio is used.
SN_909 = dict(tune_hz=236.0, ratio2=1.33, bend=0.15, bend_tau=0.0182, tone_decay=0.332, tone_amt=0.562,
              tone_tail_db=-33.7, tone_tail_decay=0.768, tone_tri=0.05, tone_shape_db=3.0, snappy=0.611,
              snappy_hold=0.067, snappy_decay=0.32, snappy_hp=3197, noise_lp=10221, grit=1.5, grit_hz=2413,
              grit_decay=0.15, click=0.205, drv=0.8, width=0.05)
# tight drum & bass snare: high body (~300 Hz) with a fast sweep, gone in ~160 ms, crack on top
SN_TIGHT = dict(body_hz=300, pitch_hz=1200, pitch_tau=0.004, body_decay=0.1, body_drv=4.0, noise_amt=4.5,
                noise_slope=-0.5, noise_hp=1500, noise_attack=0.001, click=1.0, burst_decay=0.07, tail_db=-18.0,
                tail_decay=0.15, snap=1.2, snap_hz=1800, sat_db=4.0, clip_db=7.0, room=0.1)
# Heavy Water's snare, transposed: only the pitched parts move (body, sweep start, snap, clap tone);
# envelopes, noise, click, clap layer and room stay exactly as in Heavy Water
def hw_snare(semitones, **over):
    k = 2 ** (semitones / 12.0)
    d = sn(SN_A, body_hz=165 * k, pitch_hz=SN_A["pitch_hz"] * k, snap_hz=SN_A["snap_hz"] * k,
           room=0.25, room_decay=1.0, clap_amt=0.5, clap_kw=dict(tail=0.25, room=0.0, tone=1300.0 * k))
    d.update(over)
    return d
TIGHT_GHOST = dict(body_decay=0.06, tail_db=-26.0, burst_decay=0.04, clip_db=3.0, sat_db=2.0)
GHOST_TAIL = dict(tail_db=-14.0, tail_decay=0.18, burst_decay=0.05, noise_attack=0.01)


def sn(base, **over):
    d = dict(base)
    d.update(over)
    return d

GHOST = {"velamp": 85}
TONAL = {"velamp": 35}
J12 = ("crush", 12, 26000)

KITS = [
    # ------------------------------------------------------------ kit 1
    dict(index=1, name="Heavy Water", pads=[
        P("Kick", kick, dict(f0=48, ratio=5.5, tau=0.035, decay=0.55, click=0.55, click_hz=3800, punch_db=4, drv=9)),
        P("Snare", snare, sn(SN_A, body_hz=165, room=0.25, room_decay=1.0, clap_amt=0.5, clap_kw=dict(tail=0.25, room=0.0)), var=3, level=-1),
        P("Clap", clap, dict(bursts=4, tail=0.3, room=0.4, room_decay=1.4), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.045, base=3300), var=3, level=-7, **HAT),
        P("Sub Boom", boom_808, dict(note="F1", decay=1.4), voice="mono", level=-1),
        P("Snare Stack", None, mode="stack", level=-2, layers=[
            (snare, sn(SN_B, clap_amt=0.0, room=0.2, tail_decay=0.3), []),
            (clap, dict(bursts=3, tail=0.2, room=0.0, drv=6), []),
            (crash, dict(decay=0.9, base=2600), [("gate", 0.35)]),
        ]),
        P("Open Hat", hat, dict(decay=0.42, base=3200, index=4), var=2, level=-9, **HAT),
        P("Ride", ride, dict(base=430, decay=2.8), level=-10),
        P("Rim", rimshot, dict(tone=1800, room=0.2), var=2, level=-5),
        P("Metal Tick", bell_perc, dict(f=780, decay=0.18, bp_hz=2000), var=2, level=-8),
        P("Low Tom", tom, dict(f0=95, decay=0.6, drop=1.8, room=0.25), level=-4),
        P("Crash", crash, dict(decay=2.6), level=-9),
        P("Growl F", growl, dict(note="F1", length=1.2, wobble_hz=4.67), voice="mono", level=-4, params=TONAL),
        P("Vox Yeah", vox, dict(word="yeah", f0=180, length=0.5, drv=8, room=0.3, delay_s=0.214), voice="mono", level=-5, params=TONAL),
        P("Sub Drop", sub_drop, dict(length=1.8), voice="mono", level=-3, params=TONAL),
        P("Riser", riser, dict(length=2.6), level=-8, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 2
    dict(index=2, name="Saw Teeth", pads=[
        P("Kick", kick, dict(f0=52, ratio=7, tau=0.028, decay=0.45, click=0.75, click_hz=4500, punch_db=6, drv=14, drv_kind="cubic")),
        P("Snare", snare, sn(SN_A, body_hz=175, sat_db=4, clip_db=8, tail_decay=0.55), var=3, level=-1),
        P("Clap", clap, dict(bursts=3, spread=0.008, tail=0.18, tone=1600, drv=10, room=0.2), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.035, base=3600, drv=6), var=3, level=-8, **HAT),
        P("Clipped Kick", kick, dict(f0=56, ratio=9, tau=0.02, decay=0.3, click=1.0, drv=20, drv_kind="hard", punch_db=8)),
        P("Snare Crack", snare, sn(SN_B, body_hz=240, sat_db=8, clip_db=6, room=0.25, tail_decay=0.35), var=2, level=-2),
        P("Open Hat", hat, dict(decay=0.3, base=3500, index=5, drv=5), var=2, level=-9, **HAT),
        P("Shaker", shaker, dict(length=0.16), var=3, level=-10),
        P("Clang", clang, dict(base=320, decay=0.7, drv=12, room=0.25), var=2, level=-7),
        P("Zap", laser, dict(f_hi=3000, f_lo=200, length=0.12), var=2, level=-8),
        P("Tok", blip, dict(f_start=1400, f_end=500, decay=0.05, fm_index=2), var=2, level=-6),
        P("Crash", crash, dict(decay=2.0, bright_db=3), level=-9),
        P("Yoi Bass G", yoi_stab, dict(note="G1"), var=2, voice="mono", level=-4, params=TONAL),
        P("Vox Oi", vox, dict(word="oi", f0=230, length=0.32, drv=12, room=0.15), voice="mono", level=-5, params=TONAL),
        P("Laser Drop", laser, dict(f_hi=6000, f_lo=60, length=0.6, index=5), level=-6, params=TONAL),
        P("Impact", impact, dict(boom_hz=50, decay=2.4), level=-4, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 3
    dict(index=3, name="Glass Garden", pads=[
        P("Kick", kick, dict(f0=60, ratio=4.5, tau=0.022, decay=0.32, click=0.35, click_hz=3000, punch_db=3, drv=5)),
        P("Snare", snare, sn(SN_B, body_hz=225, room=0.35, room_decay=0.7), var=3, level=-1),
        P("Snare Clap", clap, dict(bursts=3, tail=0.2, room=0.35, room_decay=1.0, tone=1500), var=2, level=-3),
        P("Closed Hat", hat, dict(decay=0.035, base=3800, tone=11000, drv=1), var=4, level=-9, **HAT),
        P("Soft Kick", kick, dict(f0=55, ratio=3.5, tau=0.02, decay=0.4, click=0.15, drv=3, punch_db=1)),
        P("Ghost Snare", snare, sn(SN_B, body_hz=225, room=0.1, **GHOST_TAIL), var=3, level=-10, params=GHOST),
        P("Open Hat", hat, dict(decay=0.28, base=3700, index=3), var=2, level=-10, **HAT),
        P("Ride", ride, dict(base=470, decay=3.2, bell=0.6), var=2, level=-11),
        P("Shaker", shaker, dict(length=0.14, tone=8500), var=3, level=-11),
        P("Rim", rimshot, dict(tone=1900, room=0.15), var=2, level=-6),
        P("Glass", glass, dict(f=1250, decay=0.7), level=-10),
        P("Crash", crash, dict(decay=2.8, bright_db=1), level=-10),
        P("Reese F", reese, dict(note="F1", length=1.6, detune=18, cutoff=650, drv=6), voice="mono", level=-5, params=TONAL),
        P("Vox Ah", vox, dict(word="ah", f0=330, gender=1.25, length=0.8, room=0.45, delay_s=0.259, drv=1, breath=0.15), voice="mono", level=-6, params=TONAL),
        P("Keys Fm9", ep_chord, dict(notes=("F3", "G#3", "C4", "D#4", "G4"), decay=2.0), level=-6, params=TONAL),
        P("Riser", riser, dict(length=2.8, tone=False), level=-9, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 4
    dict(index=4, name="Static Forge", pads=[
        P("Kick", kick, dict(f0=55, ratio=8, tau=0.018, decay=0.3, click=0.85, click_hz=5000, punch_db=7, drv=12)),
        P("Snare", snare, sn(SN_A, body_hz=190, sat_db=4, clip_db=7, tail_decay=0.5), var=3, level=-1),
        P("Clap", clap, dict(bursts=4, spread=0.008, tail=0.16, tone=1800, drv=9, room=0.15), var=2, level=-3),
        P("Closed Hat", hat, dict(decay=0.03, base=4000, index=5, drv=6, hp_hz=5500), var=3, level=-8, **HAT),
        P("Kick Hard", kick, dict(f0=58, ratio=10, tau=0.014, decay=0.26, click=1.1, drv=18, drv_kind="cubic", punch_db=8)),
        P("Snare Metal", None, mode="stack", level=-2, layers=[
            (snare, sn(SN_A, body_hz=210, sat_db=6, clip_db=8, tail_decay=0.3), []),
            (clang, dict(base=900, decay=0.2, room=0.0, drv=8), []),
        ]),
        P("Open Hat", hat, dict(decay=0.22, base=3900, index=6, drv=6), var=2, level=-9, **HAT),
        P("Ride", ride, dict(base=520, decay=2.2, bell=0.8, shimmer=0.5), level=-11),
        P("Metal Hit", clang, dict(base=420, decay=0.5, drv=14, room=0.2), var=2, level=-7),
        P("Tick", blip, dict(f_start=6000, f_end=2500, sweep=0.003, decay=0.02, fm_index=3), var=2, level=-9),
        P("Noise Hit", noise_hit, dict(decay=0.08, hp_hz=1500, drv=12, room=0.15, crush=(10, 22050)), var=2, level=-8),
        P("Crash", crash, dict(decay=2.2, bright_db=3), level=-9),
        P("Metal Reese F", reese, dict(note="F1", length=1.2, detune=30, cutoff=1100, drv=10, neuro=True), voice="mono", level=-5, params=TONAL),
        P("Vox Go", vox, dict(word="go", f0=150, length=0.3, drv=14, room=0.12, gender=0.9), voice="mono", level=-6, params=TONAL),
        P("Tear Laser", laser, dict(f_hi=7000, f_lo=80, length=0.4, index=6, drv=10), level=-7, params=TONAL),
        P("Impact", impact, dict(boom_hz=48, decay=2.2, crack=0.8, drv=14), level=-4, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 5
    dict(index=5, name="Dust Archive", pads=[
        P("Kick", kick, dict(f0=62, ratio=3.5, tau=0.02, decay=0.28, click=0.4, noise=0.25, drv=7, tape_db=4, crush=(12, 26000))),
        P("Snare", snare, sn(SN_B, body_hz=205, room=0.3, room_decay=0.5, tail_decay=0.35, tape_db=5, crush=(12, 26000)), var=3, level=-1),
        P("Clap", clap, dict(bursts=3, tail=0.2, room=0.2, crush=(12, 26000)), var=2, level=-3),
        P("Closed Hat", hat, dict(decay=0.04, base=3000, crush=(12, 26000), noise_amt=0.9, metal=0.4), var=3, level=-8, **HAT),
        P("Long Boom", boom_808, dict(note="F1", decay=2.0, glide=1.8), voice="mono", level=-2),
        P("Ghost Snare", snare, sn(SN_B, body_hz=205, room=0.1, tape_db=4, crush=(12, 26000), **GHOST_TAIL), var=3, level=-9, params=GHOST),
        P("Open Hat", hat, dict(decay=0.25, base=2900, crush=(12, 26000)), var=2, level=-9, **HAT),
        P("Ride", ride, dict(base=400, decay=2.6, bell=0.5), level=-11, post=[J12, ("tape", 3)]),
        P("Rim", rimshot, dict(tone=1600, crush=(12, 26000)), var=2, level=-6),
        P("Timbale", tom, dict(f0=330, decay=0.35, drop=1.2, stick=0.6, partials=0.6, room=0.15), var=2, level=-6, post=[J12]),
        P("Tom", tom, dict(f0=120, decay=0.5, drop=1.6, room=0.2), level=-4, post=[J12]),
        P("Crash", crash, dict(decay=2.4, dark=True), level=-10, post=[J12, ("tape", 3)]),
        P("Sub F", sub_hit, dict(note="F1", decay=1.6), voice="mono", level=-3, params=TONAL),
        P("Vox Hey", vox, dict(word="hey", f0=210, length=0.35, drv=6, room=0.2, delay_s=0.273), voice="mono", level=-5, params=TONAL, post=[J12]),
        P("Air Horn", air_horn, dict(), voice="mono", level=-8, params=TONAL),
        P("Siren", siren, dict(), level=-9, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 6
    dict(index=6, name="Velvet Basement", pads=[
        P("Kick", kick, dict(f0=50, ratio=4, tau=0.03, decay=0.42, click=0.25, click_hz=2500, drv=4, drv_kind="asym", punch_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=228, room=0.25, room_decay=0.8, noise_lp=9000), var=2, level=-5),
        P("Clap", clap, dict(bursts=4, spread=0.012, tail=0.25, tone=1200, room=0.4, room_decay=1.3, drv=2), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.04, base=3000, metal=0.4, noise_amt=0.9, tone=9000, drv=1), var=3, level=-9, **HAT),
        P("Kick Round", kick, dict(f0=46, ratio=3, tau=0.035, decay=0.5, click=0.15, drv=6, drv_kind="asym", tape_db=3)),
        P("Snap", snap, dict(tone=2300, room=0.35), var=3, level=-5),
        P("Open Hat", hat, dict(decay=0.35, base=2900, index=3, metal=0.5), var=2, level=-10, **HAT),
        P("Shaker", shaker, dict(length=0.2, tone=7000), var=3, level=-10),
        P("Conga Hi", conga, dict(f0=330, decay=0.25, slap=0.3, pan_pos=0.25), var=2, level=-5),
        P("Conga Lo", conga, dict(f0=220, decay=0.3, slap=0.2, pan_pos=-0.25), var=2, level=-5),
        P("Rim", rimshot, dict(tone=1500, room=0.25), var=2, level=-6),
        P("Ride", ride, dict(base=380, decay=2.8, bell=0.3), level=-11),
        P("Sub Pluck G", pluck_bass, dict(note="G1", length=0.35, cutoff=900, drv=3), voice="mono", level=-3, params=TONAL),
        P("Vox Oh", vox, dict(word="oh", f0=330, gender=1.25, length=0.9, drv=1, room=0.5, delay_s=0.369, breath=0.15), voice="mono", level=-6, params=TONAL),
        P("Keys Cm9", ep_chord, dict(notes=("C3", "D#3", "G3", "A#3", "D4")), level=-5, params=TONAL),
        P("Organ Fm7", organ_stab, dict(notes=("F3", "G#3", "C4", "D#4")), level=-7, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 7
    dict(index=7, name="Groove Mechanic", pads=[
        P("Kick", kick, dict(f0=52, ratio=5, tau=0.025, decay=0.36, click=0.5, drv=8, punch_db=4, low_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=242, snappy_decay=0.26), var=2, level=-3),
        P("Clap", clap, dict(bursts=3, spread=0.009, tail=0.18, tone=1500, room=0.2, drv=5), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.03, base=3500, drv=3), var=3, level=-8, **HAT),
        P("Kick Punch", kick, dict(f0=55, ratio=6, tau=0.02, decay=0.3, click=0.7, drv=11, punch_db=6)),
        P("Snap", snap, dict(tone=2600, room=0.2), var=2, level=-5),
        P("Open Hat", hat, dict(decay=0.22, base=3400, index=4), var=2, level=-9, **HAT),
        P("Shaker", shaker, dict(length=0.13, tone=8000, density=1200), var=3, level=-10),
        P("Wood Tok", wood, dict(f=1100, decay=0.08, pan_pos=0.3), var=2, level=-6),
        P("Bell", bell_perc, dict(f=620, decay=0.22, pan_pos=-0.3), var=2, level=-8),
        P("Bongo", conga, dict(f0=410, decay=0.16, slap=0.5), var=2, level=-6),
        P("Crash", crash, dict(decay=2.0), level=-10),
        P("Pluck Bass G", pluck_bass, dict(note="G1", length=0.28, cutoff=2200, drv=8), voice="mono", level=-3, params=TONAL),
        P("Vox Uh", vox, dict(word="uh", f0=130, length=0.18, drv=6, room=0.15, gender=0.9), voice="mono", level=-5, params=TONAL),
        P("Noise Sweep", riser, dict(length=2.0, tone=False, f_lo=500, f_hi=12000), level=-9, params=TONAL),
        P("Zap", blip, dict(f_start=3000, f_end=120, sweep=0.03, decay=0.18, fm_index=2, drv=6), level=-7),
    ]),
    # ------------------------------------------------------------ kit 8
    dict(index=8, name="Concrete Pulse", pads=[
        P("Kick", kick, dict(f0=48, ratio=6, tau=0.022, decay=0.38, click=0.55, drv=10, punch_db=4)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=236, room=0.3, room_decay=1.0, drv=3), var=2, level=-3),
        P("Clap", clap, dict(bursts=4, tail=0.3, room=0.45, room_decay=1.6, tone=1300, drv=4), var=3, level=-3),
        P("Closed Hat", hat, dict(decay=0.035, base=3200, index=4.5, drv=4), var=3, level=-8, **HAT),
        P("Rumble Kick", rumble_kick, dict(kick_kw=dict(f0=46, ratio=6, tau=0.022, decay=0.35, click=0.5, drv=9)), level=-1),
        P("Rim", rimshot, dict(tone=1700, room=0.3), var=2, level=-6),
        P("Open Hat", hat, dict(decay=0.32, base=3100, index=5, drv=4), var=2, level=-9, **HAT),
        P("Ride", ride, dict(base=500, decay=2.4, bell=0.25, shimmer=0.5), var=2, level=-11),
        P("Tok", blip, dict(f_start=900, f_end=450, sweep=0.01, decay=0.06, fm_index=1.5, pan_pos=0.2), var=2, level=-6),
        P("Metal", bell_perc, dict(f=880, ratios=(1, 1.37, 2.21, 3.1), decay=0.3, bp_hz=2500, pan_pos=-0.2), var=2, level=-8),
        P("Noise Hat", noise_hit, dict(decay=0.12, hp_hz=3000, drv=6, room=0.2, crush=None), var=2, level=-10),
        P("Crash", crash, dict(decay=2.6, dark=True), level=-10),
        P("Echo Chord Cm", dub_chord, dict(notes=("C3", "D#3", "G3"), delay_s=0.346), level=-6, params=TONAL),
        P("Squelch A", acid_pluck, dict(note="A1", res=10), var=2, voice="mono", level=-6, params=TONAL),
        P("Impact", impact, dict(boom_hz=45, decay=3.0, room=0.7), level=-5, params=TONAL),
        P("Riser", riser, dict(length=3.0), level=-9, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 9
    dict(index=9, name="Rust Cathedral", pads=[
        P("Kick", kick, dict(f0=45, ratio=9, tau=0.03, decay=0.5, click=0.8, noise=0.35, drv=22, drv_kind="fold", punch_db=6, top_db=3)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=225, drv=10, room=0.45, room_decay=1.4), var=2, level=-10, post=[("gate", 0.32)]),
        P("Clap", clap, dict(bursts=5, tail=0.35, drv=14, room=0.5, room_decay=2.0, crush=(10, 22050)), var=2, level=-3),
        P("Closed Hat", hat, dict(decay=0.04, base=2800, index=7, drv=12, crush=(8, 22050)), var=3, level=-9, **HAT),
        P("Rumble Kick", rumble_kick, dict(kick_kw=dict(f0=44, ratio=8, tau=0.025, decay=0.45, click=0.6, drv=16, drv_kind="fold"), rumble_decay=2.2, drv=18), level=-1),
        P("Noise Burst", noise_hit, dict(decay=0.18, gate=40, drv=16, room=0.35), var=2, level=-6),
        P("Open Hat", hat, dict(decay=0.4, base=2700, index=8, drv=10), var=2, level=-10, **HAT),
        P("Metal Ride", ride, dict(base=330, decay=3.5, bell=0.9, shimmer=0.7), level=-11, post=[("drive", 6)]),
        P("Clang", clang, dict(base=190, decay=1.4), var=2, level=-7),
        P("Pipe", clang, dict(base=520, partials=10, decay=0.9, drv=6, room=0.4), var=2, level=-8),
        P("Anvil", bell_perc, dict(f=950, ratios=(1, 2.43, 4.1, 6.3), decay=0.5, bp_hz=3000, drv=12), level=-8),
        P("Crash", crash, dict(decay=3.0, dark=True, base=1400), level=-10, post=[("drive", 8)]),
        P("Dark Stab", dub_chord, dict(notes=("C3", "C#3", "G3"), cutoff=2500, delay_s=0.333, fb=0.45, room=0.5), level=-6, params=TONAL, post=[("drive", 6)]),
        P("Vox Grunt", vox, dict(word="uh", f0=95, length=0.3, drv=20, room=0.4, gender=0.8), voice="mono", level=-6, params=TONAL),
        P("Siren", siren, dict(lo=300, hi=900, rate=0.8, length=2.2), level=-9, params=TONAL, post=[("drive", 8)]),
        P("Impact", impact, dict(boom_hz=40, decay=3.5, crack=1.0, drv=20, room=0.75), level=-4, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 10
    dict(index=10, name="Brass Tacks", pads=[
        P("Kick", kick, dict(f0=56, ratio=4, tau=0.028, decay=0.34, click=0.45, drv=8, drv_kind="asym", punch_db=5, tape_db=3)),
        P("Snare", snare, sn(SN_A, body_hz=170, tail_decay=0.6, room=0.2, room_decay=0.7, tape_db=4), var=3, level=-2),
        P("Clap", clap, dict(bursts=4, tail=0.22, room=0.3, drv=6), var=2, level=-3),
        P("Closed Hat", hat, dict(decay=0.04, base=3300, drv=3), var=3, level=-8, **HAT),
        P("Kick Dusty", kick, dict(f0=58, ratio=3.5, tau=0.02, decay=0.3, noise=0.2, drv=8, tape_db=6, crush=(12, 30000))),
        P("Ghost Snare", snare, sn(SN_A, body_hz=170, room=0.1, tape_db=4, **GHOST_TAIL), var=3, level=-10, params=GHOST),
        P("Open Hat", hat, dict(decay=0.3, base=3200), var=2, level=-9, **HAT),
        P("Ride", ride, dict(base=450, decay=2.6, bell=0.5), var=2, level=-11),
        P("Tom Hi", tom, dict(f0=170, decay=0.4, room=0.2), level=-5),
        P("Tom Lo", tom, dict(f0=110, decay=0.55, room=0.2), level=-5),
        P("Cowbell", bell_perc, dict(f=560), var=2, level=-9),
        P("Crash", crash, dict(decay=2.4), level=-10),
        P("Squelch Bass A", acid_pluck, dict(note="A1", res=8, env_hz=2500), var=3, voice="mono", level=-5, params=TONAL),
        P("Vox Yeah", vox, dict(word="yeah", f0=240, length=0.42, drv=6, room=0.25, delay_s=0.341), voice="mono", level=-5, params=TONAL, post=[("tape", 4)]),
        P("Reverse Crash", reverse_crash, dict(decay=2.0), level=-9, params=TONAL),
        P("Tape Stop", lambda rng: tape_stop(acid_pluck(rng, note="A1", length=0.9, res=6, env_hz=1800), 0.8), level=-6, params=TONAL),
    ]),
    # ============ kick-led grooves: kicks carry the weight, light snares (SN_C) sit 6-8 dB under them
    # ------------------------------------------------------------ kit 11
    dict(index=11, name="Night Bus", pads=[
        P("Kick", kick, dict(f0=55, ratio=4.5, tau=0.022, decay=0.32, click=0.45, drv=7, punch_db=4, low_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=248, snappy_decay=0.24, room=0.15), var=3, level=-3),
        P("Clap", clap, dict(bursts=3, spread=0.008, tail=0.2, tone=1500, room=0.25), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.03, base=3600, drv=2), var=4, level=-9, **HAT),
        P("Kick Sub", kick, dict(f0=48, ratio=3.0, tau=0.03, decay=0.55, click=0.2, drv=5)),
        P("Rim", rimshot, dict(tone=1750, room=0.2), var=3, level=-7),
        P("Open Hat", hat, dict(decay=0.2, base=3500), var=2, level=-10, **HAT),
        P("Shaker", shaker, dict(length=0.12, tone=8500), var=3, level=-11),
        P("Snap", snap, dict(tone=2400, room=0.3), var=2, level=-7),
        P("Wood Tok", wood, dict(f=950, decay=0.07, pan_pos=0.25), var=2, level=-8),
        P("Bongo", conga, dict(f0=380, decay=0.17, slap=0.4, pan_pos=-0.2), var=2, level=-7),
        P("Ride", ride, dict(base=520, decay=2.2, bell=0.5), level=-12),
        P("Warp Bass F", reese, dict(note="F1", length=0.7, detune=14, cutoff=900, drv=8), voice="mono", level=-5, params=TONAL),
        P("Vox Oh", vox, dict(word="oh", f0=300, gender=1.2, length=0.5, room=0.3, delay_s=0.33, drv=2), voice="mono", level=-7, params=TONAL),
        P("Organ Stab Cm7", organ_stab, dict(notes=("C4", "D#4", "G4", "A#4"), length=0.3), level=-8, params=TONAL),
        P("Reverse Crash", reverse_crash, dict(decay=1.6), level=-10, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 12
    dict(index=12, name="Gold Leaf", pads=[
        P("Kick", kick, dict(f0=50, ratio=4.0, tau=0.03, decay=0.45, click=0.3, drv=5, drv_kind="asym", punch_db=3, low_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=232, room=0.25, room_decay=0.8), var=2, level=-4),
        P("Clap", clap, dict(bursts=4, spread=0.012, tail=0.28, room=0.45, room_decay=1.4), var=3, level=-1),
        P("Closed Hat", hat, dict(decay=0.04, base=3100, metal=0.45, noise_amt=0.9), var=3, level=-9, **HAT),
        P("Kick Warm", kick, dict(f0=47, ratio=3.5, tau=0.035, decay=0.55, click=0.2, drv=6, drv_kind="asym", tape_db=3)),
        P("Tambourine", tambourine, dict(length=0.22), var=3, level=-10),
        P("Open Hat", hat, dict(decay=0.3, base=3000, index=3), var=2, level=-10, **HAT),
        P("Shaker", shaker, dict(length=0.18, tone=7500), var=3, level=-11),
        P("Conga Hi", conga, dict(f0=340, decay=0.24, slap=0.3, pan_pos=0.3), var=2, level=-7),
        P("Conga Lo", conga, dict(f0=225, decay=0.3, slap=0.2, pan_pos=-0.3), var=2, level=-7),
        P("Rim", rimshot, dict(tone=1550, room=0.25), var=2, level=-8),
        P("Crash", crash, dict(decay=2.2), level=-11),
        P("Piano Am7", house_piano, dict(notes=("A3", "C4", "E4", "G4")), level=-6, params=TONAL),
        P("Organ Bass G", pluck_bass, dict(note="G1", length=0.35, cutoff=1200, drv=4), voice="mono", level=-4, params=TONAL),
        P("Vox Yeah", vox, dict(word="yeah", f0=260, gender=1.15, length=0.45, room=0.3, delay_s=0.363, drv=3), voice="mono", level=-7, params=TONAL),
        P("Noise Sweep", riser, dict(length=2.0, tone=False), level=-10, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 13
    dict(index=13, name="Thin Line", pads=[
        P("Kick", kick, dict(f0=52, ratio=5.0, tau=0.018, decay=0.3, click=0.5, click_hz=4500, drv=6, punch_db=5)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=250, snappy_decay=0.18, snappy_hold=0.03, tone_decay=0.2), var=2, level=-3),
        P("Clap", clap, dict(bursts=3, spread=0.007, tail=0.12, room=0.15, tone=1700), var=2, level=-4),
        P("Closed Hat", hat, dict(decay=0.025, base=4000, metal=0.5), var=3, level=-10, **HAT),
        P("Kick Click", kick, dict(f0=56, ratio=7.0, tau=0.012, decay=0.22, click=0.9, click_hz=6000, drv=8)),
        P("Rim", rimshot, dict(tone=2000, decay=0.03), var=3, level=-8),
        P("Open Hat", hat, dict(decay=0.15, base=3800), var=2, level=-11, **HAT),
        P("Ride", ride, dict(base=600, decay=1.6, bell=0.3), level=-12),
        P("Blip", blip, dict(f_start=1200, f_end=900, sweep=0.005, decay=0.04, fm_index=1.0, pan_pos=0.2), var=2, level=-8),
        P("Tick", blip, dict(f_start=7000, f_end=4000, sweep=0.002, decay=0.015, fm_index=2.0, pan_pos=-0.25), var=3, level=-11),
        P("Wood", wood, dict(f=1500, decay=0.05), var=2, level=-9),
        P("Noise Hat", noise_hit, dict(decay=0.08, hp_hz=5000, drv=4, room=0.1, crush=None), var=2, level=-12),
        P("Sub Pluck A", pluck_bass, dict(note="A1", length=0.2, cutoff=700, drv=3), voice="mono", level=-4, params=TONAL),
        P("Echo Stab Fm", dub_chord, dict(notes=("F3", "G#3", "C4"), decay=0.3, delay_s=0.357), level=-8, params=TONAL),
        P("Glass", glass, dict(f=1600, decay=0.5), level=-11),
        P("Riser", riser, dict(length=2.5, tone=False), level=-10, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 14
    dict(index=14, name="Black Steel", pads=[
        P("Kick", kick, dict(f0=47, ratio=7.0, tau=0.022, decay=0.45, click=0.7, drv=14, punch_db=5, low_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=230, drv=6, room=0.35, room_decay=1.2), var=2, level=-2),
        P("Clap", clap, dict(bursts=4, tail=0.35, room=0.5, room_decay=1.8, drv=8), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.035, base=3200, index=5, drv=6), var=3, level=-9, **HAT),
        P("Rumble Kick", rumble_kick, dict(kick_kw=dict(f0=46, ratio=7, tau=0.022, decay=0.4, click=0.6, drv=12), rumble_decay=1.8, drv=14), level=-1),
        P("Rim", rimshot, dict(tone=1600, room=0.35), var=2, level=-8),
        P("Open Hat", hat, dict(decay=0.35, base=3000, index=6, drv=6), var=2, level=-10, **HAT),
        P("Ride", ride, dict(base=520, decay=2.6, bell=0.2, shimmer=0.6), var=2, level=-12),
        P("Metal", bell_perc, dict(f=930, ratios=(1, 1.41, 2.33, 3.6), decay=0.25, bp_hz=2600), var=2, level=-9),
        P("Tom", tom, dict(f0=110, decay=0.45, room=0.3), level=-6),
        P("Noise Burst", noise_hit, dict(decay=0.15, hp_hz=2000, drv=10, room=0.3), var=2, level=-9),
        P("Crash", crash, dict(decay=2.6, dark=True), level=-11),
        P("Squelch E", acid_pluck, dict(note="E2", res=11), var=3, voice="mono", level=-7, params=TONAL),
        P("Stab Dm", dub_chord, dict(notes=("D3", "F3", "A3"), cutoff=2000, delay_s=0.341), level=-8, params=TONAL),
        P("Impact", impact, dict(boom_hz=45, decay=2.8, room=0.6), level=-6, params=TONAL),
        P("Riser", riser, dict(length=3.0), level=-10, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 15
    dict(index=15, name="Red Clay", pads=[
        P("Kick", kick, dict(f0=50, ratio=4.0, tau=0.03, decay=0.5, click=0.25, drv=5, drv_kind="asym", low_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=245, snappy_decay=0.24), var=2, level=-3),
        P("Clap", clap, dict(bursts=4, tail=0.25, room=0.35), var=2, level=-3),
        P("Closed Hat", hat, dict(decay=0.035, base=3000, metal=0.4), var=3, level=-10, **HAT),
        P("Kick Deep", kick, dict(f0=45, ratio=3.0, tau=0.04, decay=0.7, click=0.1, drv=4)),
        P("Shaker Long", shaker, dict(length=0.26, tone=7000, shape=((0, 0), (0.06, 1.0), (0.15, 0.5), (0.26, 0.0))), var=3, level=-11),
        P("Open Hat", hat, dict(decay=0.28, base=2900), var=2, level=-11, **HAT),
        P("Shaker", shaker, dict(length=0.12, tone=8000), var=3, level=-11),
        P("Conga Hi", conga, dict(f0=345, decay=0.24, slap=0.35, pan_pos=0.35), var=3, level=-6),
        P("Conga Lo", conga, dict(f0=228, decay=0.3, slap=0.25, pan_pos=-0.35), var=3, level=-6),
        P("Tom Low", tom, dict(f0=90, decay=0.6, room=0.2), var=2, level=-6),
        P("Ride", ride, dict(base=460, decay=2.4, bell=0.4), level=-12),
        P("Bongo", conga, dict(f0=450, decay=0.14, slap=0.5, pan_pos=0.15), var=3, level=-7),
        P("Woodblock", wood, dict(f=1300, decay=0.06, pan_pos=-0.2), var=2, level=-8),
        P("Agogo", bell_perc, dict(f=700, ratios=(1, 1.5, 2.2, 3.0), decay=0.3, bp_hz=1600, pan_pos=0.25), var=2, level=-9),
        P("Vox Ah", vox, dict(word="ah", f0=240, gender=1.1, length=0.5, room=0.4, drv=2), voice="mono", level=-8, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 16
    dict(index=16, name="Mirror Ball", pads=[
        P("Kick", kick, dict(f0=52, ratio=3.5, tau=0.03, decay=0.38, click=0.3, drv=4, drv_kind="asym", tape_db=2)),
        P("Snare", snare_snappy, sn(SN_909, tune_hz=234, room=0.25, tape_db=3), var=3, level=-7),
        P("Clap", clap, dict(bursts=4, tail=0.24, room=0.35), var=3, level=-2),
        P("Closed Hat", hat, dict(decay=0.04, base=3200), var=3, level=-9, **HAT),
        P("Kick Round", kick, dict(f0=48, ratio=3.0, tau=0.035, decay=0.5, click=0.2, drv=5, drv_kind="asym", tape_db=4)),
        P("Tambourine", tambourine, dict(length=0.2), var=3, level=-9),
        P("Open Hat", hat, dict(decay=0.32, base=3100), var=2, level=-10, **HAT),
        P("Shaker", shaker, dict(length=0.16), var=2, level=-11),
        P("Conga Hi", conga, dict(f0=340, decay=0.22, slap=0.3, pan_pos=0.3), var=2, level=-7),
        P("Tom Hi", tom, dict(f0=190, decay=0.4, room=0.25), level=-6),
        P("Tom Lo", tom, dict(f0=120, decay=0.5, room=0.25), level=-6),
        P("Crash", crash, dict(decay=2.2), level=-11),
        P("Disco Bass A", pluck_bass, dict(note="A1", length=0.25, cutoff=2600, drv=5), voice="mono", level=-4, params=TONAL),
        P("Piano Fmaj7", house_piano, dict(notes=("F3", "A3", "C4", "E4")), level=-6, params=TONAL),
        P("Vox Oh", vox, dict(word="oh", f0=330, gender=1.25, length=0.6, room=0.4, delay_s=0.375, drv=1, breath=0.15), voice="mono", level=-7, params=TONAL),
        P("Reverse Crash", reverse_crash, dict(decay=1.8), level=-10, params=TONAL),
    ]),
    # ============ drum & bass: short punchy kicks, Heavy Water snare transposed up (hw_snare)
    # ------------------------------------------------------------ kit 17
    dict(index=17, name="Cold Circuit", pads=[
        P("Kick", kick, dict(f0=62, ratio=7, tau=0.012, decay=0.22, click=1.0, click_hz=5500, punch_db=9, drv=12)),
        P("Snare", snare, hw_snare(5), var=3, level=-1),
        P("Clap", clap, dict(bursts=3, spread=0.007, tail=0.14, tone=1800, room=0.12, drv=6), var=2, level=-4),
        P("Closed Hat", hat, dict(decay=0.03, base=3900, drv=3), var=4, level=-9, **HAT),
        P("Kick Sub", kick, dict(f0=56, ratio=6, tau=0.014, decay=0.32, click=0.7, punch_db=7, drv=10)),
        P("Ghost Snare", snare, hw_snare(5, **GHOST_TAIL), var=3, level=-10, params=GHOST),
        P("Open Hat", hat, dict(decay=0.2, base=3800), var=2, level=-10, **HAT),
        P("Ride", ride, dict(base=520, decay=2.4, bell=0.6), var=2, level=-11),
        P("Shaker", shaker, dict(length=0.1, tone=8500), var=3, level=-11),
        P("Rim", rimshot, dict(tone=2000, room=0.1), var=2, level=-7),
        P("Tick", blip, dict(f_start=6500, f_end=3000, sweep=0.002, decay=0.018, fm_index=2.0), var=2, level=-10),
        P("Crash", crash, dict(decay=2.2, bright_db=2), level=-10),
        P("Reese F", reese, dict(note="F1", length=1.4, detune=20, cutoff=800, drv=8), voice="mono", level=-5, params=TONAL),
        P("Vox Go", vox, dict(word="go", f0=160, length=0.3, drv=10, room=0.15), voice="mono", level=-6, params=TONAL),
        P("Riser", riser, dict(length=2.4), level=-9, params=TONAL),
        P("Impact", impact, dict(boom_hz=48, decay=2.2, crack=0.8, drv=12), level=-5, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 18
    dict(index=18, name="Razor Wire", pads=[
        P("Kick", kick, dict(f0=60, ratio=9, tau=0.01, decay=0.2, click=1.2, click_hz=6000, punch_db=9, drv=16, drv_kind="cubic")),
        P("Snare", snare, hw_snare(4), var=3, level=-1),
        P("Clap", clap, dict(bursts=4, spread=0.007, tail=0.12, tone=2000, drv=12, room=0.1), var=2, level=-4),
        P("Closed Hat", hat, dict(decay=0.028, base=4200, index=6, drv=8), var=3, level=-9, **HAT),
        P("Kick Dist", kick, dict(f0=58, ratio=10, tau=0.01, decay=0.22, click=1.2, drv=20, drv_kind="hard", punch_db=9)),
        P("Snare Metal", None, mode="stack", level=-2, layers=[
            (snare, hw_snare(4), []),
            (clang, dict(base=1100, decay=0.15, room=0.0, drv=10), []),
        ]),
        P("Open Hat", hat, dict(decay=0.18, base=4100, index=7, drv=8), var=2, level=-10, **HAT),
        P("Ride", ride, dict(base=560, decay=2.0, bell=0.9, shimmer=0.6), level=-12),
        P("Metal Hit", clang, dict(base=460, decay=0.4, drv=14, room=0.15), var=2, level=-8),
        P("Noise Hit", noise_hit, dict(decay=0.07, hp_hz=2000, drv=14, room=0.1, crush=(10, 22050)), var=2, level=-9),
        P("Zap", laser, dict(f_hi=4000, f_lo=300, length=0.1), var=2, level=-9),
        P("Crash", crash, dict(decay=2.0, bright_db=3), level=-10),
        P("Metal Reese F#", reese, dict(note="F#1", length=1.0, detune=32, cutoff=1300, drv=12, neuro=True), voice="mono", level=-5, params=TONAL),
        P("Metal Reese C", reese, dict(note="C2", length=0.8, detune=28, cutoff=1500, drv=12, neuro=True), voice="mono", level=-5, params=TONAL),
        P("Tear Laser", laser, dict(f_hi=8000, f_lo=70, length=0.45, index=7, drv=12), level=-8, params=TONAL),
        P("Impact", impact, dict(boom_hz=46, decay=2.0, crack=1.0, drv=16), level=-5, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 19
    dict(index=19, name="Glass Engine", pads=[
        P("Kick", kick, dict(f0=64, ratio=6, tau=0.01, decay=0.18, click=1.0, click_hz=6500, punch_db=8, drv=9)),
        P("Snare", snare, hw_snare(7), var=3, level=-2),
        P("Clap", clap, dict(bursts=3, spread=0.006, tail=0.1, tone=2200, room=0.08), var=2, level=-5),
        P("Closed Hat", hat, dict(decay=0.022, base=4500, metal=0.5), var=3, level=-10, **HAT),
        P("Kick Click", kick, dict(f0=66, ratio=8, tau=0.008, decay=0.16, click=1.3, click_hz=7000, drv=10, punch_db=9)),
        P("Ghost Snare", snare, hw_snare(7, **GHOST_TAIL), var=3, level=-10, params=GHOST),
        P("Open Hat", hat, dict(decay=0.14, base=4300), var=2, level=-11, **HAT),
        P("Ride", ride, dict(base=640, decay=1.8, bell=0.4), level=-12),
        P("Tick", blip, dict(f_start=7000, f_end=4000, sweep=0.002, decay=0.015, fm_index=2.0), var=3, level=-11),
        P("Blip", blip, dict(f_start=1500, f_end=1100, sweep=0.004, decay=0.035, fm_index=1.2), var=2, level=-9),
        P("Glass", glass, dict(f=1900, decay=0.4), var=2, level=-11),
        P("Noise Hat", noise_hit, dict(decay=0.06, hp_hz=6000, drv=4, room=0.05, crush=None), var=2, level=-12),
        P("Sub G", sub_hit, dict(note="G1", decay=0.8), voice="mono", level=-4, params=TONAL),
        P("Echo Stab Am", dub_chord, dict(notes=("A3", "C4", "E4"), decay=0.25, delay_s=0.259, fb=0.4, room=0.3), level=-8, params=TONAL),
        P("Laser", laser, dict(f_hi=5000, f_lo=400, length=0.15), var=2, level=-9, params=TONAL),
        P("Riser", riser, dict(length=2.0, tone=False), level=-10, params=TONAL),
    ]),
    # ------------------------------------------------------------ kit 20
    dict(index=20, name="Hard Light", pads=[
        P("Kick", kick, dict(f0=60, ratio=7, tau=0.012, decay=0.24, click=1.0, punch_db=8, drv=12, low_db=2)),
        P("Snare", snare, hw_snare(5), var=3, level=0),
        P("Clap", clap, dict(bursts=3, spread=0.008, tail=0.16, tone=1700, drv=8, room=0.15), var=3, level=-4),
        P("Closed Hat", hat, dict(decay=0.03, base=3800, drv=4), var=3, level=-9, **HAT),
        P("Kick Sub", kick, dict(f0=54, ratio=6, tau=0.014, decay=0.34, click=0.7, drv=10)),
        P("Snare Crack", snare, hw_snare(7, clip_db=8), var=2, level=-2),
        P("Open Hat", hat, dict(decay=0.22, base=3700, drv=4), var=2, level=-10, **HAT),
        P("Shaker", shaker, dict(length=0.11, tone=8200), var=3, level=-11),
        P("Rim", rimshot, dict(tone=1900, room=0.12), var=2, level=-7),
        P("Tom", tom, dict(f0=160, decay=0.3, drop=1.6, room=0.1), var=2, level=-6),
        P("Zap", blip, dict(f_start=3500, f_end=150, sweep=0.025, decay=0.15, fm_index=2.0, drv=6), level=-8),
        P("Crash", crash, dict(decay=2.0, bright_db=2), level=-10),
        P("Yoi Bass F", yoi_stab, dict(note="F1"), var=2, voice="mono", level=-4, params=TONAL),
        P("Vox Oi", vox, dict(word="oi", f0=240, length=0.3, drv=12, room=0.12), voice="mono", level=-6, params=TONAL),
        P("Siren", siren, dict(lo=700, hi=1300, rate=6.0, length=1.4), level=-10, params=TONAL),
        P("Air Horn", air_horn, dict(), voice="mono", level=-9, params=TONAL),
    ]),
]
