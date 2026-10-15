"""Sound recipes. Every function takes a numpy Generator (variation seed) plus
recipe parameters and returns a stereo (2, n) float array, unnormalised.
build.py trims + normalises. Genre flavour comes from the parameters in kits.py.
"""
import numpy as np
from dsp import *  # noqa: F401,F403

NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}


def hz(name):
    """'F1' -> 43.65 Hz ('C4' = middle C, 261.6 Hz)."""
    pitch, octave = name[:-1], int(name[-1])
    return 440.0 * 2 ** ((NOTE[pitch] + (octave + 1) * 12 - 69) / 12.0)


def chord_hz(names):
    return [hz(x) for x in names]


# ================================================================ kicks
def kick(rng, f0=50.0, ratio=5.0, tau=0.03, curve=1.0, decay=0.45, hold=0.0,
         click=0.4, click_hz=3500.0, noise=0.0, punch_db=3.0, drv=6.0, drv_kind="tanh",
         crush=None, tape_db=0.0, low_db=0.0, top_db=0.0, length=None):
    n = n_of(length or decay * 1.25 + hold + 0.04)
    f = pitch_drop(n, f0, ratio, tau, curve)
    body = sine(f) * env_exp(n, decay, attack=0.0002, hold=hold)
    # 2nd harmonic gives small speakers something to read
    body += 0.12 * sine(f * 2) * env_exp(n, decay * 0.35)
    snap = bp(white(n, rng), click_hz, 0.8) * env_exp(n, 0.008)
    snap += 0.5 * sine(click_hz * 0.45, n) * env_exp(n, 0.006)
    x = body + 1.6 * click * snap
    if noise:
        x += noise * hp(white(n, rng), 2500) * env_exp(n, 0.03)
    x = drive(x, drv, drv_kind)
    if punch_db:
        x = transient(x, punch_db, 6)
    if low_db:
        x = lowshelf(x, 90, low_db)
    if top_db:
        x = highshelf(x, 4000, top_db)
    if tape_db:
        x = tape(x, tape_db)
    if crush:
        x = lp(bitcrush(x, *crush), crush[1] * 0.45)
    return to_stereo(dc_block(x))


def rumble_kick(rng, kick_kw, rumble_decay=1.6, rumble_lp=180.0, rumble_amt=0.9, drv=10.0):
    """techno rumble: kick -> long reverb -> lowpass -> distortion -> ducked under the kick."""
    k = mono(kick(rng, **kick_kw))
    ir = make_ir(rumble_decay, rng, predelay=0.0, damp_hz=1500)
    wet = mono(convolve(k, ir, wet=1.0, dry=0.0))
    wet = lp(drive(lp(wet, rumble_lp, order=2) * 4, drv), rumble_lp * 1.2, order=4)  # steep: rumble stays sub
    wet = sidechain_duck(wet, 0.0, depth=0.95, release_s=0.22)
    wet *= np.max(np.abs(k)) / (np.max(np.abs(wet)) + 1e-9) * rumble_amt
    x = mix(k, wet)
    return to_stereo(x)


def boom_808(rng, note="F1", decay=1.6, glide=1.5, glide_s=0.06, drv=8.0, click=0.25):
    """long tuned sub kick (jungle / breaks / dubstep sub boom)."""
    f0 = hz(note)
    n = n_of(decay * 1.2 + 0.05)
    f = pitch_drop(n, f0, glide, glide_s)
    x = sine(f) * env_exp(n, decay, attack=0.0003)
    x += click * bp(white(n, rng), 3000, 1.0) * env_exp(n, 0.004)
    x = drive(x, drv, "asym")
    return to_stereo(dc_block(x))


# ================================================================ snares / claps
def clap(rng, bursts=4, spread=0.010, tone=1300.0, q=1.1, tail=0.22, room=0.25, room_decay=0.7,
         drv=4.0, width=0.35, crush=None, bright_db=2.0):
    n = n_of(tail * 1.5 + bursts * spread + 0.05)
    nz = white(n, rng)
    env = np.zeros(n)
    t = 0.0
    for k in range(bursts):
        t += spread * (0.75 + 0.5 * rng.random()) if k else 0.0
        i = n_of(t)
        d = tail if k == bursts - 1 else 0.007 + 0.004 * rng.random()
        e = env_exp(n - i, d, attack=0.0004)
        env[i:] = np.maximum(env[i:], e * (0.8 + 0.2 * rng.random()))
    src = hp(bp(nz, tone, q), 600)
    src = highshelf(src, 5000, bright_db)
    x = drive(src * env * 3, drv)
    st = widen(x, rng, width)
    if room:
        st = convolve(st, make_ir(room_decay, rng, predelay=0.006, damp_hz=7000), wet=room)
    if crush:
        st = lp(bitcrush(st, *crush), crush[1] * 0.45)
    return st


