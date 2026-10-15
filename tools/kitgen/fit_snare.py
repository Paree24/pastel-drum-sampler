"""Dev helper: fit the snare recipe's parameters to reference snares by
matching broad statistics only (envelopes per band + octave balance).
No audio from the references is used in any generated sample.
    tools/.venv/bin/python tools/kitgen/fit_snare.py [--recipe snare|snare_snappy] ref1.wav [ref2.wav ...]"""
import os, sys
import numpy as np, soundfile as sf
from scipy import signal
sys.path.insert(0, os.path.dirname(__file__))
from dsp import SR, n_of
import sounds

TIMES = (0.0, 0.01, 0.025, 0.05, 0.1, 0.2, 0.3)
OCT = [(120, 240), (240, 480), (480, 960), (960, 1920), (1920, 3840), (3840, 7680), (7680, 15360)]
LO = signal.butter(4, 300, "low", fs=SR, output="sos")
HI = signal.butter(4, 3000, "high", fs=SR, output="sos")


def features(m, sr=SR):
    m = np.asarray(m, float)
    pk = np.max(np.abs(m)) + 1e-12
    on = int(np.argmax(np.abs(m) > pk * 0.05))
    m = np.concatenate([m[on:], np.zeros(int(0.6 * sr))])   # short references: silence past the end
    w = int(0.006 * sr)
    def env(v):
        return [20 * np.log10(np.sqrt(np.mean(v[int(t * sr):int(t * sr) + w] ** 2) + 1e-12) / pk) for t in TIMES]
    seg = np.concatenate([np.zeros(2048), m[:int(0.5 * sr)], np.zeros(2048)])
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    fr = np.fft.rfftfreq(len(seg), 1 / sr)
    tot = sp.sum()
    bands = [10 * np.log10(sp[(fr >= a) & (fr < b)].sum() / tot + 1e-12) for a, b in OCT]
    f = np.array(env(m) + env(signal.sosfilt(LO, m)) + env(signal.sosfilt(HI, m)) + bands)
    return np.maximum(f, -60.0)   # below -60 dB nobody hears the difference


def load(path):
    x, sr = sf.read(path, always_2d=True)
    assert sr == SR
    return x.mean(axis=1)


# search spaces: (name, lo, hi)
SPACES = {}
SPACES["snare"] = [("body_hz", 150, 240), ("pitch_hz", 250, 900), ("pitch_tau", 0.002, 0.012), ("body_decay", 0.06, 0.3), ("body_attack", 0.0003, 0.025),
         ("body_drv", 0, 14), ("noise_amt", 0.3, 2.5), ("noise_slope", -4.0, 1.0), ("noise_hp", 150, 1200),
         ("noise_attack", 0.0005, 0.07), ("click", 0.0, 1.5), ("burst_decay", 0.03, 0.25), ("tail_db", -24, -4), ("tail_decay", 0.15, 0.8), ("snap", 0.0, 1.5),
         ("snap_hz", 900, 4000), ("sat_db", 0, 10), ("clip_db", 0, 8), ("room", 0.0, 0.5)]
SPACES["snare_snappy"] = [("tune_hz", 170, 330), ("ratio2", 1.15, 1.9), ("bend", 0.0, 0.5), ("bend_tau", 0.002, 0.02),
         ("tone_decay", 0.05, 0.45), ("tone_amt", 0.2, 1.6), ("tone_tail_db", -40, -10), ("tone_tail_decay", 0.15, 0.8), ("tone_tri", 0.0, 1.0), ("tone_shape_db", 0, 10),
         ("snappy", 0.3, 2.0), ("snappy_hold", 0.0, 0.09), ("snappy_decay", 0.06, 0.4), ("snappy_hp", 600, 4000),
         ("noise_lp", 6000, 19000), ("grit", 0.0, 1.5), ("grit_hz", 500, 3000), ("grit_decay", 0.02, 0.15),
         ("click", 0.0, 1.0), ("drv", 0.0, 10.0)]
RECIPE = "snare"
SPACE = SPACES[RECIPE]


def render(p):
    return getattr(sounds, RECIPE)(np.random.default_rng(7), **p).mean(axis=0)


def fit(target, iters=1200, seed=0):
    rng = np.random.default_rng(seed)
    best = {k: (lo + hi) / 2 for k, lo, hi in SPACE}
    best_d = np.mean(np.abs(features(render(best)) - target))
    step = 0.35
    for it in range(iters):
        cand = dict(best)
        for k, lo, hi in SPACE:
            if rng.random() < 0.35:
                cand[k] = float(np.clip(cand[k] + rng.normal() * step * (hi - lo), lo, hi))
        d = np.mean(np.abs(features(render(cand)) - target))
        if d < best_d:
            best, best_d = cand, d
        if it % 250 == 249:
            step *= 0.6
    return best, best_d


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--recipe":
        RECIPE = args[1]; SPACE = SPACES[RECIPE]; args = args[2:]
    # --fix name=value ... : pin parameters measured directly (e.g. partial frequencies)
    FIXED = {}
    while args and args[0] == "--fix":
        k, v = args[1].split("="); FIXED[k] = float(v); args = args[2:]
    if FIXED:
        SPACE = [sp for sp in SPACE if sp[0] not in FIXED]
        _render = render
        render = lambda p: _render({**p, **FIXED})
    for path in args:
        tgt = features(load(path))
        p, d = fit(tgt)
        got = features(render(p))
        print(f"\n{os.path.basename(path)}: mean |feature error| {d:.2f} dB")
        print("  params:", {k: round(v, 4) for k, v in {**p, **FIXED}.items()})
        lab = [f"env{int(t*1000)}" for t in TIMES] + [f"lo{int(t*1000)}" for t in TIMES] + [f"hi{int(t*1000)}" for t in TIMES] + [f"oct{a}" for a, b in OCT]
        print("  worst:", sorted(zip(np.abs(got - tgt), lab, tgt.round(1), got.round(1)), reverse=True)[:6])
