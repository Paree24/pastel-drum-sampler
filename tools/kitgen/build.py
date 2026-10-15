"""Render the factory kits: WAVs into <factory>/Samples/<Kit>/ + kits.json
manifest (FactoryBuild turns the manifest into sampler kit XMLs).

    tools/.venv/bin/python tools/kitgen/build.py Factory [--only 3]

Deterministic: every file has a fixed seed, so rebuilding gives identical audio.
"""
import argparse
import json
import os
import sys

import numpy as np
import soundfile as sf

sys.path.insert(0, os.path.dirname(__file__))
from dsp import *            # noqa: E402,F401,F403
import kits as K             # noqa: E402

VOICE = {"poly": 0, "mono": 1, "gated": 2}


def post_chain(x, post):
    for step in post:
        kind = step[0]
        if kind == "crush":
            x = lp(bitcrush(x, step[1], step[2]), step[2] * 0.45)
        elif kind == "tape":
            x = tape(x, step[1])
        elif kind == "drive":
            x = drive(x, step[1])
        elif kind == "gate":   # gated-reverb chop
            x = fade_out(x[..., :n_of(step[1])], 15)
        else:
            raise ValueError(kind)
    return x


def finish(x):
    x = to_stereo(np.asarray(x, dtype=float))
    x = np.vstack([dc_block(c) for c in x])
    x = trim(x, -70.0, 8.0)
    if not np.all(np.isfinite(x)):
        raise RuntimeError("non-finite audio")
    return normalize(x, -0.5)


def jittered(fn, kw, rng, jit):
    table = jit if jit is not None else K.JIT.get(getattr(fn, "__name__", ""), {})
    out = dict(kw)
    for k, pct in table.items():
        if k in out and isinstance(out[k], (int, float)) and not isinstance(out[k], bool):
            out[k] = out[k] * (1 + pct * rng.uniform(-1, 1))
    return out


BANDS = [(20, 120), (120, 400), (400, 2000), (2000, 6000), (6000, 20000)]   # sub low mid hi air
BAND_NAMES = ["sub", "low", "mid", "hi", "air"]


def stats(x):
    m = mono(x)
    n = len(m)
    seg = np.concatenate([np.zeros(n_of(0.04)), m[:n_of(0.4)], np.zeros(n_of(0.04))])  # keep the onset inside the window
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / SR)
    tot = spec.sum() + 1e-12
    bands = [10 * np.log10(spec[(f >= a) & (f < b)].sum() / tot + 1e-12) for a, b in BANDS]
    rms = float(np.sqrt(np.mean(m ** 2)) + 1e-12)
    side = float(np.sqrt(np.mean(((x[0] - x[1]) / 2) ** 2)))
    return dict(len_ms=n / SR * 1000, bands=bands, crest_db=20 * np.log10(np.max(np.abs(m)) / rms),
                dc=float(abs(np.mean(m))), width=side / (rms + 1e-12), end=float(np.max(np.abs(x[:, -8:]))))


def role_check(name, bands):
    """the strongest band must suit the pad's role (catches broken recipes, allows genre colour)."""
    nm = name.lower()
    top = BAND_NAMES[int(np.argmax(bands))]
    if any(k in nm for k in ("kick", "boom")) or nm.startswith("sub"):
        ok = top in ("sub", "low")
    elif "hat" in nm or "shaker" in nm:
        ok = top in ("hi", "air")
    elif "ride" in nm or "crash" in nm:
        ok = top in ("mid", "hi", "air")
    elif any(k in nm for k in ("snare", "clap", "snap")):
        ok = top != "sub"   # electronic snares often peak in the air band (reference snares do too)
    else:
        ok = True
    return "" if ok else f" ROLE?(top band {top})"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("factory")
    ap.add_argument("--only", type=int, default=0)
    args = ap.parse_args()
    root = os.path.abspath(args.factory)
    manifest = {"kits": []}
    warnings = []
    for kit in K.KITS:
        if args.only and kit["index"] != args.only:
            continue
        kdir = os.path.join(root, "Samples", kit["name"])
        os.makedirs(kdir, exist_ok=True)
        for f in os.listdir(kdir):
            if f.endswith((".wav", ".flac")):
                os.remove(os.path.join(kdir, f))
        entry = {"index": kit["index"], "name": kit["name"], "pads": []}
        print(f"\n=== {kit['index']:02d} {kit['name']}")
        for pi, pad in enumerate(kit["pads"]):
            renders = []
            base_seed = kit["index"] * 10007 + pi * 101
            if pad.layers:
                for li, (fn, kw, post) in enumerate(pad.layers):
                    rng = np.random.default_rng(base_seed + li)
                    renders.append((f"L{li + 1}", post_chain(fn(rng, **kw), post)))
            else:
                for v in range(pad.var):
                    rng = np.random.default_rng(base_seed + v)
                    kw = pad.kw if v == 0 else jittered(pad.fn, pad.kw, rng, pad.jit)
                    renders.append((f"V{v + 1}" if pad.var > 1 else "", post_chain(pad.fn(rng, **kw), pad.post)))
            files = []
            for tag, x in renders:
                x = finish(x)
                st = stats(x)
                flag = role_check(pad.name, st["bands"])
                if st["dc"] > 0.01:
                    flag += " DC?"
                if st["end"] > 1e-3:
                    flag += " CLICK-END?"
                if st["len_ms"] < 15:
                    flag += " TOO-SHORT?"
                fn = f"{pi + 1:02d} {pad.name}{(' ' + tag) if tag else ''}.flac"
                sf.write(os.path.join(kdir, fn), x.T, SR, subtype="PCM_24")   # lossless, ~half of WAV
                files.append(os.path.join("Samples", kit["name"], fn))
                print(f"  {fn:34s} {st['len_ms']:7.0f} ms  bands {' '.join(f'{v:6.1f}' for v in st['bands'])}  crest {st['crest_db']:5.1f} dB  width {st['width']:.2f}{flag}")
                if flag:
                    warnings.append(f"{kit['name']} / {fn}:{flag}")
            params = {"level": pad.level}
            params.update(pad.params)
            entry["pads"].append({"name": pad.name, "files": files, "mode": "stack" if pad.mode == "stack" else "rr",
                                  "choke": pad.choke, "voice": VOICE[pad.voice], "params": params})
        manifest["kits"].append(entry)
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "kits.json")  # build intermediate, not shipped
    if args.only and os.path.exists(path):   # merge a partial rebuild into the existing manifest
        old = json.load(open(path))
        keep = [k for k in old["kits"] if k["index"] != args.only]
        manifest["kits"] = sorted(keep + manifest["kits"], key=lambda k: k["index"])
    json.dump(manifest, open(path, "w"), indent=1)
    print(f"\nmanifest: {path}")
    print("warnings:" if warnings else "no warnings")
    for w in warnings:
        print("  " + w)


if __name__ == "__main__":
    main()
