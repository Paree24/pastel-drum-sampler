#include "SampleEngine.h"

namespace mds {

// ---------------- parameter tables ----------------
static const PadParamMeta kPadMeta[NumPadParams] = {
    // key          label          min      max        def        centre   fmt     env
    { "tune",       "TUNE",        -24.f,   24.f,      0.f,       0.f,     "st",   false },
    { "level",      "LEVEL",       -60.f,   6.f,       0.f,       0.f,     "dB",   false },
    { "pan",        "PAN",         -100.f,  100.f,     0.f,       0.f,     "pan",  false },
    { "attack",     "ATTACK",      0.f,     1000.f,    0.f,       40.f,    "ms",   true  },
    { "hold",       "HOLD",        0.f,     HoldFullMs, HoldFullMs, 300.f, "hold", true  },
    { "decay",      "DECAY",       5.f,     5000.f,    400.f,     300.f,   "ms",   true  },
    { "release",    "RELEASE",     5.f,     5000.f,    150.f,     300.f,   "ms",   true  },
    { "pitchamt",   "P ENV",       -48.f,   48.f,      0.f,       0.f,     "st",   false },
    { "pitchdecay", "P DECAY",     1.f,     2000.f,    80.f,      120.f,   "ms",   false },
    { "pitchcurve", "P CURVE",     0.f,     100.f,     35.f,      0.f,     "%",    false },
    { "cutoff",     "CUTOFF",      20.f,    20000.f,   20000.f,   640.f,   "Hz",   false },
    { "reso",       "RESO",        0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "fltenv",     "F ENV",       -100.f,  100.f,     0.f,       0.f,     "%",    false },
    { "fltdecay",   "F DECAY",     1.f,     5000.f,    200.f,     200.f,   "ms",   false },
    { "drive",      "DRIVE",       0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "comp",       "COMP",        0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "tone",       "TONE",        -100.f,  100.f,     0.f,       0.f,     "%",    false },
    { "punch",      "PUNCH",       0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "velamp",     "VEL>AMP",     0.f,     100.f,     70.f,      0.f,     "%",    false },
    { "velstart",   "VEL>START",   0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "velfilt",    "VEL>FILT",    0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "randpitch",  "RAND PITCH",  0.f,     100.f,     0.f,       0.f,     "ct",   false },
    { "sendrev",    "SEND REV",    0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "senddly",    "SEND DLY",    0.f,     100.f,     0.f,       0.f,     "%",    false },
    { "length",     "LENGTH",      -100.f,  100.f,     0.f,       0.f,     "%",    true  },
};

const PadParamMeta& padParamMeta (int idx) { return kPadMeta[juce::jlimit (0, NumPadParams - 1, idx)]; }

juce::NormalisableRange<float> padParamRange (int idx) {
    auto& m = padParamMeta (idx);
    juce::NormalisableRange<float> r (m.min, m.max);
    if (m.centre > 0.f) r.setSkewForCentre (m.centre);
    return r;
}

juce::String padParamID (int pad, int idx) { return "p" + juce::String (pad) + "_" + padParamMeta (idx).key; }
juce::String padMacroID (int pad, int k)   { return "p" + juce::String (pad) + "_m" + juce::String (k); }
juce::String padFilterModeID (int pad)     { return "p" + juce::String (pad) + "_fmode"; }

static juce::String fmtMs (float v) {
    if (v < 10.f) return juce::String (v, 1) + " ms";
    if (v < 1000.f) return juce::String (juce::roundToInt (v)) + " ms";
    return juce::String (v / 1000.f, 2) + " s";
}
static juce::String fmtPan (float v) {
    int p = juce::roundToInt (v);
    if (p == 0) return "C";
    return (p < 0 ? "L" : "R") + juce::String (std::abs (p));
}
static juce::String fmtDb (float v) {
    if (v <= -59.5f) return "-inf";
    return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
}

juce::String formatPadParam (int idx, float v) {
    auto& m = padParamMeta (idx);
    juce::String f = m.fmt;
    if (f == "st") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st";
    if (f == "dB") return fmtDb (v);
    if (f == "pan") return fmtPan (v);
    if (f == "ms") return fmtMs (v);
    if (f == "hold") return v >= HoldFullMs - 0.5f ? juce::String ("FULL") : fmtMs (v);
    if (f == "Hz") return v < 1000.f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.f, 1) + " kHz";
    if (f == "ct") return juce::String::charToString (0x00b1) + juce::String (juce::roundToInt (v)) + " ct";
    int pc = juce::roundToInt (v);
    return (m.min < 0.f && pc > 0 ? "+" : "") + juce::String (pc) + "%";
}