def snare(rng, body_hz=180.0, pitch_hz=450.0, pitch_tau=0.004, body_decay=0.14, body_attack=0.0003, body_amt=1.0, body_drv=6.0,
          ring=0.0, noise_amt=0.8, noise_slope=-1.5, noise_hp=300.0, noise_attack=0.0005, burst_decay=0.07,
          tail_db=-12.0, tail_decay=0.45, click=0.4, snap=0.5, snap_hz=1800.0, sat_db=4.0, clip_db=3.0, room=0.2, room_decay=0.5, width=0.3,
          clap_amt=0.0, clap_kw=None, crush=None, tape_db=0.0, length=None):
    """electronic snare, shaped after measured reference snares:
       body  - sine whose pitch falls fast (pitch_hz -> body_hz, tau ~4 ms): the low thump lands WITH the
               hit and is gone by ~100 ms (no lingering tone)
       click - 3 ms high transient (the hit)
       noise - full-range (slightly pink) noise that can swell in after the click (noise_attack),
               then a lower, longer tail plateau
       snap  - short mid-band crack (~1-3 kHz)
       then light saturation / clipping and a short stereo room."""
    n = n_of(length or max(tail_decay, body_decay) * 1.25 + 0.08)
    t = tvec(n)
    f = body_hz + (pitch_hz - body_hz) * np.exp(-t / pitch_tau)
    body = sine(f) * env_exp(n, body_decay, attack=body_attack)
    body = drive(body, body_drv)
    if ring:
        body += ring * sine(body_hz * 1.47, n) * env_exp(n, body_decay * 0.9)
    nz = hp(colored(n, rng, noise_slope), noise_hp, order=2)
    nz = lp(nz, 15000)
    nenv = np.maximum(env_exp(n, burst_decay, attack=noise_attack),
                      db(tail_db) * env_exp(n, tail_decay, attack=max(0.004, noise_attack)))
    nz = nz * nenv
    sn = bp(white(n, rng), snap_hz, 1.2) * env_exp(n, 0.012, attack=0.0003)
    ck = hp(white(n, rng), 3000) * env_exp(n, 0.003, attack=0.0001)
    unit = lambda v: v / (np.max(np.abs(v)) + 1e-9)
    x = body_amt * unit(body) + noise_amt * unit(nz) + snap * unit(sn) + click * unit(ck)
    x = drive(unit(x), sat_db)
    if clip_db:
        x = drive(unit(x), clip_db, "cubic")
    st = widen(x, rng, width)
    if clap_amt:
        st = mix(st, clap(rng, **(clap_kw or {})) * clap_amt)
    if room:   # room on the noise/crack only: its low end would bring back the lingering tone
        wet = convolve(st, make_ir(room_decay, rng, predelay=0.008, damp_hz=6000), wet=1.0, dry=0.0)
        st = mix(st, hp(wet, 350, order=2) * room)
    if tape_db:
        st = tape(st, tape_db)
    if crush:
        st = lp(bitcrush(st, *crush), crush[1] * 0.45)
    return st


def snare_snappy(rng, tune_hz=258.0, ratio2=1.3, bend=0.27, bend_tau=0.005, tone_decay=0.25, tone_amt=0.8,
                 tone_tail_db=-60.0, tone_tail_decay=0.4,
                 tone_tri=0.2, tone_shape_db=0.0, snappy=1.0, snappy_hold=0.04, snappy_decay=0.2,
                 snappy_hp=1500.0, noise_lp=16000.0, grit=0.3, grit_hz=1200.0, grit_decay=0.04, click=0.3,
                 drv=2.0, room=0.0, room_decay=0.5, width=0.05, tape_db=0.0, crush=None, length=None):
    """drum-machine snare for house / techno, in the spirit of a 909 (not a copy of its circuit):
       tone   - two close, near-sine partials (tune_hz and tune_hz*ratio2, ~1.3 apart) with a short
                downward bend and a long, quiet decay: the 'bonk' under the hiss
       snappy - bright full-band noise that HOLDS for a moment, then fades: the signature hiss
       grit   - optional short mid noise; light saturation, no clipping."""
    n = n_of(length or max(snappy_decay + snappy_hold, tone_decay, tone_tail_decay if tone_tail_db > -59 else 0) * 1.3 + 0.06)
    t = tvec(n)
    bendc = 1.0 + bend * np.exp(-t / bend_tau)
    def osc(freq):   # sine .. triangle blend, straight from phase (no drift when bending)
        ph = np.cumsum(freq / SR)
        tri = 2.0 * np.abs(2.0 * (ph - np.floor(ph + 0.5))) - 1.0
        return drive((1 - tone_tri) * np.sin(TWO_PI * ph) + tone_tri * tri, tone_shape_db)
    def tenv(scale):   # two-stage: quick drop, then a long quiet tail
        return np.maximum(env_exp(n, tone_decay * scale, attack=0.0003), db(tone_tail_db) * env_exp(n, tone_tail_decay * scale, attack=0.0003))
    o1 = osc(tune_hz * bendc) * tenv(1.0)
    o2 = osc(tune_hz * ratio2 * bendc) * tenv(0.8)
    tone = o1 + 0.75 * o2
    nz = lp(white(n, rng), noise_lp, order=2)
    sn = hp(nz, snappy_hp, order=2) * env_exp(n, snappy_decay, attack=0.0005, hold=snappy_hold)
    gr = bp(nz, grit_hz, 0.5) * env_exp(n, grit_decay, attack=0.0005)
    ck = hp(white(n, rng), 4000) * env_exp(n, 0.0025, attack=0.0001)
    unit = lambda v: v / (np.max(np.abs(v)) + 1e-9)
    x = tone_amt * unit(tone) + snappy * unit(sn) + grit * unit(gr) + click * unit(ck)
    x = drive(unit(x), drv)
    st = widen(x, rng, width)
    if room:   # short room on the noise only (no low ring)
        wet = convolve(st, make_ir(room_decay, rng, predelay=0.006, damp_hz=8000), wet=1.0, dry=0.0)
        st = mix(st, hp(wet, 400, order=2) * room)
    if tape_db:
        st = tape(st, tape_db)
    if crush:
        st = lp(bitcrush(st, *crush), crush[1] * 0.45)
    return st


