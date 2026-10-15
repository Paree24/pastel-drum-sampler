"""Synthesis + processing building blocks for the factory kit generator.

Everything works on float64 numpy arrays at SR. Mono signals are 1-D,
stereo signals are shape (2, n). Nothing here is realtime: clarity first.
"""
import numpy as np
from scipy import signal

SR = 44100
TWO_PI = 2.0 * np.pi


# ---------------------------------------------------------------- basics
def n_of(seconds):
    return max(1, int(round(seconds * SR)))


def tvec(n):
    return np.arange(n) / SR


def db(x):
    return 10.0 ** (x / 20.0)


def pad_to(x, n):
    if x.shape[-1] >= n:
        return x[..., :n]
    w = [(0, 0)] * (x.ndim - 1) + [(0, n - x.shape[-1])]
    return np.pad(x, w)


def mix(*parts):
    """Sum signals of different lengths / mono+stereo (mono is centred)."""
    n = max(p.shape[-1] for p in parts)
    stereo = any(p.ndim == 2 for p in parts)
    out = np.zeros((2, n) if stereo else n)
    for p in parts:
        p = pad_to(p, n)
        out += np.vstack([p, p]) if (stereo and p.ndim == 1) else p
    return out


def to_stereo(x):
    return x if x.ndim == 2 else np.vstack([x, x])


def mono(x):
    return x if x.ndim == 1 else x.mean(axis=0)


# ---------------------------------------------------------------- envelopes
def env_exp(n, decay, attack=0.0005, hold=0.0):
    """attack ramp, optional hold, exponential decay reaching -60 dB at `decay` s."""
    t = tvec(n)
    a = np.clip(t / max(attack, 1e-6), 0, 1) if attack > 0 else np.ones(n)
    k = np.log(1000.0) / max(decay, 1e-4)
    d = np.exp(-k * np.clip(t - attack - hold, 0, None))
    return a * d


def env_curve(n, points):
    """piecewise-linear envelope from [(time_s, value), ...]."""
    t = tvec(n)
    ts, vs = zip(*points)
    return np.interp(t, ts, vs)


def pitch_drop(n, f_end, ratio, tau, curve=1.0):
    """frequency track falling from f_end*ratio to f_end (exponential, time constant tau)."""
    t = tvec(n)
    return f_end * (1.0 + (ratio - 1.0) * np.exp(-(t / tau) ** curve))


def phase_of(freq):
    return np.cumsum(TWO_PI * np.asarray(freq) / SR)


# ---------------------------------------------------------------- oscillators
def sine(freq, n=None, phase0=0.0):
    if np.isscalar(freq):
        return np.sin(TWO_PI * freq * tvec(n) + phase0)
    return np.sin(phase_of(freq) + phase0)


def _blep(p, dt):
    out = np.zeros_like(p)
    m = p < dt
    x = p[m] / dt[m]
    out[m] = x + x - x * x - 1.0
    m = p > 1.0 - dt
    x = (p[m] - 1.0) / dt[m]
    out[m] = x * x + x + x + 1.0
    return out


def saw(freq, n=None, phase0=0.0):
    f = np.full(n, float(freq)) if np.isscalar(freq) else np.asarray(freq, dtype=float)
    dt = np.clip(f / SR, 1e-9, 0.5)
    p = (np.cumsum(dt) + phase0) % 1.0
    return 2.0 * p - 1.0 - _blep(p, dt)


def square(freq, n=None, width=0.5, phase0=0.0):
    f = np.full(n, float(freq)) if np.isscalar(freq) else np.asarray(freq, dtype=float)
    dt = np.clip(f / SR, 1e-9, 0.5)
    p = (np.cumsum(dt) + phase0) % 1.0
    p2 = (p + 1.0 - width) % 1.0
    return (2.0 * p - 1.0 - _blep(p, dt)) - (2.0 * p2 - 1.0 - _blep(p2, dt))


def triangle(freq, n=None):
    sq = square(freq, n)
    f = freq if np.isscalar(freq) else np.asarray(freq)
    tri = np.cumsum(sq) * 4.0 * (np.mean(f) / SR)
    return dc_block(tri)


def fm(carrier, mod_ratio, index, n=None, feedback=0.0):
    """2-op FM: carrier freq (scalar or track), modulator at carrier*ratio, index scalar or envelope."""
    fc = np.full(n, float(carrier)) if np.isscalar(carrier) else np.asarray(carrier, dtype=float)
    pm = phase_of(fc * mod_ratio)
    mod = np.sin(pm)
    return np.sin(phase_of(fc) + np.asarray(index) * mod)