static const LayerParamMeta kLayerMeta[NumLayerParams] = {
    { "tune",  "TUNE",  -24.f,  24.f,  0.f, "st"  },
    { "fine",  "FINE",  -100.f, 100.f, 0.f, "ct"  },
    { "start", "START", 0.f,    1.f,   0.f, "pos" },
    { "end",   "END",   0.f,    1.f,   1.f, "pos" },
    { "level", "LEVEL", -60.f,  6.f,   0.f, "dB"  },
    { "pan",   "PAN",   -100.f, 100.f, 0.f, "pan" },
};
const LayerParamMeta& layerParamMeta (int idx) { return kLayerMeta[juce::jlimit (0, NumLayerParams - 1, idx)]; }

juce::String formatLayerParam (int idx, float v, double lengthSec) {
    switch (idx) {
        case LP_Tune:  return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " st";
        case LP_Fine:  return (v > 0.5f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct";
        case LP_Start:
        case LP_End:   return lengthSec > 0.0 ? fmtMs ((float) (v * lengthSec * 1000.0)) : juce::String (juce::roundToInt (v * 100.f)) + "%";
        case LP_Level: return fmtDb (v);
        default:       return fmtPan (v);
    }
}

// ---------------- sample analysis ----------------
void SampleData::analyse() {
    const int n = length(), nch = channels();
    peak = 0.f;
    for (int c = 0; c < nch; ++c) peak = juce::jmax (peak, buf.getMagnitude (c, 0, n));
    ovMin.assign ((size_t) OverviewSize, 0.f);
    ovMax.assign ((size_t) OverviewSize, 0.f);
    if (n <= 0) return;
    for (int b = 0; b < OverviewSize; ++b) {
        int s0 = (int) ((juce::int64) b * n / OverviewSize);
        int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) (b + 1) * n / OverviewSize));
        float lo = 0.f, hi = 0.f;
        for (int i = s0; i < s1 && i < n; ++i) {
            float v = 0.f;
            for (int c = 0; c < nch; ++c) v += buf.getSample (c, i);
            v /= (float) nch;
            lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
        }
        ovMin[(size_t) b] = lo; ovMax[(size_t) b] = hi;
    }
}

// ---------------- voice ----------------
static float coefFor (float ms, float sr) { // multiplier reaching -60 dB after ms
    return std::exp (std::log (0.001f) / juce::jmax (1.f, ms * 0.001f * sr));
}