def rimshot(rng, tone=1700.0, body_hz=420.0, decay=0.045, crush=None, room=0.0):
    n = n_of(decay * 2 + 0.06)
    wood = modal(n, [tone, tone * 2.42, tone * 3.9], [decay, decay * 0.6, decay * 0.4], [1.0, 0.45, 0.2], rng)
    body = sine(pitch_drop(n, body_hz, 1.3, 0.004)) * env_exp(n, decay * 0.8)
    clk = hp(white(n, rng), 3000) * env_exp(n, 0.002)
    x = drive(wood + 0.6 * body + 0.7 * clk, 6)
    st = to_stereo(x)
    if room:
        st = convolve(st, make_ir(0.5, rng, predelay=0.005), wet=room)
    if crush:
        st = lp(bitcrush(st, *crush), crush[1] * 0.45)
    return st


def snap(rng, tone=2300.0, room=0.3):
    n = n_of(0.25)
    env = env_exp(n, 0.03, attack=0.0003)
    i = n_of(0.006)
    env[i:] = np.maximum(env[i:], 0.7 * env_exp(n - i, 0.05))
    x = bp(white(n, rng), tone, 2.0) * env + 0.3 * hp(white(n, rng), 6000) * env_exp(n, 0.004)
    return convolve(widen(x, rng, 0.2), make_ir(0.45, rng, predelay=0.004), wet=room)


# ================================================================ hats / cymbals
def metal_fm(n, base, rng, index=3.0, pairs=3, ratio=1.414):
    """inharmonic metallic spectrum from detuned FM pairs (not a square bank)."""
    mults = [1.0, 1.3428, 1.7231, 2.1879][:pairs]
    out = np.zeros(n)
    for m in mults:
        c = base * m * (1 + 0.004 * rng.standard_normal())
        out += fm(c, ratio * (1 + 0.02 * rng.standard_normal()), index, n)
    return out / pairs


def hat(rng, decay=0.05, base=3100.0, index=3.5, metal=0.6, noise_amt=0.7, hp_hz=4300.0, tone=8200.0,
        tone_db=3.0, attack=0.0004, width=0.25, crush=None, drv=2.0, length=None):
    n = n_of(length or decay * 1.4 + 0.02)
    x = metal * metal_fm(n, base, rng, index) + noise_amt * white(n, rng)
    x = hp(x, hp_hz, order=2) + 0.18 * bp(x, hp_hz * 0.7, 1.2)
    x = lp(peak(x, tone, tone_db, 0.8), 14000)
    x = x * env_exp(n, decay, attack=attack)
    x = drive(x, drv)
    st = widen(x, rng, width)
    if crush:
        st = lp(bitcrush(st, *crush), crush[1] * 0.45)
    return st


def ride(rng, base=420.0, decay=3.0, bell=0.4, shimmer=0.35, partials=90, width=0.35):
    """dense inharmonic cloud 2.5-12 kHz (the wash) + a mid 'ping' cluster set by base/bell + stick."""
    n = n_of(decay * 1.1 + 0.05)
    fr = np.exp(rng.uniform(np.log(2500), np.log(12000), partials))
    decs = decay * rng.uniform(0.35, 1.0, partials) * (fr / 3000.0) ** -0.5
    cloud = modal(n, fr, decs, rng.uniform(0.5, 1.0, partials), rng) / np.sqrt(partials)
    ping_r = np.array([2.4, 3.1, 4.6, 5.9, 7.3])
    ping = modal(n, base * ping_r * (1 + 0.01 * rng.standard_normal(5)), decay * np.array([0.6, 0.5, 0.4, 0.35, 0.3]),
                 [1.0, 0.8, 0.6, 0.45, 0.35], rng) / 2.5
    wash = hp(colored(n, rng, -2.0), 5000) * env_exp(n, decay * 0.5, attack=0.002)
    stick = hp(white(n, rng), 3000) * env_exp(n, 0.01)
    x = 1.6 * cloud + (0.35 + bell) * ping + shimmer * 0.6 * wash + 0.5 * stick
    x = lp(hp(x, 500), 14000)
    return widen(x, rng, width)