def supersaw(freq, n, voices=5, detune_cents=18.0, rng=None):
    rng = rng or np.random.default_rng(0)
    out = np.zeros(n)
    for i in range(voices):
        c = detune_cents * ((i / max(1, voices - 1)) * 2 - 1) if voices > 1 else 0
        out += saw(freq * 2 ** (c / 1200.0), n, phase0=rng.random())
    return out / np.sqrt(voices)


# ---------------------------------------------------------------- noise
def white(n, rng):
    return rng.uniform(-1.0, 1.0, n)


def colored(n, rng, slope_db_oct=-3.0):
    """FFT-shaped noise: -3 dB/oct = pink, -6 = brown, +3 = blue."""
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1.0 / SR)
    f[0] = f[1]
    spec *= (f / 1000.0) ** (slope_db_oct / 6.0206)
    x = np.fft.irfft(spec, n)
    return x / (np.max(np.abs(x)) + 1e-12)


# ---------------------------------------------------------------- filters
def _sos(kind, fc, q=0.707, gain_db=0.0):
    fc = float(np.clip(fc, 10.0, SR * 0.49))
    w = TWO_PI * fc / SR
    cw, sw = np.cos(w), np.sin(w)
    al = sw / (2.0 * q)
    A = 10 ** (gain_db / 40.0)
    if kind == "lp":
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]; a = [1 + al, -2 * cw, 1 - al]
    elif kind == "hp":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]; a = [1 + al, -2 * cw, 1 - al]
    elif kind == "bp":
        b = [al, 0, -al]; a = [1 + al, -2 * cw, 1 - al]
    elif kind == "notch":
        b = [1, -2 * cw, 1]; a = [1 + al, -2 * cw, 1 - al]
    elif kind == "peak":
        b = [1 + al * A, -2 * cw, 1 - al * A]; a = [1 + al / A, -2 * cw, 1 - al / A]
    elif kind == "lowshelf":
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - s)]
        a = [(A + 1) + (A - 1) * cw + s, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s]
    elif kind == "highshelf":
        s = 2 * np.sqrt(A) * al
        b = [A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - s)]
        a = [(A + 1) - (A - 1) * cw + s, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s]
    else:
        raise ValueError(kind)
    b = np.array(b) / a[0]
    a = np.array(a) / a[0]
    return np.concatenate([b, a])[None, :]


def filt(x, kind, fc, q=0.707, gain_db=0.0, order=1):
    sos = np.repeat(_sos(kind, fc, q, gain_db), order, axis=0)
    return signal.sosfilt(sos, x, axis=-1)


def lp(x, fc, q=0.707, order=1): return filt(x, "lp", fc, q, order=order)
def hp(x, fc, q=0.707, order=1): return filt(x, "hp", fc, q, order=order)
def bp(x, fc, q=1.0, order=1): return filt(x, "bp", fc, q, order=order)
def peak(x, fc, gain_db, q=1.0): return filt(x, "peak", fc, q, gain_db)
def lowshelf(x, fc, gain_db): return filt(x, "lowshelf", fc, 0.707, gain_db)
def highshelf(x, fc, gain_db): return filt(x, "highshelf", fc, 0.707, gain_db)


def tv_filter(x, kind, fc_track, q=0.707, block=32):
    """time-varying biquad: coefficients follow fc_track (per sample), updated every `block` samples."""
    if x.ndim == 2:
        return np.vstack([tv_filter(x[0], kind, fc_track, q, block), tv_filter(x[1], kind, fc_track, q, block)])
    n = len(x)
    fc_track = np.broadcast_to(np.asarray(fc_track, dtype=float), (n,))
    out = np.empty(n)
    zi = np.zeros(2)
    for s in range(0, n, block):
        e = min(n, s + block)
        c = _sos(kind, fc_track[s], q)[0]
        out[s:e], zi = signal.lfilter(c[:3], c[3:], x[s:e], zi=zi)
    return out


def dc_block(x, fc=12.0):
    return hp(x, fc)


