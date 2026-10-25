#pragma once
#include <JuceHeader.h>
#include "dsp/SampleEngine.h"
#include "dsp/Effects.h"
#include "dsp/Limiter.h"
#include "dsp/TempoSync.h"
#include "Presets.h"
#include "Visuals.h"
#include "MidiLearn.h"

namespace mds {

class SamplerProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    SamplerProcessor();
    ~SamplerProcessor() override;

    void prepareToPlay (double sr, int block) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pastel Drum Sampler"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return juce::jmax (1, (int) getFactoryPresets().size()); }
    int getCurrentProgram() override { return currentKit; }
    void setCurrentProgram (int i) override { loadKit (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    // ---- pads ----
    struct MacroTarget { std::atomic<int> param { -1 }; std::atomic<float> depth { 0.f }; };
    struct Macro { juce::String name; MacroTarget t[MaxMacroTargets]; };
    struct PadEngine {
        Layer layers[NumLayers];
        std::atomic<int> output { 0 };     // 0 = Main, 1..16 = aux bus "Out 2".."Out 17"
        std::atomic<int> choke { 0 };      // 0 = none, 1..8
        std::atomic<int> voiceMode { VM_Poly }, layerMode { LM_Stack };
        std::atomic<bool> mute { false }, solo { false };
        juce::String name;                 // message thread; empty = first layer's file name
        Macro macros[NumMacros];
        FxChainState fx;
        SlotDsp fxDsp[FxChainState::NumSlots];
        // audio thread
        float eff[NumPadParams] = {};      // macro-modulated parameter values (cooked)
        int fmode = 0, tail = 0;
        std::atomic<int> rrNext { 0 };      // round robin: next layer to try (UI shows it)
        PadDsp dsp;
        // UI feedback
        std::atomic<float> meter { 0.f };
        std::atomic<int> hits { 0 };
        std::atomic<int> playLayer { -1 };
        std::atomic<float> playPos { -1.f };  // 0..1 across the whole file, -1 = idle
        ScopeTap scope;                    // post-FX, pre-level mono (freezes when the pad's tail ends)
        // UI -> audio audition (velocity, 0 = none)
        std::atomic<float> pendingTrig { 0.f };
    };
    PadEngine pads[NumPads];

    FxChainState globalFx;
    SlotDsp globalDsp[FxChainState::NumSlots];
    std::atomic<float> revDecay { 0.5f }, revDamp { 0.5f }, dlyTime { 0.35f }, dlyFb { 0.35f };
    std::atomic<int> dlySync { -1 };          // delay return: -1 = free (DLY TIME), else a TempoSync division
    std::atomic<float> hostBpm { 120.f };     // last tempo read from the playhead (UI readout)
    SlotDsp sendRevDsp, sendDlyDsp;
    std::atomic<float> masterGain { 0.8f };
    std::atomic<bool> limiterOn { true };
    std::atomic<float> outLevel { 0.f };
    std::atomic<int> selectedPad { 0 }, selectedLayer { 0 };
    std::atomic<int> activeVoices { 0 };

    juce::AudioProcessorValueTreeState apvts;
    pastel::MidiLearn midiLearn { *this, "Pastel Drum Sampler" }; // global CC map + preset knob
    std::atomic<int> stateVersion { 0 }; // bumped on every structural change; UI refreshes on change
    double currentSampleRate() const { return sampleRate; }

    // ---- samples (message thread) ----
    juce::AudioFormatManager formats;
    static bool isAudioFile (const juce::File& f);
    SampleData::Ptr loadSampleFile (const juce::File& f);  // decodes, or reuses the pool copy
    bool loadLayer (int pad, int layer, const juce::File& f, bool undoable = true);
    void loadPadFiles (int pad, const juce::Array<juce::File>& files); // replaces the pad's layers
    void clearLayer (int pad, int layer, bool undoable = true);
    void clearPad (int pad);
    void setLayerMode (int pad, int mode);   // VELOCITY on full-range layers splits 0..127 evenly first
    juce::String padDisplayName (int pad) const;
    int numLoadedLayers (int pad) const;
    void auditionPad (int pad, float velocity = 0.85f) { pads[juce::jlimit (0, NumPads - 1, pad)].pendingTrig.store (velocity); }
    // browser preview: plays a file once through Main (decoded here, on the message thread)
    bool previewFile (const juce::File& f);
    void stopPreview();
    bool isPreviewing() const { return previewPlaying.load(); }
    void maintenance();   // reverb/delay memory + sample pool cleanup (timer; tests call it directly)

    // ---- macros ----
    juce::AudioParameterFloat* macroParam (int pad, int k) const { return macroPar[pad][k]; }
    juce::AudioParameterFloat* padParam (int pad, int idx) const { return padPar[pad][idx]; }
    void setMacroTarget (int pad, int k, int param, float depth); // depth 0 = remove
    float macroDepth (int pad, int k, int param) const;
    void defaultMacros (int pad);

    // ---- pad clipboard ----
    juce::ValueTree padToVT (int pad);
    void padFromVT (int pad, const juce::ValueTree&, const juce::File& baseDir = {});
    juce::ValueTree padClipboard;
    void copyPad (int pad) { padClipboard = padToVT (pad); }
    void pastePad (int pad);

    // ---- kits / state ----
    int currentKit = -1; // factory index, or -1 for user/init sound
    juce::String kitName = "Init";
    juce::File kitBaseDir;  // folder of the last kit file (relative sample paths)
    void loadKit (int idx);
    void initKit();
    void initKitNoUndo();
    juce::ValueTree fullStateVT();
    // kit loads off the message thread: the samples decode on a worker thread,
    // then the state is applied here on the message thread as one undo step.
    // A newer request supersedes one still decoding. setAsyncLoads (false) = inline (tests).
    void loadStateAsync (const juce::ValueTree& kit, const juce::File& baseDir, std::function<void()> onApplied = {});
    void loadKitAsync (int factoryIdx);
    bool loadUserKitAsync (const juce::File& file);   // false: not a kit file (nothing changes)
    void setAsyncLoads (bool a) { asyncLoads = a; }
    bool isLoadingKit() const { return pendingLoads.load() > 0; }
    void loadFullStateVT (const juce::ValueTree&, const juce::File& baseDir = {}, bool undoable = true);

    // randomize locks (session state, saved with the DAW project): every pad
    // param, plus layer settings, filter mode, FX chain, macro positions
    enum { LockLayers = NumPadParams, LockFMode, LockFx, LockMacros, NumLocks };
    bool isLocked (int pad, int key) const { return locks[juce::jlimit (0, NumPads - 1, pad)][juce::jlimit (0, NumLocks - 1, key)]; }
    void setLocked (int pad, int key, bool on) { locks[juce::jlimit (0, NumPads - 1, pad)][juce::jlimit (0, NumLocks - 1, key)] = on; }
    void toggleLock (int pad, int key) { setLocked (pad, key, ! isLocked (pad, key)); }

    void randomizePad (int pad, float intensity = 1.f);
    void randomizeKit (float intensity = 1.f);
    void randomizePadFx (int pad, float intensity);
    void mutatePad (int pad, float amount = 0.25f);
    void mutateKit (float amount = 0.18f);

    // one history for everything: full-state snapshots (kit edits) and single
    // parameter steps ("PARAM", recorded per knob gesture: begin..end = one step)
    void pushUndo();
    void undo();
    void redo();
    std::vector<juce::ValueTree> undoStack, redoStack;
    static constexpr size_t MaxUndo = 30;

    bool locks[NumPads][NumLocks] = {};

    // parameter helpers (message thread)
    void setPadParam (int pad, int idx, float cooked);
    float getPadParam (int pad, int idx) const;
    void pullParamsForTest() { pullParams(); }

private:
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec spec {};
    static constexpr float LimitCeiling = 0.9661f;   // -0.3 dBFS
    PeakLimiter limiter;                              // Main only; its lookahead is the plugin latency
    std::vector<float> auxDelay;                      // aux buses delayed by the same lookahead (stay in sync with Main)
    int auxDelayLen = 1, auxDelayPos = 0;
    int globalTail = 0, sendTail = 0, tailLenSamples = 44100 * 4;
    juce::uint32 voiceStamp = 0;
    juce::Random liveRng { (juce::uint32) juce::Time::currentTimeMillis() };
    juce::Random audioRng { 0x5eed };

    SampleVoice voices[MaxVoices + StealVoices];
    std::atomic<float>* padRaw[NumPads][NumPadParams] = {};
    juce::AudioParameterFloat* padPar[NumPads][NumPadParams] = {};
    juce::AudioParameterFloat* macroPar[NumPads][NumMacros] = {};
    std::atomic<float>* macroRaw[NumPads][NumMacros] = {};
    std::atomic<float>* fmodeRaw[NumPads] = {};
    std::atomic<float>* masterRaw = nullptr;
    juce::NormalisableRange<float> ranges[NumPadParams];

    juce::SpinLock previewLock;
    SampleData::Ptr previewPending;          // guarded by previewLock
    std::atomic<bool> previewSwap { false }, previewPlaying { false };
    SampleData::Ptr previewVoice;             // audio thread only
    double previewPos = 0.0;

    juce::CriticalSection poolLock;
    juce::ReferenceCountedArray<SampleData> pool;

    static BusesProperties makeBuses();
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void cacheParamPointers();
    void prepareLimiter();                  // limiter + aux delay lines; sets the plugin latency
    void pullParams();                      // audio thread: APVTS + macros -> eff
    void triggerPad (int pad, float vel);   // audio thread
    void startVoice (int pad, int layer, float vel);
    SampleVoice& allocVoice();
    void releasePad (int pad);
    void timerCallback() override { maintenance(); }
    void restoreLayer (int pad, int layer, const juce::ValueTree& lv, const juce::File& baseDir);
    juce::Array<juce::File> layerCandidates (const juce::ValueTree& lv, const juce::File& baseDir) const;

    // knob-turn undo: every change gesture on an automatable parameter (editor
    // knobs, an embedding host's UI) becomes one PARAM step on the undo stack
    struct GestureUndo : juce::AudioProcessorListener {
        explicit GestureUndo (SamplerProcessor& p) : proc (p) {}
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override;
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) override;
        SamplerProcessor& proc;
        std::vector<float> startValue;   // normalised value at gesture start, -1 = no gesture (message thread)
        bool applying = false;           // undo/redo is moving a parameter: don't record it
    };
    GestureUndo gestureUndo { *this };
    void pushUndoEntry (juce::ValueTree entry);
    juce::ValueTree paramEntry (int index) const;   // PARAM step holding the parameter's current value
    void applyParamEntry (const juce::ValueTree&);
    void applyStateVT (const juce::ValueTree&, const juce::File& baseDir);

    // background kit loader (declared last: destroyed first, before the pool it fills)
    bool asyncLoads = true;
    std::atomic<int> loadGen { 0 }, pendingLoads { 0 };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);   // message thread: callbacks after deletion bail out
    juce::ThreadPool loader { juce::ThreadPoolOptions {}.withThreadName ("Kit loader").withNumberOfThreads (1) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerProcessor)
};

} // namespace mds