def crash(rng, decay=2.4, base=1900.0, bright_db=0.0, attack=0.002, width=0.6, dark=False):
    n = n_of(decay * 1.1 + 0.05)
    nz = colored(n, rng, -3.0)
    x = hp(nz, 700, order=2) * env_exp(n, decay, attack=attack)
    x += 0.5 * metal_fm(n, base, rng, 6.0, 4) * env_exp(n, decay * 0.75, attack=attack)
    fr = np.exp(rng.uniform(np.log(1500), np.log(9000), 60))
    x += 1.2 * modal(n, fr, decay * rng.uniform(0.4, 0.9, 60), np.ones(60), rng) / np.sqrt(60)
    x = hp(x, 400)
    x = lp(x, 6500 if dark else 12500)
    if bright_db:
        x = highshelf(x, 6000, bright_db)
    return widen(x, rng, width)


def tambourine(rng, length=0.22, hits=3, spread=0.012, jingle_lo=5500.0, jingle_hi=13000.0, skin=0.15, width=0.35):
    """jingle cloud: dense high partials that ring briefly, excited by a few micro-hits (the shake)."""
    n = n_of(length + hits * spread + 0.05)
    exc = np.zeros(n)
    t0 = 0.0
    for k in range(hits):
        i = n_of(t0)
        exc[i:] = np.maximum (exc[i:], (0.65 ** k) * env_exp(n - i, length, attack=0.0003))   # jingles ring on after each hit
        t0 += spread * (0.7 + 0.6 * rng.random())
    fr = np.exp(rng.uniform(np.log(jingle_lo), np.log(jingle_hi), 70))
    ring = modal(n, fr, length * rng.uniform(0.3, 1.0, 70), np.ones(70), rng) / np.sqrt(70)
    nz = hp(white(n, rng), 6000)
    x = (ring * 2.0 + 0.5 * nz) * exc
    x += 0.6 * nz * env_exp(n, 0.008)   # the strike itself
    x += skin * bp(white(n, rng), 900, 1.2) * env_exp(n, 0.02)
    return widen(hp(x, 2500), rng, width)


def shaker(rng, length=0.18, tone=7500.0, density=900.0, shape=((0, 0), (0.035, 1.0), (0.09, 0.45), (0.18, 0.0)), width=0.3):
    n = n_of(length + 0.02)
    grains = np.zeros(n)
    count = int(density * length)
    pos = rng.integers(0, n - 64, count)
    for p in pos:
        L = int(rng.integers(20, 60))
        grains[p:p + L] += rng.uniform(-1, 1, L) * np.hanning(L)
    x = hp(bp(grains, tone, 0.9), 4000)
    x *= env_curve(n, list(shape))
    return widen(x, rng, width)


# ================================================================ toms / tuned percussion
def tom(rng, f0=140.0, decay=0.45, drop=1.5, stick=0.4, partials=0.35, drv=4.0, room=0.0):
    n = n_of(decay * 1.3 + 0.05)
    f = pitch_drop(n, f0, drop, 0.03)
    x = sine(f) * env_exp(n, decay, attack=0.0004)
    r = membrane_ratios(6)[1:]
    x += partials * modal(n, [f0 * k for k in r], [decay * 0.4 / (i + 1) for i in range(len(r))], [0.6, 0.4, 0.3, 0.25, 0.2], rng)
    x += stick * bp(white(n, rng), 3000, 0.8) * env_exp(n, 0.006)
    x = drive(x, drv)
    st = to_stereo(x)
    if room:
        st = convolve(st, make_ir(0.8, rng, predelay=0.01), wet=room)
    return st


def conga(rng, f0=330.0, decay=0.25, slap=0.3, pan_pos=0.0):
    n = n_of(decay * 1.4 + 0.04)
    f = pitch_drop(n, f0, 1.12, 0.01)
    x = sine(f) * env_exp(n, decay, attack=0.0005)
    x += 0.4 * modal(n, [f0 * 1.594, f0 * 2.136, f0 * 2.65], [decay * 0.4, decay * 0.25, decay * 0.18], [0.5, 0.35, 0.2], rng)
    x += slap * hp(bp(white(n, rng), 2500, 0.9), 900) * env_exp(n, 0.02)
    return pan(drive(x, 3), pan_pos)


def wood(rng, f=2200.0, decay=0.07, ratios=(1.0, 2.76, 5.4), click=0.4, pan_pos=0.0, drv=2.0):
    n = n_of(decay * 2 + 0.03)
    x = modal(n, [f * r for r in ratios], [decay, decay * 0.5, decay * 0.25], [1.0, 0.4, 0.2], rng)
    x += click * hp(white(n, rng), 5000) * env_exp(n, 0.0015)
    return pan(drive(x, drv), pan_pos)


