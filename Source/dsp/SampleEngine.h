#pragma once
#include "DrumDsp.h"
#include <atomic>
#include <vector>

// ============================================================
// 16-pad sample engine: generic pads (no drum-type assumptions),
// 4 layers per pad (stack or round robin), one-shot/gated voices.
// ============================================================

namespace mds {

constexpr int NumPads = 16;
constexpr int NumLayers = 4;
constexpr int NumMacros = 4;
constexpr int MaxMacroTargets = 8;
constexpr int NumAuxOuts = 16;      // stereo aux buses after Main ("Out 2".."Out 17")
constexpr int NumChokeGroups = 8;
constexpr int BasePadNote = 36;     // pad 1 = C1 (GM drum layout), chromatic upwards
constexpr int MaxVoices = 64;

inline int padMidiNote (int pad) { return BasePadNote + pad; }
inline juce::String noteName (int m) {
    static const char* n[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 2); // 36 -> C1
}
inline juce::Colour padColour (int p) {
    static const juce::uint32 c[NumPads] = {
        0xfff49ac1, 0xfff87171, 0xfffda4af, 0xfffb923c, 0xfffbbf24, 0xfffde047, 0xffa3e635, 0xff34d399,
        0xff2dd4bf, 0xff22d3ee, 0xff38bdf8, 0xff60a5fa, 0xffa78bfa, 0xffc4b5fd, 0xfff0abfc, 0xfff9a8d4
    };
    return juce::Colour (c[juce::jlimit (0, NumPads - 1, p)]);
}

// ---- per-pad automatable parameters (cooked units in APVTS) ----
enum PadParam {
    PP_Tune = 0, PP_Level, PP_Pan,
    PP_Attack, PP_Hold, PP_Decay, PP_Release,
    PP_PitchAmt, PP_PitchDecay, PP_PitchCurve,
    PP_Cutoff, PP_Reso, PP_FltEnv, PP_FltDecay,
    PP_Drive, PP_Comp, PP_Tone, PP_Punch,
    PP_VelAmp, PP_VelStart, PP_VelFilt, PP_RandPitch,
    PP_SendRev, PP_SendDly,
    PP_Length,          // -100..+100: down = fade out earlier (relative to the sample), up = lift the tail
    NumPadParams
};

struct PadParamMeta {
    const char* key;    // stable parameter-ID suffix: never rename (automation / saved kits)
    const char* label;
    float min, max, def;
    float centre;       // skew centre (0 = linear)
    const char* fmt;    // st dB pan ms hold Hz % ct
    bool isEnv;         // envelope: RANDOMIZE leaves it alone
};
const PadParamMeta& padParamMeta (int idx);
juce::NormalisableRange<float> padParamRange (int idx);
juce::String formatPadParam (int idx, float v);
juce::String padParamID (int pad, int idx);   // "p{pad}_{key}"
juce::String padMacroID (int pad, int k);     // "p{pad}_m{k}"
juce::String padFilterModeID (int pad);       // "p{pad}_fmode"  LP / BP / HP
// RANDOMIZE / MUTATE only touch timbre: pitch, filter, drive, tone, sends.
// Never level, pan, amp envelope, compression, punch, velocity response or
// humanize - those change how the drums play, not what they sound like.
inline bool isTimbreParam (int i) {
    switch (i) {
        case PP_Tune: case PP_PitchAmt: case PP_PitchDecay: case PP_PitchCurve:
        case PP_Cutoff: case PP_Reso: case PP_FltEnv: case PP_FltDecay:
        case PP_Drive: case PP_Tone: case PP_SendRev: case PP_SendDly: return true;
        default: return false;
    }
}
constexpr float HoldFullMs = 5000.f;          // HOLD at max = play the whole sample

// ---- per-layer settings (not automatable; saved with the kit) ----
enum LayerParam { LP_Tune = 0, LP_Fine, LP_Start, LP_End, LP_Level, LP_Pan, NumLayerParams };
struct LayerParamMeta { const char* key; const char* label; float min, max, def; const char* fmt; };
const LayerParamMeta& layerParamMeta (int idx);
juce::String formatLayerParam (int idx, float v, double lengthSec);

enum VoiceMode { VM_Poly = 0, VM_Mono, VM_Gated };
enum LayerMode { LM_Stack = 0, LM_RoundRobin };

// ---- decoded sample (shared, immutable after load) ----
// Lifetime: the processor's pool keeps one reference to every sample, voices
// and layers add theirs; the pool drops samples on the message thread once it
// holds the last reference, so the audio thread never frees memory.
struct SampleData : juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<SampleData>;
    juce::AudioBuffer<float> buf;   // 1 or 2 channels
    double sampleRate = 44100.0;
    juce::File file;
    juce::Time modTime;
    int bitDepth = 0;
    float peak = 0.f;
    static constexpr int OverviewSize = 1024;
    std::vector<float> ovMin, ovMax; // mono overview for waveform drawing