# ---------------------------------------------------------------- shaping
def drive(x, amount_db, kind="tanh"):
    g = db(amount_db)
    if kind == "tanh":
        return np.tanh(x * g) / np.tanh(g) if g > 1 else x
    if kind == "hard":
        return np.clip(x * g, -1, 1)
    if kind == "asym":   # even harmonics, warm tape/valve flavour
        y = np.tanh(x * g + 0.25) - np.tanh(0.25)
        return dc_block(y) / (np.max(np.abs(y)) + 1e-12)
    if kind == "fold":
        y = x * g
        return np.sin(np.pi / 2 * y)
    if kind == "cubic":
        y = np.clip(x * g, -1, 1)
        return 1.5 * y - 0.5 * y ** 3
    raise ValueError(kind)


def bitcrush(x, bits=12, rate=SR):
    """quantise + sample-and-hold rate reduction (vintage sampler grit)."""
    y = x
    if rate < SR:
        step = SR / rate
        idx = (np.floor(np.arange(x.shape[-1]) / step) * step).astype(int)
        y = y[..., idx]
    q = 2 ** (bits - 1)
    return np.round(y * q) / q


def tape(x, amount_db=6.0, rng=None, wow=0.0):
    """saturation + gentle HF loss + optional wow (pitch wobble) for dusty character."""
    y = drive(x, amount_db, "asym")
    y = lp(y, 14000.0)
    if wow > 0 and rng is not None:
        n = y.shape[-1]
        t = tvec(n)
        d = wow * 0.002 * SR * (np.sin(TWO_PI * 0.6 * t + rng.random() * 6) + 0.3 * np.sin(TWO_PI * 3.1 * t))
        idx = np.clip(np.arange(n) + d - d.min(), 0, n - 1)
        if y.ndim == 2:
            y = np.vstack([np.interp(idx, np.arange(n), c) for c in y])
        else:
            y = np.interp(idx, np.arange(n), y)
    return y


def compress(x, thr_db=-18.0, ratio=4.0, attack_ms=5.0, release_ms=80.0, makeup_db=0.0, knee_db=6.0):
    """feed-forward compressor, stereo-linked, peak detector."""
    det = np.max(np.abs(to_stereo(x)), axis=0)
    lvl = 20 * np.log10(det + 1e-9)
    over = lvl - thr_db
    gr = np.where(over <= -knee_db / 2, 0.0,
                  np.where(over >= knee_db / 2, over * (1 - 1 / ratio),
                           (1 - 1 / ratio) * (over + knee_db / 2) ** 2 / (2 * knee_db)))
    a = np.exp(-1.0 / (attack_ms * 1e-3 * SR))
    r = np.exp(-1.0 / (release_ms * 1e-3 * SR))
    env = np.empty_like(gr)
    e = 0.0
    for i, g in enumerate(gr):   # smoothing has to be sequential
        c = a if g > e else r
        e = c * e + (1 - c) * g
        env[i] = e
    return x * db(makeup_db - env)


def transient(x, attack_db=6.0, ms=8.0):
    """boost (or cut) the first `ms` of every onset: fast-vs-slow follower."""
    m = np.abs(mono(x))
    fast = signal.lfilter([1 - np.exp(-1 / (0.0005 * SR))], [1, -np.exp(-1 / (0.0005 * SR))], m)
    slow = signal.lfilter([1 - np.exp(-1 / (ms * 1e-3 * SR))], [1, -np.exp(-1 / (ms * 1e-3 * SR))], m)
    diff = np.clip(20 * np.log10((fast + 1e-6) / (slow + 1e-6)), 0, 24) / 24.0
    g = db(attack_db * diff)
    return x * g


# ---------------------------------------------------------------- space
def make_ir(decay, rng, predelay=0.0, damp_hz=6000.0, size=1.0, er=True, width=1.0):
    """synthetic stereo room/plate IR: early reflections + decorrelated exponential noise tail
    that darkens over time (high frequencies die first)."""
    n = n_of(decay * 1.2 + predelay + 0.05)
    t = tvec(n)
    tails = []
    for ch in range(2):
        nz = rng.standard_normal(n)
        env = np.exp(-np.log(1000.0) * np.clip(t - predelay, 0, None) / decay) * (t >= predelay)
        early = lp(nz, damp_hz * 1.6)
        late = lp(nz, damp_hz * 0.35)
        mixw = np.clip((t - predelay) / max(decay * 0.5, 1e-3), 0, 1)
        tail = (early * (1 - mixw) + late * mixw) * env
        tail *= np.clip((t - predelay) / 0.004, 0, 1)            # soft onset
        if er:
            for k in range(6):
                d = predelay + (0.004 + rng.random() * 0.03) * size
                i = int(d * SR)
                if i < n:
                    tail[i] += (0.5 - 0.06 * k) * (1 if rng.random() > 0.5 else -1)
        tails.append(tail)
    ir = np.vstack(tails)
    m = ir.mean(axis=0)
    ir = np.vstack([m + width * (ir[0] - m), m + width * (ir[1] - m)])
    return ir / (np.sqrt(np.sum(ir ** 2) / 2) + 1e-12)