def bell_perc(rng, f=560.0, ratios=(1.0, 1.504, 2.11, 2.68), decay=0.28, bp_hz=1400.0, drv=3.0, pan_pos=0.0):
    """cowbell / agogo-style metal percussion."""
    n = n_of(decay * 1.4 + 0.03)
    x = modal(n, [f * r for r in ratios], [decay, decay * 0.7, decay * 0.5, decay * 0.4], [1.0, 0.8, 0.3, 0.2], rng)
    x = bp(x, bp_hz, 0.6) * 2 + 0.3 * x
    return pan(drive(x, drv), pan_pos)


def glass(rng, f=1250.0, decay=0.6, ratios=(1.0, 2.63, 5.12, 8.3)):
    n = n_of(decay * 1.2 + 0.03)
    x = modal(n, [f * r for r in ratios], [decay, decay * 0.5, decay * 0.3, decay * 0.2], [1.0, 0.5, 0.3, 0.15], rng, strike_noise=0.2)
    return widen(hp(x, 500), rng, 0.25)


def clang(rng, base=210.0, decay=1.4, partials=26, drv=10.0, room=0.35, crush=None):
    """industrial struck steel: stiff-plate modes + strike, then dirt and space."""
    n = n_of(decay * 1.2 + 0.05)
    r = plate_ratios(partials, rng)
    x = modal(n, base * r, decay * rng.uniform(0.3, 1.0, partials) / np.sqrt(r), rng.uniform(0.4, 1.0, partials) / np.sqrt(np.arange(1, partials + 1)), rng, strike_noise=0.8)
    x = drive(hp(x, 120), drv, "fold")
    st = widen(x, rng, 0.35)
    if room:
        st = convolve(st, make_ir(1.4, rng, predelay=0.012, damp_hz=5000), wet=room)
    if crush:
        st = lp(bitcrush(st, *crush), crush[1] * 0.45)
    return st


def blip(rng, f_start=1800.0, f_end=300.0, sweep=0.02, decay=0.08, fm_index=0.0, drv=4.0, pan_pos=0.0):
    """short synthetic tick / zap percussion (techno, tech house)."""
    n = n_of(decay * 1.5 + 0.02)
    f = f_end + (f_start - f_end) * np.exp(-tvec(n) / sweep)
    x = fm(f, 1.5, fm_index * env_exp(n, decay * 0.5), n) if fm_index else sine(f)
    x = drive(x * env_exp(n, decay, attack=0.0003), drv)
    return pan(x, pan_pos)


def noise_hit(rng, decay=0.15, hp_hz=800.0, gate=0.0, drv=14.0, room=0.3, crush=(8, 16000)):
    """industrial gated/distorted noise burst."""
    n = n_of(decay * 1.4 + 0.05)
    x = hp(colored(n, rng, -1.0), hp_hz) * env_exp(n, decay, attack=0.001)
    if gate:
        g = (np.sin(TWO_PI * gate * tvec(n)) > -0.2).astype(float)
        x *= lp(g, 400)
    x = drive(x, drv, "hard")
    x = bitcrush(x, *crush) if crush else x
    st = widen(x, rng, 0.4)
    return convolve(st, make_ir(0.9, rng, predelay=0.01), wet=room) if room else st


# ================================================================ bass / tonal one-shots
def sub_hit(rng, note="F1", decay=1.4, drv=4.0, glide_st=2.0):
    f0 = hz(note)
    n = n_of(decay * 1.2 + 0.05)
    f = f0 * 2 ** (glide_st * np.exp(-tvec(n) / 0.02) / 12)
    x = sine(f) * env_exp(n, decay, attack=0.002, hold=decay * 0.25)
    return to_stereo(drive(x, drv, "asym"))


def _sub_layer(note, n, decay_hold, lvl=0.8):
    return lvl * sine(hz(note), n) * env_curve(n, [(0, 0), (0.004, 1), (decay_hold, 1), (decay_hold + 0.08, 0)])


def growl(rng, note="F1", length=1.2, wobble_hz=4.67, vowels=("o", "a"), index=3.0, drv=16.0, sub=0.8):
    """dubstep wobble/growl one-shot: FM + formant sweep synced to an LFO + fold distortion + clean sub."""
    n = n_of(length)
    f = hz(note)
    t = tvec(n)
    lfo = 0.5 - 0.5 * np.cos(TWO_PI * wobble_hz * t)
    src = 0.6 * saw(f * 2, n) + 0.4 * fm(f * 2, 0.5, index * (0.3 + lfo), n)
    track = []
    steps = int(length * wobble_hz * 2) + 1
    for k in range(steps):
        track.append((k / (wobble_hz * 2), vowels[k % len(vowels)]))
    voc = formant_voice(src, track, rng, gender=0.9)
    x = drive(hp(voc, 120) * 3, drv, "fold")
    x = lp(x, 9000)
    x = x * env_curve(n, [(0, 0), (0.005, 1), (length - 0.06, 1), (length, 0)])
    out = widen(x, rng, 0.25)
    return mix(out, _sub_layer(note, n, length - 0.1, sub))


