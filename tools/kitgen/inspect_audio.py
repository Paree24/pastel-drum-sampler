"""Dev helper: band-energy table + spectrogram/waveform sheet for rendered WAVs.
    tools/.venv/bin/python tools/kitgen/inspect_audio.py <dir> <out.png>"""
import os, sys, wave
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

BANDS = [(20, 120, "sub"), (120, 400, "low"), (400, 2000, "mid"), (2000, 6000, "hi"), (6000, 20000, "air")]

def read(path):
    if path.endswith(".flac"):
        import soundfile as sf
        x, sr = sf.read(path, always_2d=True)
        return x.T, sr
    w = wave.open(path)
    n, ch, sw, sr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = np.frombuffer(w.readframes(n), dtype=np.uint8).reshape(-1, sw)
    full = np.zeros((raw.shape[0], 4), dtype=np.uint8); full[:, 4 - sw:] = raw
    x = full.view("<i4").reshape(-1).astype(float) / 2 ** 31
    return x.reshape(-1, ch).T, sr

def bands(m, sr):
    seg = np.concatenate([np.zeros(int(sr * 0.05)), m[: int(sr * 0.4)], np.zeros(int(sr * 0.05))])  # onset away from the window edge
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    tot = spec.sum() + 1e-12
    return [10 * np.log10(spec[(f >= a) & (f < b)].sum() / tot + 1e-12) for a, b, _ in BANDS]

files = sorted(f for f in os.listdir(sys.argv[1]) if f.endswith((".wav", ".flac")))
print(f"{'file':34s}" + "".join(f"{b[2]:>7s}" for b in BANDS))
cols = 4
rows = (len(files) + cols - 1) // cols
fig, axes = plt.subplots(rows, cols, figsize=(cols * 4.2, rows * 2.3))
for ax, fn in zip(axes.flat, files):
    x, sr = read(os.path.join(sys.argv[1], fn))
    m = x.mean(axis=0)
    print(f"{fn:34s}" + "".join(f"{v:7.1f}" for v in bands(m, sr)))
    ax.specgram(m + 1e-9, NFFT=512, Fs=sr, noverlap=384, cmap="magma", vmin=-120)
    ax.set_yscale("symlog", linthresh=200); ax.set_ylim(30, 20000)
    t = np.arange(len(m)) / sr
    ax2 = ax.twinx(); ax2.plot(t, m, color="cyan", lw=0.3, alpha=0.6); ax2.set_ylim(-1, 1); ax2.set_yticks([])
    ax.set_title(fn, fontsize=8); ax.tick_params(labelsize=6)
for ax in list(axes.flat)[len(files):]:
    ax.axis("off")
plt.tight_layout(); plt.savefig(sys.argv[2], dpi=70)