def convolve(x, ir, wet=0.3, dry=1.0):
    xs = to_stereo(x)
    w = np.vstack([signal.fftconvolve(xs[0], ir[0]), signal.fftconvolve(xs[1], ir[1])])
    out = pad_to(xs * dry, w.shape[-1]) + w * wet
    return out


def delay(x, time_s, feedback=0.35, wet=0.3, lp_hz=5000.0, pingpong=True, tail_s=None):
    xs = to_stereo(x)
    d = n_of(time_s)
    reps = int(np.ceil(np.log(0.001) / np.log(max(feedback, 1e-3)))) if feedback > 0 else 1
    n = xs.shape[-1] + d * (reps + 1) if tail_s is None else xs.shape[-1] + n_of(tail_s)
    out = pad_to(xs * 1.0, n)
    tap = xs.copy()
    g = wet
    for r in range(1, reps + 1):
        tap = lp(tap, lp_hz)
        s = d * r
        if s >= n:
            break
        seg = tap * g
        if pingpong and r % 2 == 1:
            seg = seg[::-1]
        out[:, s:s + seg.shape[-1]] += seg[:, :max(0, n - s)]
        g *= feedback
    return out


def sidechain_duck(x, start_s=0.0, depth=0.85, release_s=0.18):
    """gain dip at the start (kick ducking its own reverb tail, techno rumble style)."""
    n = x.shape[-1]
    t = tvec(n)
    g = 1 - depth * np.exp(-np.clip(t - start_s, 0, None) / (release_s / 3)) * (t >= start_s)
    return x * g


# ---------------------------------------------------------------- physical models
def modal(n, freqs, decays, amps, rng=None, strike_noise=0.0):
    """sum of exponentially decaying partials (bars, plates, membranes, bells)."""
    rng = rng or np.random.default_rng(0)
    t = tvec(n)
    out = np.zeros(n)
    for f, d, a in zip(freqs, decays, amps):
        if f >= SR * 0.45:
            continue
        out += a * np.sin(TWO_PI * f * t + rng.random() * TWO_PI) * np.exp(-np.log(1000.0) * t / d)
    if strike_noise > 0:
        out += strike_noise * white(n, rng) * env_exp(n, 0.006)
    return out


def membrane_ratios(k=8):
    # circular membrane (Bessel zeros / first zero)
    r = [1.0, 1.594, 2.136, 2.296, 2.653, 2.918, 3.156, 3.501, 3.600, 3.652]
    return r[:k]


def plate_ratios(k=24, rng=None):
    # stiff plate: partials spread ~ m^2 + n^2 pattern, irregular
    rng = rng or np.random.default_rng(1)
    vals = sorted({(m * m + q * q) for m in range(1, 8) for q in range(1, 8)})
    r = np.array(vals[:k], dtype=float) / vals[0]
    return r * (1 + 0.01 * rng.standard_normal(len(r)))


def karplus(n, f, decay, rng, bright=0.5):
    """Karplus-Strong plucked string / struck bar (bright=1: no damping filter)."""
    L = max(2, int(round(SR / f)))
    y = np.zeros(n + L + 1)
    y[1:L + 1] = lp(white(L, rng), 800 + bright * 9000)
    loss = np.exp(np.log(0.001) / max(decay * f, 1.0))
    w0 = 1 - 0.5 * (1 - bright)
    w1 = 0.5 * (1 - bright)
    for i in range(L + 1, n + L + 1):
        y[i] = loss * (w0 * y[i - L] + w1 * y[i - L - 1])
    return y[L + 1:]


# ---------------------------------------------------------------- voice / formants
VOWELS = {   # F1, F2, F3, F4 (Hz) with bandwidth multipliers: chest-voice male-ish
    "a": (730, 1090, 2440, 3400),
    "e": (530, 1840, 2480, 3500),
    "i": (300, 2200, 3000, 3700),
    "o": (500, 850, 2400, 3300),
    "u": (330, 800, 2250, 3200),
    "uh": (640, 1190, 2390, 3300),
}