def yoi_stab(rng, note="F1", length=0.5, drv=20.0, sub=0.85, yoi=True):
    """riddim "yoi" bass: square through a fast o->i formant move, hard clipped."""
    n = n_of(length)
    f = hz(note) * 2
    src = square(f, n, 0.35) * 0.7 + saw(f * 1.005, n) * 0.5
    track = [(0, "o"), (length * 0.35, "i"), (length * 0.7, "u")] if yoi else [(0, "a"), (length, "a")]
    x = formant_voice(src, track, rng, gender=0.85)
    x = drive(hp(x, 110) * 4, drv, "hard")
    x = lp(x, 7000) * env_curve(n, [(0, 0), (0.003, 1), (length - 0.05, 1), (length, 0)])
    return mix(widen(x, rng, 0.15), _sub_layer(note, n, length - 0.07, sub))


def reese(rng, note="F1", length=1.6, detune=22.0, cutoff=700.0, drv=8.0, neuro=False):
    n = n_of(length)
    f = hz(note)
    t = tvec(n)
    L = saw(f * 2 ** (-detune / 1200), n, rng.random()) + 0.5 * saw(f * 2, n, rng.random())
    R = saw(f * 2 ** (detune / 1200), n, rng.random()) + 0.5 * saw(f * 2 * 1.003, n, rng.random())
    st = np.vstack([L, R])
    cut = cutoff * (1 + 0.6 * np.sin(TWO_PI * 0.35 * t))
    st = tv_filter(st, "lp", cut, 1.2, 64)
    if neuro:   # moving comb + fold: metallic neuro character
        d = (3.0 + 2.0 * np.sin(TWO_PI * 1.3 * t)) / 1000.0
        idx = np.clip(np.arange(n) - (d * SR), 0, n - 1)
        st = np.vstack([c + 0.8 * np.interp(idx, np.arange(n), c) for c in st])
        st = drive(hp(st, 90) * 2, drv + 8, "fold")
        st = tv_filter(st, "notch", 900 + 700 * np.sin(TWO_PI * 2.1 * t), 2.0, 64)
    else:
        st = drive(st, drv)
    st = hp(st, 160) + to_stereo(_sub_layer(note, n, length - 0.1, 0.7))
    return st * env_curve(n, [(0, 0), (0.006, 1), (length - 0.08, 1), (length, 0)])


def ep_chord(rng, notes=("C3", "D#3", "G3", "A#3", "D4"), decay=1.8, bright=1.0, room=0.25):
    """FM electric piano chord (deep house)."""
    n = n_of(decay * 1.1 + 0.05)
    out = np.zeros(n)
    for f in chord_hz(notes):
        idx = (2.2 * bright) * env_exp(n, 0.7) + 0.25
        tone = fm(f, 1.0, idx, n)
        tine = 0.25 * bright * fm(f, 14.0, 1.2 * env_exp(n, 0.04), n) * env_exp(n, 0.08)
        out += (tone + tine) * env_exp(n, decay * (1.2 - f / 2000), attack=0.002)
    t = tvec(n)
    trem = 1 + 0.15 * np.sin(TWO_PI * 4.5 * t)
    L, R = out * trem, out * (2 - trem)
    st = np.vstack([L, R]) / len(notes)
    return convolve(st, make_ir(1.5, rng, predelay=0.015), wet=room) if room else st


def house_piano(rng, notes=("A3", "C4", "E4", "G4"), decay=1.6, bright=1.0, room=0.25):
    """bright FM 'house piano' chord stab: body pair + bell pair, detuned double, hammer."""
    n = n_of(decay * 1.1 + 0.05)
    out = np.zeros(n)
    for f in chord_hz(notes):
        for det in (-0.004, 0.004):
            ff = f * (1 + det)
            body = fm(ff, 1.0, (5.0 * bright) * env_exp(n, 0.7) + 1.0, n)
            bell = 0.5 * bright * fm(ff * 2, 3.5, 2.0 * env_exp(n, 0.4), n) * env_exp(n, decay * 0.45)
            out += (body + bell) * env_exp(n, decay * (1.25 - f / 1800), attack=0.001)
        out += 0.15 * hp(white(n, rng), 2500) * env_exp(n, 0.006)
    st = widen(out / (2 * len(notes)), rng, 0.3)
    st = drive(st * 1.5, 3)
    return convolve(st, make_ir(1.4, rng, predelay=0.012), wet=room) if room else st


def organ_stab(rng, notes=("F3", "G#3", "C4", "D#4"), length=0.45, room=0.2):
    """90s house organ stab: drawbar additive + key click."""
    n = n_of(length + 0.1)
    out = np.zeros(n)
    for f in chord_hz(notes):
        for h, a in ((0.5, 0.5), (1, 1.0), (2, 0.7), (3, 0.5), (4, 0.4), (6, 0.2), (8, 0.2)):
            out += a * sine(f * h * (1 + 0.0007 * rng.standard_normal()), n)
    out += 0.4 * hp(white(n, rng), 3000) * env_exp(n, 0.004) * len(notes)
    out *= env_curve(n, [(0, 0), (0.003, 1), (length * 0.5, 0.8), (length, 0)])
    st = widen(drive(out / (len(notes) * 4), 6), rng, 0.3)
    return convolve(st, make_ir(1.0, rng, predelay=0.01), wet=room)