void SampleVoice::start (const VoiceStart& s, juce::uint32 stamp) {
    data = s.data;
    if (data == nullptr || data->length() < 4) { finish(); return; }
    pad = s.pad; layer = s.layer; age = stamp;
    const double L = data->length();
    double s0 = juce::jlimit (0.0, 1.0, (double) s.startFrac) * L;
    double s1 = juce::jlimit (0.0, 1.0, (double) s.endFrac) * L;
    if (s1 < s0 + 16.0) s1 = juce::jmin (L, s0 + 16.0);
    if (s1 < s0 + 16.0) s0 = juce::jmax (0.0, s1 - 16.0);
    regionStart = s0; len = s1 - s0;
    regionStartFrac = (float) (s0 / L); regionEndFrac = (float) (s1 / L);
    reverse = s.reverse;
    pos = juce::jlimit (0.0, juce::jmax (0.0, len - 16.0), s.startOffsetSec * data->sampleRate);
    baseInc = data->sampleRate / (double) hostSr * std::exp2 ((double) s.semis / 12.0);
    gL = s.gainL; gR = s.gainR;
    // region edges that cut into the waveform get a ~0.7 ms fade-in (no click);
    // an untouched forward start keeps the transient intact
    fadeIn = (pos > 0.0 || s0 > 0.0 || reverse) ? 32.f : 0.f;

    gated = s.gated;
    if (s.attackMs < 0.05f) { env = 1.f; stage = Hold; atkInc = 1.f; }
    else { env = 0.f; stage = Attack; atkInc = 1.f / juce::jmax (1.f, s.attackMs * 0.001f * hostSr); }
    holdFull = s.holdMs >= HoldFullMs - 0.5f;
    holdLeft = (int) (s.holdMs * 0.001f * hostSr);
    decCoef = coefFor (s.decayMs, hostSr);
    relCoef = coefFor (s.releaseMs, hostSr);
    chokeGain = 1.f; chokeStep = 0.f;

    pitchOn = std::abs (s.pitchAmt) > 0.01f;
    if (pitchOn) penv.trigger (s.pitchAmt, s.pitchDecayMs * 0.001f, s.pitchCurve);

    fMode = s.filterMode;
    baseCut = s.cutoff * std::exp2 (-s.velFiltOct);
    q = 0.707f + s.reso * s.reso * 14.f;
    fEnvOct = s.fltEnv * 6.f;
    fEnv = 1.f; fEnvCoef = coefFor (s.fltDecayMs, hostSr);
    useFilter = fMode != 0 || baseCut < 19000.f || (fEnvOct < -0.01f) || s.reso > 0.01f;
    fL.reset(); fR.reset(); filterTick = 0;
    curCut = juce::jlimit (20.f, 20000.f, baseCut * std::exp2 (fEnvOct));
    lenFadeEnd = s.lengthScale < 0.999f ? len * (double) juce::jlimit (0.01f, 1.f, s.lengthScale) : 0.0;
    lenFadeStart = lenFadeEnd * 0.35;   // gentle fade over the last 65 % of the shortened sound
    active = true;
}

void SampleVoice::noteOff() {
    if (active && gated && stage != Release && stage != Done) stage = Release;
}

void SampleVoice::choke (float ms) {
    if (! active) return;
    float n = juce::jmax (1.f, ms * 0.001f * hostSr);
    float step = chokeGain / n;
    chokeStep = juce::jmax (chokeStep, step);
}

float SampleVoice::readAt (int ch, double idx) const {
    const auto& b = data->buf;
    const int n = b.getNumSamples();
    const int c = juce::jmin (ch, b.getNumChannels() - 1);
    const float* d = b.getReadPointer (c);
    int i1 = (int) std::floor (idx);
    float fr = (float) (idx - i1);
    auto at = [d, n] (int i) { return d[juce::jlimit (0, n - 1, i)]; };
    float y0 = at (i1 - 1), y1 = at (i1), y2 = at (i1 + 1), y3 = at (i1 + 2);
    // 4-point Hermite
    float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * fr + c2) * fr + c1) * fr + y1;
}

void SampleVoice::render (float& outL, float& outR) {
    if (! active) return;
    if (pos >= len) { finish(); return; }

    // ---- envelope ----
    switch (stage) {
        case Attack:  env += atkInc; if (env >= 1.f) { env = 1.f; stage = Hold; } break;
        case Hold:    if (! holdFull && --holdLeft <= 0) stage = Decay; break;
        case Decay:   env *= decCoef; if (env < 0.0005f) { finish(); return; } break;
        case Release: env *= relCoef; if (env < 0.0005f) { finish(); return; } break;
        default: break;
    }
    if (chokeStep > 0.f) {
        chokeGain -= chokeStep;
        if (chokeGain <= 0.f) { finish(); return; }
    }

    // ---- read ----
    double src = reverse ? regionStart + len - 1.0 - pos : regionStart + pos;
    float a = readAt (0, src);
    float b = data->channels() > 1 ? readAt (1, src) : a;

    float edge = 1.f;
    if (fadeIn > 0.f && pos < fadeIn) edge = (float) (pos / fadeIn);
    double remain = len - pos;
    if (remain < 64.0) edge *= (float) (remain / 64.0); // never end on a step
    if (lenFadeEnd > 0.0 && pos > lenFadeStart) {
        if (pos >= lenFadeEnd) { finish(); return; }
        float k = (float) (1.0 - (pos - lenFadeStart) / (lenFadeEnd - lenFadeStart));
        edge *= k * k;
    }

    // ---- filter ----
    if (useFilter) {
        if (fEnvOct != 0.f) fEnv *= fEnvCoef;
        if ((filterTick++ & 7) == 0)
            curCut = juce::jlimit (20.f, 20000.f, baseCut * std::exp2 (fEnvOct * fEnv));
        a = fL.process (a, curCut, q, fMode);
        b = fR.process (b, curCut, q, fMode);
    }

    float g = env * chokeGain * edge;
    outL += a * g * gL;
    outR += b * g * gR;

    // ---- advance ----
    double inc = baseInc;
    if (pitchOn) inc *= std::exp2 ((double) penv.next() / 12.0);
    pos += inc;
}