    int length() const { return buf.getNumSamples(); }
    int channels() const { return buf.getNumChannels(); }
    double seconds() const { return sampleRate > 0 ? length() / sampleRate : 0.0; }
    void analyse();                  // peak + overview
};

struct Layer {
    mutable juce::SpinLock lock;
    SampleData::Ptr data;            // guarded by lock: audio thread copies the Ptr at trigger
    juce::String path;               // message thread: requested file (kept when missing)
    bool missing = false;            // message thread: path set but file could not be loaded
    std::atomic<float> v[NumLayerParams];
    std::atomic<bool> reverse { false }, normalize { false };

    Layer() { resetParams(); }
    SampleData::Ptr get() const { const juce::SpinLock::ScopedLockType l (lock); return data; }
    void set (SampleData::Ptr p) {
        SampleData::Ptr old;
        { const juce::SpinLock::ScopedLockType l (lock); old = data; data = p; }
        // old released here (message thread); the pool still owns a reference
    }
    bool hasSample() const { return get() != nullptr; }
    void resetParams() {
        for (int i = 0; i < NumLayerParams; ++i) v[i].store (layerParamMeta (i).def);
        reverse = false; normalize = false;
    }
};

// everything a voice needs, resolved at trigger time
struct VoiceStart {
    SampleData::Ptr data;
    int pad = 0, layer = 0;
    double hostRate = 44100.0;
    float startFrac = 0.f, endFrac = 1.f;
    double startOffsetSec = 0.0;   // VEL>START skip
    bool reverse = false;
    float gainL = 1.f, gainR = 1.f;
    float semis = 0.f;
    float attackMs = 0.f, holdMs = HoldFullMs, decayMs = 400.f, releaseMs = 150.f;
    bool gated = false;
    float pitchAmt = 0.f, pitchDecayMs = 80.f, pitchCurve = 0.35f;
    int filterMode = 0;            // 0 LP 1 BP 2 HP
    float cutoff = 20000.f, reso = 0.f, fltEnv = 0.f, fltDecayMs = 200.f, velFiltOct = 0.f;
    float lengthScale = 1.f;       // LENGTH < 0: fraction of the region that plays (fades out before)
};

struct SampleVoice {
    bool active = false;
    int pad = -1, layer = -1;
    juce::uint32 age = 0;
    SampleData::Ptr data;

    void prepare (float sr) { hostSr = sr; fL.setSampleRate (sr); fR.setSampleRate (sr); penv.setSampleRate (sr); }
    void start (const VoiceStart& s, juce::uint32 stamp);
    void noteOff();                       // gated pads: enter RELEASE
    void choke (float ms);                // fast fade (choke groups, mono retrigger, steal)
    // adds one stereo frame; ends the voice itself when done
    void render (float& outL, float& outR);
    float progress() const { return len > 0.0 ? (float) (pos / len) : 0.f; } // 0..1 through the region
    float regionStartFrac = 0.f, regionEndFrac = 1.f;
    bool reversed() const { return reverse; }

private:
    float hostSr = 44100.f;
    double pos = 0.0, len = 0.0, regionStart = 0.0, baseInc = 1.0;
    bool reverse = false;
    float gL = 1.f, gR = 1.f;
    enum Stage { Attack, Hold, Decay, Release, Done } stage = Done;
    float env = 0.f, atkInc = 1.f, decCoef = 0.f, relCoef = 0.f;
    int holdLeft = 0; bool holdFull = true, gated = false;
    float fadeIn = 0.f;                   // region-start anti-click (samples)
    float chokeGain = 1.f, chokeStep = 0.f;
    PitchEnv penv; bool pitchOn = false;
    bool useFilter = false; int fMode = 0;
    float baseCut = 20000.f, q = 0.707f, fEnvOct = 0.f, fEnv = 0.f, fEnvCoef = 0.f;
    int filterTick = 0; float curCut = 20000.f;
    double lenFadeStart = 0.0, lenFadeEnd = 0.0;   // LENGTH fade (source samples into the region)
    ResFilter fL, fR;
    float readAt (int ch, double idx) const;
    void finish() { active = false; stage = Done; data = nullptr; }
};

// pad strip DSP between the voices and the FX chain: DRIVE, COMP, TONE, PUNCH
struct PadDsp {
    float sr = 44100.f;
    float compEnv = 0.f, tiltL = 0.f, tiltR = 0.f, pF = 0.f, pS = 0.f, pG = 0.f;
    float sF = 0.f, sS = 0.f, sG = 0.f, sFr = 0.f, sSr = 0.f;   // LENGTH > 0: tail lift followers
    float tiltC = 0.f, cA = 0.f, cR = 0.f, pFa = 0.f, pFr = 0.f, pSa = 0.f, pSr = 0.f, pGc = 0.f;
    void prepare (float s);
    void reset() { compEnv = tiltL = tiltR = pF = pS = pG = sF = sS = sG = 0.f; }
    // drive/comp/punch/sustain 0..1, tone -1..1
    void process (float& l, float& r, float drive, float comp, float tone, float punch, float sustain = 0.f);
};

} // namespace mds