def dub_chord(rng, notes=("C3", "D#3", "G3"), decay=0.5, cutoff=1400.0, delay_s=0.3516, fb=0.5, room=0.35):
    """techno dub chord: detuned saws -> filter env -> ping-pong delay -> space."""
    n = n_of(decay + 0.05)
    out = np.zeros(n)
    for f in chord_hz(notes):
        out += supersaw(f, n, 3, 12, rng)
    fc = 250 + cutoff * env_exp(n, decay * 0.6)
    x = tv_filter(out / len(notes), "lp", fc, 1.4, 32) * env_exp(n, decay, attack=0.002)
    x = drive(x, 4)
    st = delay(x, delay_s, fb, wet=0.55, lp_hz=2500)
    st = convolve(st, make_ir(2.0, rng, predelay=0.02, damp_hz=4000), wet=room)
    return hp(st, 120)


def acid_pluck(rng, note="A1", length=0.35, res=9.0, env_hz=3200.0, drv=10.0, accent=1.0):
    n = n_of(length)
    f = hz(note) * 2
    x = saw(f, n) * 0.8 + square(f, n) * 0.2
    fc = 220 + env_hz * accent * env_exp(n, length * 0.45)
    x = tv_filter(x, "lp", fc, res, 16)
    x = drive(x * env_curve(n, [(0, 0), (0.002, 1), (length * 0.7, 0.8), (length, 0)]), drv)
    return to_stereo(hp(x, 40))


def pluck_bass(rng, note="G1", length=0.3, cutoff=1800.0, drv=6.0):
    n = n_of(length)
    f = hz(note)
    x = 0.6 * saw(f, n) + 0.5 * square(f * 0.5, n) + 0.4 * sine(f, n)
    x = tv_filter(x, "lp", 120 + cutoff * env_exp(n, length * 0.4), 2.0, 16)
    x = drive(x * env_exp(n, length, attack=0.002, hold=length * 0.3), drv)
    return to_stereo(dc_block(x))


# ================================================================ FX
def riser(rng, length=2.5, f_lo=300.0, f_hi=9000.0, tone=True, room=0.4):
    n = n_of(length)
    t = tvec(n)
    k = (t / length) ** 2
    fc = f_lo * (f_hi / f_lo) ** k
    x = tv_filter(white(n, rng), "bp", fc, 2.5, 64)
    if tone:
        x += 0.3 * supersaw(220 * 2 ** (12 * k / 12), n, 3, 20, rng) * k
    x *= env_curve(n, [(0, 0), (length * 0.9, 1), (length, 0)])
    st = widen(x, rng, 0.5)
    return convolve(st, make_ir(1.5, rng), wet=room)


def downlifter(rng, length=2.0, room=0.4):
    return convolve(widen(_down(rng, length), rng, 0.5), make_ir(1.8, rng), wet=room)


def _down(rng, length):
    n = n_of(length)
    t = tvec(n)
    fc = 8000 * (150 / 8000) ** (t / length) ** 0.6
    x = tv_filter(white(n, rng), "bp", fc, 3.0, 64) + 0.4 * sine(1200 * (60 / 1200) ** (t / length), n)
    return x * env_exp(n, length, attack=0.003)


def impact(rng, boom_hz=55.0, decay=2.5, crack=0.6, room=0.6, drv=10.0):
    n = n_of(decay * 1.1 + 0.05)
    f = pitch_drop(n, boom_hz * 0.6, 3.0, 0.08)
    boom = sine(f) * env_exp(n, decay * 0.7, attack=0.001)
    cr = crack * hp(colored(n, rng, -2.0), 600) * env_exp(n, 0.25)
    x = drive(boom + cr, drv)
    st = to_stereo(x)
    return convolve(st, make_ir(decay, rng, predelay=0.02, damp_hz=4000, width=1.0), wet=room)


def sub_drop(rng, f_hi=90.0, f_lo=28.0, length=1.8, drv=6.0):
    n = n_of(length)
    t = tvec(n)
    f = f_lo + (f_hi - f_lo) * np.exp(-t / (length * 0.35))
    x = sine(f) * env_curve(n, [(0, 0), (0.01, 1), (length * 0.7, 0.8), (length, 0)])
    return to_stereo(drive(x, drv, "asym"))


def laser(rng, f_hi=5000.0, f_lo=120.0, length=0.25, index=3.0, drv=6.0):
    n = n_of(length)
    t = tvec(n)
    f = f_lo + (f_hi - f_lo) * np.exp(-t / (length * 0.18))
    x = fm(f, 0.5, index * env_exp(n, length), n) * env_exp(n, length, attack=0.001)
    return widen(drive(x, drv), rng, 0.3)