// ---------------- pad strip ----------------
void PadDsp::prepare (float s) {
    sr = s;
    auto co = [this] (float sec) { return std::exp (-1.f / (sec * sr)); };
    tiltC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 700.f / sr);
    cA = co (0.003f); cR = co (0.12f);
    pFa = co (0.0002f); pFr = co (0.03f);
    pSa = co (0.015f);  pSr = co (0.03f);
    pGc = co (0.0005f);
    sFr = co (0.015f); sSr = co (0.35f);
    reset();
}

void PadDsp::process (float& l, float& r, float drive, float comp, float tone, float punch, float sustain) {
    if (drive > 0.001f) {
        // peaks stay near their level, the body comes up: classic saturation loudness
        float g = 1.f + drive * drive * 15.f;
        float norm = 0.7f / std::tanh (0.7f * g);
        l = std::tanh (l * g) * norm;
        r = std::tanh (r * g) * norm;
    }
    if (comp > 0.001f) {
        float thr = -6.f - comp * 30.f, ratio = 1.f + comp * 7.f;
        float inDb = 20.f * std::log10 (juce::jmax (std::abs (l), std::abs (r)) + 1e-6f);
        float target = juce::jmax (0.f, inDb - thr) * (1.f - 1.f / ratio);
        float c = target > compEnv ? cA : cR;
        compEnv = c * compEnv + (1.f - c) * target;
        float gg = juce::Decibels::decibelsToGain (-compEnv + comp * 10.f);
        l *= gg; r *= gg;
    }
    if (std::abs (tone) > 0.005f) {
        // +-9 dB see-saw around 700 Hz
        float gh = juce::Decibels::decibelsToGain (tone * 9.f), gl = 1.f / gh;
        tiltL += tiltC * (l - tiltL);
        tiltR += tiltC * (r - tiltR);
        l = tiltL * gl + (l - tiltL) * gh;
        r = tiltR * gl + (r - tiltR) * gh;
    }
    if (punch > 0.001f) {
        float ax = juce::jmax (std::abs (l), std::abs (r));
        pF = ax > pF ? pFa * pF + (1.f - pFa) * ax : pFr * pF + (1.f - pFr) * ax;
        pS = ax > pS ? pSa * pS + (1.f - pSa) * ax : pSr * pS + (1.f - pSr) * ax;
        auto db = [] (float e) { return 20.f * std::log10 (juce::jmax (e, 1.0e-5f)); };
        float diff = pS < 1.0e-4f ? 0.f : juce::jlimit (0.f, 18.f, db (pF) - db (pS));
        pG += (punch * diff * 0.9f - pG) * (1.f - pGc);
        float gg = juce::Decibels::decibelsToGain (pG);
        l *= gg; r *= gg;
    }
    if (sustain > 0.001f) {
        // tail lift (transient-designer sustain): a slow-release follower stays up while a
        // fast one falls, so the gain only rises in the decay, never on the hit
        float ax = juce::jmax (std::abs (l), std::abs (r));
        sF = ax > sF ? ax : sFr * sF + (1.f - sFr) * ax;
        sS = ax > sS ? ax : sSr * sS + (1.f - sSr) * ax;
        auto db = [] (float e) { return 20.f * std::log10 (juce::jmax (e, 1.0e-5f)); };
        float diff = sS < 2.0e-4f ? 0.f : juce::jlimit (0.f, 24.f, db (sS) - db (sF));
        sG += (sustain * diff * 0.75f - sG) * (1.f - pGc);
        float gg = juce::Decibels::decibelsToGain (sG);
        l *= gg; r *= gg;
    }
}

} // namespace mds