def glottal(f0_track, rng, breath=0.08, jitter=0.004):
    """band-limited pulse-ish source: saw with spectral tilt + aspiration noise."""
    n = len(f0_track)
    f = f0_track * (1 + jitter * colored(n, rng, -6.0))
    src = saw(f, n) * 0.6 + square(f, n, 0.1) * 0.4
    src = lp(src, 3500, 0.5)
    return src + breath * white(n, rng)


def formant_voice(src, vowel_track, rng, gender=1.0):
    """vowel_track: list of (time_s, vowel). Formants glide linearly between vowels."""
    n = len(src)
    t = tvec(n)
    ts = [p[0] for p in vowel_track]
    out = np.zeros(n)
    gains = [1.0, 0.5, 0.25, 0.12]
    for k in range(4):
        fk = np.interp(t, ts, [VOWELS[v][k] * gender for _, v in vowel_track])
        bw_q = [6.0, 9.0, 12.0, 14.0][k]
        out += gains[k] * tv_filter(src, "bp", fk, q=bw_q, block=64)
    return out


# ---------------------------------------------------------------- utilities
def fade_out(x, ms=10.0):
    n = x.shape[-1]
    k = min(n, n_of(ms / 1000.0))
    w = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, k))
    y = x.copy()
    y[..., n - k:] *= w
    return y


def fade_in(x, ms=0.5):
    n = x.shape[-1]
    k = min(n, max(1, n_of(ms / 1000.0)))
    y = x.copy()
    y[..., :k] *= np.linspace(0, 1, k)
    return y


def trim(x, thr_db=-70.0, tail_ms=8.0):
    """cut the silent tail (relative to peak) and fade the last ms to true zero."""
    m = np.max(np.abs(to_stereo(x)), axis=0)
    pk = m.max() + 1e-12
    idx = np.where(m > pk * db(thr_db))[0]
    end = idx[-1] if len(idx) else len(m) - 1
    end = min(x.shape[-1], end + n_of(tail_ms / 1000.0))
    return fade_out(x[..., :end], tail_ms)


def normalize(x, peak_db=-0.5):
    return x * (db(peak_db) / (np.max(np.abs(x)) + 1e-12))


def widen(x, rng, amount=0.3, ms=0.0):
    """decorrelate a mono-ish sound into stereo (short allpass-ish smear per side)."""
    m = mono(x)
    sides = []
    for ch in range(2):
        d = n_of((ms or (0.3 + rng.random() * 0.9)) / 1000.0)
        side = np.concatenate([np.zeros(d), m])[:len(m)]
        side = hp(side, 300)
        sides.append(side)
    s = (sides[0] - sides[1]) * amount
    return np.vstack([m + s, m - s])


def pan(x, p):
    """p -1..1, equal power."""
    m = mono(x)
    a = (p + 1) * np.pi / 4
    return np.vstack([m * np.cos(a) * np.sqrt(2), m * np.sin(a) * np.sqrt(2)])


def reverse(x):
    return x[..., ::-1].copy()


def resample(x, ratio):
    """play faster (ratio > 1) or slower: pitch + time change, like a sampler."""
    n = x.shape[-1]
    m = int(n / ratio)
    src = np.arange(m) * ratio
    if x.ndim == 2:
        return np.vstack([np.interp(src, np.arange(n), c) for c in x])
    return np.interp(src, np.arange(n), x)


def tape_stop(x, stop_s=0.6):
    """slow the playback to a halt (classic dubstep/jungle tape stop)."""
    n = x.shape[-1]
    m = n_of(stop_s)
    rate = np.linspace(1, 0, m) ** 1.5
    pos = np.cumsum(rate)
    pos = pos[pos < n - 1]
    if x.ndim == 2:
        return np.vstack([np.interp(pos, np.arange(n), c) for c in x])
    return np.interp(pos, np.arange(n), x)


def write_wav24(path, x):
    """24-bit PCM WAV (stereo or mono)."""
    import struct
    xs = np.atleast_2d(x)
    ch, n = xs.shape
    q = np.clip(np.round(xs.T * 8388607.0), -8388608, 8388607).astype("<i4")
    raw = q.reshape(-1).view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(raw)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, ch, SR, SR * ch * 3, ch * 3, 24))
        f.write(b"data" + struct.pack("<I", len(raw)) + raw)