def siren(rng, lo=650.0, hi=1150.0, rate=4.0, length=1.6, room=0.3, delay_s=0.32):
    n = n_of(length)
    t = tvec(n)
    f = lo + (hi - lo) * (0.5 - 0.5 * np.cos(TWO_PI * rate * t))
    x = 0.7 * triangle(f, n) + 0.3 * square(f, n, 0.3) * 0.5
    x = lp(x, 4000) * env_curve(n, [(0, 0), (0.02, 1), (length - 0.1, 1), (length, 0)])
    st = delay(x, delay_s, 0.45, 0.45, 3000)
    return convolve(st, make_ir(1.2, rng), wet=room)


def air_horn(rng, f=466.0, length=0.9, stabs=3):
    """sound-system air horn: detuned saws through brassy resonances, stuttered."""
    n = n_of(length)
    t = tvec(n)
    src = sum(saw(f * 2 ** (c / 1200) * (1 + 0.01 * np.sin(TWO_PI * 6 * t)), n) for c in (-14, -5, 4, 13)) / 4
    x = peak(peak(src, 1100, 9, 2), 2400, 6, 2.5)
    x = drive(hp(x, 300), 10)
    gate = np.zeros(n)
    seg = length / (stabs + 0.6)
    for k in range(stabs):
        s, e = k * seg, k * seg + seg * (0.75 if k < stabs - 1 else 1.4)
        gate += (t >= s) * (t < e)
    x *= lp(np.clip(gate, 0, 1), 150)
    return convolve(widen(x, rng, 0.3), make_ir(0.8, rng), wet=0.25)


def tape_stop_hit(rng, src, stop_s=0.7):
    return tape_stop(src, stop_s)


def reverse_crash(rng, decay=2.0):
    x = crash(rng, decay=decay)
    return fade_in(reverse(x), 40)


def crackle(rng, length=1.6, density=40.0):
    n = n_of(length)
    x = 0.05 * lp(white(n, rng), 6000)
    for p in rng.integers(0, n - 200, int(density * length)):
        L = int(rng.integers(3, 30))
        x[p:p + L] += rng.uniform(-1, 1) * np.hanning(L) * rng.uniform(0.2, 1.0)
    x = hp(x, 300)
    return widen(x * env_curve(n, [(0, 0), (0.05, 1), (length - 0.1, 1), (length, 0)]), rng, 0.4)


# ================================================================ vox (formant synthesis)
def vox(rng, word="hey", f0=210.0, length=0.4, gender=1.0, drv=4.0, room=0.2, delay_s=0.0, breath=0.08):
    """synthetic shouted / sung syllables. Not a recording: glottal source + formant bank."""
    n = n_of(length)
    t = tvec(n)
    tracks = {
        "hey":  ([(0, "e"), (0.05, "e"), (length * 0.6, "e"), (length, "i")], [(0, 1.0), (length, 0.86)], 0.06),
        "yeah": ([(0, "i"), (0.07, "e"), (length * 0.45, "a"), (length, "a")], [(0, 1.0), (length, 0.82)], 0.0),
        "ah":   ([(0, "a"), (length, "a")], [(0, 1.0), (length, 0.97)], 0.0),
        "oh":   ([(0, "o"), (length, "u")], [(0, 1.0), (length, 0.9)], 0.0),
        "oi":   ([(0, "o"), (length * 0.4, "o"), (length * 0.75, "i"), (length, "i")], [(0, 1.0), (length, 1.15)], 0.0),
        "go":   ([(0, "u"), (0.03, "o"), (length, "o")], [(0, 0.9), (0.05, 1.05), (length, 0.85)], 0.0),
        "uh":   ([(0, "uh"), (length, "uh")], [(0, 1.0), (length, 0.8)], 0.0),
    }
    vt, pt, h_len = tracks[word]
    contour = np.interp(t, [p[0] for p in pt], [p[1] for p in pt])
    vib = 1 + 0.012 * np.sin(TWO_PI * 5.5 * t) * np.clip((t - 0.12) / 0.2, 0, 1)
    src = glottal(f0 * contour * vib, rng, breath=breath)
    if h_len:   # aspirated onset ("h")
        src[:n_of(h_len)] = white(n_of(h_len), rng) * 0.5
    if word == "go":   # velar burst
        k = n_of(0.012)
        src[:k] += lp(white(k, rng), 1500) * 2
    x = formant_voice(src, vt, rng, gender)
    env = env_curve(n, [(0, 0), (0.012, 1), (length * 0.7, 0.85), (length, 0)])
    x = drive(x * env * 3, drv)
    x = compress(x, -14, 3, 3, 60, 4)
    st = widen(x, rng, 0.15)
    if delay_s:
        st = delay(st, delay_s, 0.4, 0.35, 3500)
    return convolve(st, make_ir(1.2, rng, predelay=0.02), wet=room) if room else st
