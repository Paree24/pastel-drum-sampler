#include "PluginProcessor.h"

namespace mds {

juce::AudioProcessor::BusesProperties SamplerProcessor::makeBuses() {
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < NumAuxOuts; ++i)
        b = b.withOutput ("Out " + juce::String (i + 2), juce::AudioChannelSet::stereo(), false);
    return b;
}

SamplerProcessor::SamplerProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", makeLayout()) {
    formats.registerBasicFormats();
    for (int i = 0; i < NumPadParams; ++i) ranges[i] = padParamRange (i);
    cacheParamPointers();
    for (auto& v : voices) v.prepare ((float) sampleRate);
    prepareLimiter();   // usable before the host's first prepareToPlay
    initKitNoUndo();
    // first factory kit if one ships, else the blank slate
    auto& fac = getFactoryPresets();
    if (! fac.empty()) applyPreset (*this, fac.front().index);
    undoStack.clear();
    midiLearn.onPresetStep = [this, requested = -1] (int delta) mutable {
        // same walk as the header's < > buttons: factory order, wrapping.
        // currentKit changes only when a load lands: while one is running,
        // keep walking from the kit the knob asked for last
        auto& all = getFactoryPresets();
        if (all.empty()) return;
        const int from = isLoadingKit() && requested >= 0 ? requested : currentKit;
        int pos = 0;
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].index == from) { pos = (int) i; break; }
        const int n = (int) all.size();
        requested = all[(size_t) (((pos + delta) % n + n) % n)].index;
        loadKitAsync (requested);
    };
    gestureUndo.startValue.assign ((size_t) getParameters().size(), -1.f);
    addListener (&gestureUndo);
    startTimerHz (10);
}

SamplerProcessor::~SamplerProcessor() {
    *alive = false;
    ++loadGen;                          // a decoding job stops at the next file
    loader.removeAllJobs (true, 10000);
    removeListener (&gestureUndo);
    stopTimer();
}

bool SamplerProcessor::isBusesLayoutSupported (const BusesLayout& l) const {
    auto main = l.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i) {
        const auto& s = l.outputBuses.getReference (i);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    for (const auto& in : l.inputBuses) if (! in.isDisabled()) return false;
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout SamplerProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int p = 0; p < NumPads; ++p) {
        auto prefix = "Pad " + juce::String (p + 1) + " ";
        for (int i = 0; i < NumPadParams; ++i) {
            auto& m = padParamMeta (i);
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (padParamID (p, i), 1), prefix + juce::String (m.label).toLowerCase(),
                padParamRange (i), m.def));
        }
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (padFilterModeID (p), 1), prefix + "filter mode",
            juce::StringArray ({ "LP", "BP", "HP" }), 0));
        for (int k = 0; k < NumMacros; ++k)
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (padMacroID (p, k), 1), prefix + "macro " + juce::String (k + 1),
                juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master", 1), "Master", juce::NormalisableRange<float> (0.f, 1.2f), 0.8f));
    return layout;
}

void SamplerProcessor::cacheParamPointers() {
    for (int p = 0; p < NumPads; ++p) {
        for (int i = 0; i < NumPadParams; ++i) {
            padRaw[p][i] = apvts.getRawParameterValue (padParamID (p, i));
            padPar[p][i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (padParamID (p, i)));
            jassert (padRaw[p][i] != nullptr && padPar[p][i] != nullptr);
        }
        for (int k = 0; k < NumMacros; ++k) {
            macroRaw[p][k] = apvts.getRawParameterValue (padMacroID (p, k));
            macroPar[p][k] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (padMacroID (p, k)));
        }
        fmodeRaw[p] = apvts.getRawParameterValue (padFilterModeID (p));
    }
    masterRaw = apvts.getRawParameterValue ("master");
}

void SamplerProcessor::prepareToPlay (double sr, int block) {
    sampleRate = sr;
    tailLenSamples = (int) (sr * 4.0);
    spec.sampleRate = sr; spec.maximumBlockSize = (juce::uint32) juce::jmax (1, block); spec.numChannels = 2;
    for (auto& v : voices) { v.prepare ((float) sr); v.active = false; v.data = nullptr; }
    for (auto& pe : pads) {
        FxProcessor::prepareAll (pe.fxDsp, FxChainState::NumSlots, spec);
        pe.dsp.prepare ((float) sr);
        pe.tail = 0;
    }
    FxProcessor::prepareAll (globalDsp, FxChainState::NumSlots, spec);
    for (auto* d : { &sendRevDsp, &sendDlyDsp }) { d->prepare (spec); d->ensureHeavy (sr); d->reset(); }
    // the send delay also runs tempo-synced: room for a 1/2 note at slow tempos
    sendDlyDsp.delayBuf.setSize (2, (int) (sr * FxProcessor::sendDelayMaxSec) + 64, false, true, false);
    sendDlyDsp.delayBuf.clear(); sendDlyDsp.delayPos = 0;
    globalTail = sendTail = 0;
    prepareLimiter();
    maintenance();
    pullParams();
}

void SamplerProcessor::prepareLimiter() {
    limiter.prepare (sampleRate);
    auxDelayLen = limiter.latency();
    auxDelay.assign ((size_t) (NumAuxOuts * 2 * auxDelayLen), 0.f);
    auxDelayPos = 0;
    setLatencySamples (limiter.latency());
}

// ---------------- audio thread ----------------
void SamplerProcessor::pullParams() {
    for (int p = 0; p < NumPads; ++p) {
        auto& pe = pads[p];
        float off[NumPadParams] = {};
        bool any = false;
        for (int k = 0; k < NumMacros; ++k) {
            float bip = (macroRaw[p][k]->load() - 0.5f) * 2.f; // centre = neutral
            if (std::abs (bip) < 1.0e-4f) continue;
            for (auto& t : pe.macros[k].t) {
                int prm = t.param.load();
                if (prm < 0 || prm >= NumPadParams) continue;
                off[prm] += t.depth.load() * bip;
                any = true;
            }
        }
        for (int i = 0; i < NumPadParams; ++i) {
            float base = padRaw[p][i]->load();
            if (any && off[i] != 0.f)
                base = ranges[i].convertFrom0to1 (juce::jlimit (0.f, 1.f, ranges[i].convertTo0to1 (base) + off[i]));
            pe.eff[i] = base;
        }
        pe.fmode = juce::jlimit (0, 2, (int) std::round (fmodeRaw[p]->load()));
    }
    if (masterRaw) masterGain = masterRaw->load();
}

SampleVoice& SamplerProcessor::allocVoice() {
    // MaxVoices may sound at once; when they all do, the oldest one fades out
    // (StealFadeMs) in a spare slot instead of being cut mid-waveform
    constexpr float StealFadeMs = 4.f;
    SampleVoice *freeV = nullptr, *oldest = nullptr, *oldestFading = nullptr;
    int live = 0;
    for (auto& v : voices) {
        if (! v.active) { if (freeV == nullptr) freeV = &v; continue; }
        if (v.fading()) { if (oldestFading == nullptr || v.age < oldestFading->age) oldestFading = &v; continue; }
        ++live;
        if (oldest == nullptr || v.age < oldest->age) oldest = &v;
    }
    if (live >= MaxVoices && oldest != nullptr) oldest->choke (StealFadeMs);
    if (freeV != nullptr) return *freeV;
    return oldestFading != nullptr ? *oldestFading : *oldest; // every spare slot busy fading: hard steal
}

void SamplerProcessor::startVoice (int p, int li, float vel) {
    auto& pe = pads[p];
    auto& L = pe.layers[li];
    auto d = L.get();
    if (d == nullptr) return;
    const float* e = pe.eff;
    VoiceStart s;
    s.data = d; s.pad = p; s.layer = li; s.hostRate = sampleRate;
    // power curve: half velocity ~-8 dB and ghost notes -30 dB or lower at the
    // default 70% (the old blend floored at 1 - VEL>AMP: only ~10 dB of range);
    // VEL>AMP 0 = no velocity response (same curve as Pastel Drum Synth)
    float velAmt = juce::jlimit (0.f, 1.f, e[PP_VelAmp] * 0.01f);
    float velGain = std::pow (juce::jlimit (0.05f, 1.f, vel), 2.f * velAmt);
    float lvDb = L.v[LP_Level].load();
    float lvl = lvDb <= -59.5f ? 0.f : juce::Decibels::decibelsToGain (lvDb);
    if (L.normalize.load() && d->peak > 1.0e-4f) lvl *= 0.977f / d->peak;
    float lp = L.v[LP_Pan].load() * 0.01f;
    s.gainL = lvl * velGain * (lp > 0.f ? 1.f - lp : 1.f);
    s.gainR = lvl * velGain * (lp < 0.f ? 1.f + lp : 1.f);
    float rnd = e[PP_RandPitch] > 0.f ? (audioRng.nextFloat() * 2.f - 1.f) * e[PP_RandPitch] * 0.01f : 0.f;
    s.semis = e[PP_Tune] + L.v[LP_Tune].load() + L.v[LP_Fine].load() * 0.01f + rnd;
    s.startFrac = L.v[LP_Start].load(); s.endFrac = L.v[LP_End].load();
    s.reverse = L.reverse.load();
    s.startOffsetSec = e[PP_VelStart] * 0.01f * (1.f - vel) * 0.06f;
    s.attackMs = e[PP_Attack]; s.holdMs = e[PP_Hold]; s.decayMs = e[PP_Decay]; s.releaseMs = e[PP_Release];
    s.gated = pe.voiceMode.load() == VM_Gated;
    s.pitchAmt = e[PP_PitchAmt]; s.pitchDecayMs = e[PP_PitchDecay]; s.pitchCurve = e[PP_PitchCurve] * 0.01f;
    s.filterMode = pe.fmode; s.cutoff = e[PP_Cutoff]; s.reso = e[PP_Reso] * 0.01f;
    s.fltEnv = e[PP_FltEnv] * 0.01f; s.fltDecayMs = e[PP_FltDecay];
    s.velFiltOct = e[PP_VelFilt] * 0.01f * (1.f - vel) * 5.f;
    s.lengthScale = e[PP_Length] < 0.f ? std::pow (0.05f, -e[PP_Length] * 0.01f) : 1.f;   // -100 -> 5 %
    allocVoice().start (s, ++voiceStamp);
    pe.playLayer = li;
}

void SamplerProcessor::triggerPad (int p, float vel) {
    auto& pe = pads[p];
    int g = pe.choke.load();
    if (g > 0)
        for (auto& v : voices)
            if (v.active && v.pad != p && pads[v.pad].choke.load() == g) v.choke (8.f);
    if (pe.voiceMode.load() == VM_Mono)
        for (auto& v : voices) if (v.active && v.pad == p) v.choke (4.f);
    if (pe.layerMode.load() == LM_RoundRobin) {
        for (int k = 0; k < NumLayers; ++k) {
            int li = (pe.rrNext + k) % NumLayers;
            if (pe.layers[li].hasSample()) { startVoice (p, li, vel); pe.rrNext = (li + 1) % NumLayers; break; }
        }
    } else if (pe.layerMode.load() == LM_Velocity) {
        const int v = juce::roundToInt (juce::jlimit (0.f, 1.f, vel) * 127.f); // lovel/hivel are MIDI 0..127
        for (int li = 0; li < NumLayers; ++li)
            if (pe.layers[li].velocityMatches (v)) startVoice (p, li, vel);
    } else {
        for (int li = 0; li < NumLayers; ++li) startVoice (p, li, vel);
    }
    ++pe.hits;
}

void SamplerProcessor::releasePad (int p) {
    for (auto& v : voices) if (v.active && v.pad == p) v.noteOff();
}

void SamplerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midiLearn.process (midi); // learned CCs land before this block renders
    pullParams();
    const int N = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, N);

    // output buses: Main + enabled aux pairs (disabled aux -> pad falls back to Main)
    float* outL[1 + NumAuxOuts] = {};
    float* outR[1 + NumAuxOuts] = {};
    const int nb = juce::jmin (getBusCount (false), 1 + NumAuxOuts);
    for (int b = 0; b < nb; ++b) {
        auto* bus = getBus (false, b);
        if (bus == nullptr || ! bus->isEnabled()) continue;
        auto bb = getBusBuffer (buffer, false, b);
        if (bb.getNumChannels() == 0) continue;
        outL[b] = bb.getWritePointer (0);
        outR[b] = bb.getNumChannels() > 1 ? bb.getWritePointer (1) : nullptr;
    }

    if (previewSwap.exchange (false)) {
        const juce::SpinLock::ScopedLockType sl (previewLock);
        previewVoice = previewPending;   // swap only: the old Ptr's last owner is the pool
        previewPos = 0.0;
    }
    const double previewInc = previewVoice != nullptr ? previewVoice->sampleRate / sampleRate : 0.0;

    for (int p = 0; p < NumPads; ++p) {
        float v = pads[p].pendingTrig.exchange (0.f);
        if (v > 0.f && ! pads[p].mute.load()) triggerPad (p, v);
    }

    // block-rate pad gains / routing
    float gainL[NumPads], gainR[NumPads], sRev[NumPads], sDly[NumPads];
    bool silent[NumPads], hasFx[NumPads];
    int route[NumPads];
    bool anySolo = false;
    for (auto& pe : pads) if (pe.solo.load()) { anySolo = true; break; }
    for (int p = 0; p < NumPads; ++p) {
        auto& pe = pads[p];
        float lv = pe.eff[PP_Level] <= -59.5f ? 0.f : juce::Decibels::decibelsToGain (pe.eff[PP_Level]);
        float pn = pe.eff[PP_Pan] * 0.01f;
        gainL[p] = lv * (pn > 0.f ? 1.f - pn : 1.f);
        gainR[p] = lv * (pn < 0.f ? 1.f + pn : 1.f);
        sRev[p] = pe.eff[PP_SendRev] * 0.01f; sDly[p] = pe.eff[PP_SendDly] * 0.01f;
        silent[p] = pe.mute.load() || (anySolo && ! pe.solo.load());
        hasFx[p] = pe.fx.enabled.load() && FxProcessor::numActive (pe.fx) > 0;
        int o = pe.output.load();
        route[p] = (o > 0 && o <= NumAuxOuts && outL[o] != nullptr) ? o : 0;
    }

    const float sr = (float) sampleRate;
    const double bpm = TempoSync::hostBpm (getPlayHead(), hostBpm.load());
    hostBpm = (float) bpm;
    const int dsync = dlySync.load();
    const float dlySec = dsync >= 0 ? (float) TempoSync::divisionSeconds (dsync, bpm) : FxProcessor::sendDelaySeconds (dlyTime.load());
    float mg = masterGain.load();
    if (! std::isfinite (mg)) mg = 0.8f;
    auto it = midi.cbegin();
    const auto end = midi.cend();
    const bool limitOn = limiterOn.load();
    bool anyAux = false;
    for (int o = 1; o <= NumAuxOuts; ++o) anyAux = anyAux || outL[o] != nullptr;
    if (auxDelay.size() < (size_t) (NumAuxOuts * 2 * auxDelayLen)) anyAux = false; // not prepared

    for (int i = 0; i < N; ++i) {
        // sample-accurate MIDI
        for (; it != end && (*it).samplePosition <= i; ++it) {
            const auto msg = (*it).getMessage();
            if (msg.isNoteOn()) {
                int p = msg.getNoteNumber() - BasePadNote;
                if (p >= 0 && p < NumPads && ! pads[p].mute.load()) triggerPad (p, msg.getFloatVelocity());
            } else if (msg.isNoteOff()) {
                int p = msg.getNoteNumber() - BasePadNote;
                if (p >= 0 && p < NumPads) releasePad (p);
            } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                for (auto& v : voices) v.choke (5.f);
            }
        }

        float pl[NumPads] = {}, pr[NumPads] = {};
        bool sounding[NumPads] = {};
        for (auto& v : voices)
            if (v.active) { sounding[v.pad] = true; v.render (pl[v.pad], pr[v.pad]); }

        float mainL = 0.f, mainR = 0.f, revIn = 0.f, dlyIn = 0.f;
        float auxL[1 + NumAuxOuts] = {}, auxR[1 + NumAuxOuts] = {};
        bool anySound = false;
        for (int p = 0; p < NumPads; ++p) {
            auto& pe = pads[p];
            if (sounding[p]) pe.tail = hasFx[p] ? tailLenSamples : 256;
            if (pe.tail <= 0) continue;
            if (! sounding[p]) --pe.tail;
            anySound = true;
            if (silent[p]) continue;
            float l = pl[p], r = pr[p];
            pe.dsp.process (l, r, pe.eff[PP_Drive] * 0.01f, pe.eff[PP_Comp] * 0.01f, pe.eff[PP_Tone] * 0.01f, pe.eff[PP_Punch] * 0.01f,
                            juce::jmax (0.f, pe.eff[PP_Length]) * 0.01f);
            if (hasFx[p]) FxProcessor::processChainStereo (pe.fx, pe.fxDsp, l, r, sr);
            pe.scope.push ((l + r) * 0.5f);
            l *= gainL[p]; r *= gainR[p];
            if (! std::isfinite (l) || ! std::isfinite (r)) { l = r = 0.f; }
            float mono = (l + r) * 0.5f;
            revIn += mono * sRev[p];
            dlyIn += mono * sDly[p];
            float m = juce::jmin (1.f, juce::jmax (std::abs (l), std::abs (r)));
            if (m > pe.meter.load (std::memory_order_relaxed)) pe.meter.store (m, std::memory_order_relaxed);
            int o = route[p];
            if (o == 0) { mainL += l; mainR += r; }
            else { auxL[o] += l * mg; auxR[o] += r * mg; }
        }
        if (anyAux) {   // aux buses: the limiter's lookahead as a plain delay, so every bus lines up with Main
            for (int o = 1; o <= NumAuxOuts; ++o) {
                if (outL[o] == nullptr) continue;
                float* dl = auxDelay.data() + (size_t) ((o - 1) * 2 * auxDelayLen);
                float* dr = dl + auxDelayLen;
                const float yl = dl[auxDelayPos], yr = dr[auxDelayPos];
                dl[auxDelayPos] = auxL[o]; dr[auxDelayPos] = auxR[o];
                if (outR[o]) { outL[o][i] += yl; outR[o][i] += yr; }
                else outL[o][i] += (yl + yr) * 0.5f;
            }
            auxDelayPos = (auxDelayPos + 1) % auxDelayLen;
        }

        // shared send returns ring out on their own tail after the last pad
        if (anySound) sendTail = tailLenSamples;
        if (anySound || sendTail > 0) {
            float dL = 0.f, dR = 0.f, rL = 0.f, rR = 0.f;
            FxProcessor::sendDelayStereo (sendDlyDsp, dlyIn, dlySec, dlyFb.load(), sr, dL, dR);
            FxProcessor::sendReverbStereo (sendRevDsp, revIn, revDecay.load(), revDamp.load(), rL, rR);
            mainL += rL + dL; mainR += rR + dR;
            if (! anySound) --sendTail;
        }
        if (anySound) globalTail = tailLenSamples;
        if ((anySound || globalTail > 0) && globalFx.enabled.load()) {
            FxProcessor::processChainStereo (globalFx, globalDsp, mainL, mainR, sr);
            if (! anySound) --globalTail;
        }
        float gL = mainL * mg, gR = mainR * mg;
        if (previewVoice != nullptr) {   // browser preview (linear interpolation is plenty here)
            const auto& pb = previewVoice->buf;
            int i0 = (int) previewPos;
            if (i0 + 1 < pb.getNumSamples()) {
                float fr = (float) (previewPos - i0);
                auto rd = [&] (int ch) { int c = juce::jmin (ch, pb.getNumChannels() - 1);
                                         return pb.getSample (c, i0) * (1.f - fr) + pb.getSample (c, i0 + 1) * fr; };
                gL += rd (0) * 0.8f * mg; gR += rd (1) * 0.8f * mg;
                previewPos += previewInc;
            } else previewVoice = nullptr;
        }
        if (! std::isfinite (gL)) gL = 0.f;
        if (! std::isfinite (gR)) gR = 0.f;
        limiter.process (&gL, &gR, 1, LimitCeiling, limitOn);   // off: same lookahead delay, no gain change
        if (outR[0]) { outL[0][i] += gL; outR[0][i] += gR; }
        else if (outL[0]) outL[0][i] += (gL + gR) * 0.5f;
        outLevel = juce::jmax (outLevel.load (std::memory_order_relaxed) * 0.9995f, juce::jmin (1.f, (std::abs (gL) + std::abs (gR)) * 0.5f));
    }

    // UI feedback: meters fall, playhead follows each pad's newest voice
    const float fall = std::pow (0.9995f, (float) N);
    int nAct = 0;
    for (int p = 0; p < NumPads; ++p) {
        auto& pe = pads[p];
        pe.meter.store (pe.meter.load (std::memory_order_relaxed) * fall, std::memory_order_relaxed);
        const SampleVoice* newest = nullptr;
        for (auto& v : voices)
            if (v.active && v.pad == p && (newest == nullptr || v.age > newest->age)) newest = &v;
        if (newest) {
            float a = newest->regionStartFrac, b = newest->regionEndFrac, t = newest->progress();
            pe.playPos.store (newest->reversed() ? b - t * (b - a) : a + t * (b - a));
            pe.playLayer.store (newest->layer);
        } else pe.playPos.store (-1.f);
    }
    for (auto& v : voices) if (v.active && ! v.fading()) ++nAct;
    activeVoices = nAct;
    previewPlaying = previewVoice != nullptr;
}

bool SamplerProcessor::previewFile (const juce::File& f) {
    auto d = loadSampleFile (f);   // pooled: the audio thread never frees it
    { const juce::SpinLock::ScopedLockType sl (previewLock); previewPending = d; }
    previewSwap = true;
    return d != nullptr;
}

void SamplerProcessor::stopPreview() {
    { const juce::SpinLock::ScopedLockType sl (previewLock); previewPending = nullptr; }
    previewSwap = true;
}

// ---------------- samples (message thread) ----------------
bool SamplerProcessor::isAudioFile (const juce::File& f) {
    return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

SampleData::Ptr SamplerProcessor::loadSampleFile (const juce::File& f) {
    if (! f.existsAsFile()) return nullptr;
    auto mod = f.getLastModificationTime();
    {
        const juce::ScopedLock sl (poolLock);
        for (auto* s : pool) if (s->file == f && s->modTime == mod) return s;
    }
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->sampleRate <= 0.0) return nullptr;
    const auto maxLen = (juce::int64) (r->sampleRate * 60.0); // one-shots: cap at 60 s
    int len = (int) juce::jmin (r->lengthInSamples, maxLen);
    if (len <= 0) return nullptr;
    int nch = juce::jlimit (1, 2, (int) r->numChannels);
    SampleData::Ptr d = new SampleData();
    d->buf.setSize (nch, len);
    d->buf.clear();
    r->read (&d->buf, 0, len, 0, true, nch > 1);
    d->sampleRate = r->sampleRate;
    d->file = f; d->modTime = mod;
    d->bitDepth = (int) r->bitsPerSample;
    d->analyse();
    const juce::ScopedLock sl (poolLock);
    pool.add (d.get());
    return d;
}

bool SamplerProcessor::loadLayer (int pad, int layer, const juce::File& f, bool undoable) {
    if (pad < 0 || pad >= NumPads || layer < 0 || layer >= NumLayers) return false;
    auto d = loadSampleFile (f);
    if (d == nullptr) return false; // unreadable: keep what was there
    if (undoable) pushUndo();
    auto& L = pads[pad].layers[layer];
    L.v[LP_Start] = 0.f; L.v[LP_End] = 1.f; L.reverse = false; // region belonged to the old file
    L.path = f.getFullPathName(); L.missing = false;
    L.set (d);
    ++stateVersion;
    return true;
}

void SamplerProcessor::loadPadFiles (int pad, const juce::Array<juce::File>& files) {
    juce::Array<juce::File> audio;
    for (auto& f : files) if (isAudioFile (f) && audio.size() < NumLayers) audio.add (f);
    if (audio.isEmpty()) return;
    pushUndo();
    int loaded = 0;
    for (int li = 0; li < NumLayers; ++li) {
        if (li < audio.size() && loadLayer (pad, li, audio[li], false)) { pads[pad].layers[li].resetParams(); ++loaded; }
        else clearLayer (pad, li, false);
    }
    pads[pad].name = {};
    pads[pad].rrNext = 0;
    ++stateVersion;
}

void SamplerProcessor::clearLayer (int pad, int layer, bool undoable) {
    if (undoable) pushUndo();
    auto& L = pads[pad].layers[layer];
    L.set (nullptr);
    L.path = {}; L.missing = false;
    L.resetParams();
    ++stateVersion;
}

juce::String SamplerProcessor::padDisplayName (int pad) const {
    auto& pe = pads[pad];
    if (pe.name.isNotEmpty()) return pe.name;
    for (auto& L : pe.layers)
        if (L.path.isNotEmpty()) return juce::File::isAbsolutePath (L.path) ? juce::File (L.path).getFileNameWithoutExtension() : L.path;
    return {};
}

int SamplerProcessor::numLoadedLayers (int pad) const {
    int n = 0;
    for (auto& L : pads[pad].layers) if (L.hasSample()) ++n;
    return n;
}

void SamplerProcessor::maintenance() {
    for (auto& pe : pads) FxProcessor::ensureHeavyForChain (pe.fx, pe.fxDsp, sampleRate);
    FxProcessor::ensureHeavyForChain (globalFx, globalDsp, sampleRate);
    const juce::ScopedLock sl (poolLock);
    for (int i = pool.size(); --i >= 0;)
        if (pool.getObjectPointerUnchecked (i)->getReferenceCount() == 1) pool.remove (i); // only the pool holds it
}

// ---------------- parameters / macros ----------------
void SamplerProcessor::setPadParam (int pad, int idx, float cooked) {
    if (auto* p = padPar[pad][idx]) p->setValueNotifyingHost (p->convertTo0to1 (cooked));
}
float SamplerProcessor::getPadParam (int pad, int idx) const { return padRaw[pad][idx]->load(); }

void SamplerProcessor::setMacroTarget (int pad, int k, int param, float depth) {
    auto& m = pads[pad].macros[k];
    for (auto& t : m.t)
        if (t.param.load() == param) {
            if (std::abs (depth) < 1.0e-4f) t.param = -1;
            else t.depth = depth;
            ++stateVersion;
            return;
        }
    if (std::abs (depth) < 1.0e-4f) return;
    for (auto& t : m.t)
        if (t.param.load() < 0) { t.depth = depth; t.param = param; ++stateVersion; return; }
}

float SamplerProcessor::macroDepth (int pad, int k, int param) const {
    for (auto& t : pads[pad].macros[k].t) if (t.param.load() == param) return t.depth.load();
    return 0.f;
}

void SamplerProcessor::defaultMacros (int pad) {
    // generic for any sound: same four on every pad, centre = neutral
    static const char* names[NumMacros] = { "PITCH", "LENGTH", "COLOR", "SPACE" };
    auto& pe = pads[pad];
    for (int k = 0; k < NumMacros; ++k) {
        pe.macros[k].name = names[k];
        for (auto& t : pe.macros[k].t) { t.param = -1; t.depth = 0.f; }
        if (auto* mp = macroPar[pad][k]) mp->setValueNotifyingHost (0.5f);
    }
    setMacroTarget (pad, 0, PP_Tune, 0.25f);      // +-12 st
    setMacroTarget (pad, 0, PP_PitchAmt, 0.1f);
    setMacroTarget (pad, 1, PP_Length, 0.5f);     // full knob travel = full LENGTH range: down shorter, up longer tail
    setMacroTarget (pad, 2, PP_Cutoff, 0.5f);     // down = darker
    setMacroTarget (pad, 2, PP_Tone, 0.35f);
    setMacroTarget (pad, 2, PP_Drive, 0.3f);
    setMacroTarget (pad, 3, PP_SendRev, 0.5f);
    setMacroTarget (pad, 3, PP_SendDly, 0.25f);
}

// ---------------- pad (de)serialisation ----------------
juce::ValueTree SamplerProcessor::padToVT (int p) {
    auto& pe = pads[p];
    juce::ValueTree v ("PAD");
    v.setProperty ("id", p, nullptr);
    v.setProperty ("name", pe.name, nullptr);
    v.setProperty ("out", pe.output.load(), nullptr);
    v.setProperty ("choke", pe.choke.load(), nullptr);
    v.setProperty ("voice", pe.voiceMode.load(), nullptr);
    v.setProperty ("lmode", pe.layerMode.load(), nullptr);
    v.setProperty ("mute", pe.mute.load(), nullptr);
    v.setProperty ("solo", pe.solo.load(), nullptr);
    v.setProperty ("fmode", juce::roundToInt (fmodeRaw[p]->load()), nullptr);
    for (int i = 0; i < NumPadParams; ++i)
        v.setProperty (juce::String ("p_") + padParamMeta (i).key, getPadParam (p, i), nullptr);
    for (int k = 0; k < NumMacros; ++k) {
        juce::ValueTree m ("MACRO");
        m.setProperty ("k", k, nullptr);
        m.setProperty ("name", pe.macros[k].name, nullptr);
        m.setProperty ("value", macroRaw[p][k]->load(), nullptr);
        juce::StringArray tg;
        for (auto& t : pe.macros[k].t)
            if (t.param.load() >= 0) tg.add (juce::String (padParamMeta (t.param.load()).key) + ":" + juce::String (t.depth.load(), 4));
        m.setProperty ("targets", tg.joinIntoString (","), nullptr);
        v.addChild (m, -1, nullptr);
    }
    for (int li = 0; li < NumLayers; ++li) {
        auto& L = pe.layers[li];
        juce::ValueTree lv ("LAYER");
        lv.setProperty ("i", li, nullptr);
        lv.setProperty ("path", L.path, nullptr);
        for (int i = 0; i < NumLayerParams; ++i) lv.setProperty (layerParamMeta (i).key, L.v[i].load(), nullptr);
        lv.setProperty ("rev", L.reverse.load(), nullptr);
        lv.setProperty ("norm", L.normalize.load(), nullptr);
        lv.setProperty ("lovel", L.loVel.load(), nullptr);
        lv.setProperty ("hivel", L.hiVel.load(), nullptr);
        v.addChild (lv, -1, nullptr);
    }
    v.addChild (pe.fx.toVT(), -1, nullptr);
    return v;
}

static int padParamFromKey (const juce::String& key) {
    for (int i = 0; i < NumPadParams; ++i) if (key == padParamMeta (i).key) return i;
    return -1;
}

// resolution order: kit-relative (collected kits travel), absolute, then
// the bare file name next to the kit / inside "<kit> Samples"
juce::Array<juce::File> SamplerProcessor::layerCandidates (const juce::ValueTree& lv, const juce::File& baseDir) const {
    juce::String path = lv.getProperty ("path").toString(), rel = lv.getProperty ("rel").toString();
    juce::Array<juce::File> cands;
    auto base = baseDir != juce::File() ? baseDir : kitBaseDir;
    if (rel.isNotEmpty() && base != juce::File()) cands.add (base.getChildFile (rel));
    if (juce::File::isAbsolutePath (path)) cands.add (juce::File (path));
    auto fileName = rel.isNotEmpty() ? rel.fromLastOccurrenceOf ("/", false, false) : path.fromLastOccurrenceOf ("/", false, false).fromLastOccurrenceOf ("\\", false, false);
    if (base != juce::File() && fileName.isNotEmpty()) cands.add (base.getChildFile (fileName));
    return cands;
}

void SamplerProcessor::restoreLayer (int p, int li, const juce::ValueTree& lv, const juce::File& baseDir) {
    auto& L = pads[p].layers[li];
    for (int i = 0; i < NumLayerParams; ++i)
        L.v[i] = (float) lv.getProperty (layerParamMeta (i).key, layerParamMeta (i).def);
    L.reverse = (bool) lv.getProperty ("rev", false);
    L.normalize = (bool) lv.getProperty ("norm", false);
    L.loVel = juce::jlimit (0, 127, (int) lv.getProperty ("lovel", 0));
    L.hiVel = juce::jlimit (0, 127, (int) lv.getProperty ("hivel", 127));
    juce::String path = lv.getProperty ("path").toString(), rel = lv.getProperty ("rel").toString();
    L.missing = false;
    if (path.isEmpty() && rel.isEmpty()) { L.set (nullptr); L.path = {}; return; }
    for (auto& c : layerCandidates (lv, baseDir))
        if (c.existsAsFile())
            if (auto d = loadSampleFile (c)) { L.set (d); L.path = c.getFullPathName(); return; }
    L.set (nullptr);
    L.path = juce::File::isAbsolutePath (path) || rel.isEmpty() ? path : rel;
    L.missing = true;
}

void SamplerProcessor::padFromVT (int p, const juce::ValueTree& v, const juce::File& baseDir) {
    auto& pe = pads[p];
    pe.name = v.getProperty ("name").toString();
    pe.output = juce::jlimit (0, NumAuxOuts, (int) v.getProperty ("out", 0));
    pe.choke = juce::jlimit (0, NumChokeGroups, (int) v.getProperty ("choke", 0));
    pe.voiceMode = juce::jlimit (0, 2, (int) v.getProperty ("voice", 0));
    pe.layerMode = juce::jlimit (0, (int) LM_Velocity, (int) v.getProperty ("lmode", 0));
    pe.mute = (bool) v.getProperty ("mute", false);
    pe.solo = (bool) v.getProperty ("solo", false);
    pe.rrNext = 0;
    if (auto* fm = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (padFilterModeID (p))))
        fm->setValueNotifyingHost (fm->convertTo0to1 ((float) juce::jlimit (0, 2, (int) v.getProperty ("fmode", 0))));
    for (int i = 0; i < NumPadParams; ++i)
        setPadParam (p, i, (float) v.getProperty (juce::String ("p_") + padParamMeta (i).key, padParamMeta (i).def));
    bool sawMacro = false;
    for (auto c : v) {
        if (c.hasType ("MACRO")) {
            int k = (int) c.getProperty ("k", -1);
            if (k < 0 || k >= NumMacros) continue;
            if (! sawMacro) { // file has macros: start from a clean slate
                for (auto& m : pe.macros) for (auto& t : m.t) { t.param = -1; t.depth = 0.f; }
                sawMacro = true;
            }
            pe.macros[k].name = c.getProperty ("name", "MACRO " + juce::String (k + 1)).toString();
            if (auto* mp = macroPar[p][k]) mp->setValueNotifyingHost ((float) c.getProperty ("value", 0.5f));
            for (auto& tok : juce::StringArray::fromTokens (c.getProperty ("targets").toString(), ",", "")) {
                int prm = padParamFromKey (tok.upToFirstOccurrenceOf (":", false, false));
                if (prm >= 0) setMacroTarget (p, k, prm, tok.fromFirstOccurrenceOf (":", false, false).getFloatValue());
            }
        } else if (c.hasType ("LAYER")) {
            int li = (int) c.getProperty ("i", -1);
            if (li >= 0 && li < NumLayers) restoreLayer (p, li, c, baseDir);
        } else if (c.hasType ("CHAIN")) {
            pe.fx.fromVT (c);
            FxProcessor::normaliseChainOrder (pe.fx);
        }
    }
    if (! sawMacro) defaultMacros (p);
}

void SamplerProcessor::pastePad (int pad) {
    if (! padClipboard.isValid()) return;
    pushUndo();
    padFromVT (pad, padClipboard);
    maintenance();
    ++stateVersion;
}

void SamplerProcessor::setLayerMode (int pad, int mode) {
    auto& pe = pads[pad];
    mode = juce::jlimit (0, (int) LM_Velocity, mode);
    if (mode == LM_Velocity) {
        // fresh velocity mode (every loaded layer still spans 0..127): give each loaded
        // layer an equal slice, layer 1 softest, so switching modes is playable at once
        int loaded[NumLayers], n = 0;
        bool allFull = true;
        for (int li = 0; li < NumLayers; ++li)
            if (pe.layers[li].hasSample()) {
                loaded[n++] = li;
                allFull = allFull && pe.layers[li].loVel.load() == 0 && pe.layers[li].hiVel.load() == 127;
            }
        if (n > 1 && allFull) {
            pushUndo();
            for (int k = 0; k < n; ++k) {
                pe.layers[loaded[k]].loVel = k * 128 / n;
                pe.layers[loaded[k]].hiVel = (k + 1) * 128 / n - 1;
            }
        }
    }
    pe.layerMode = mode;
    pe.rrNext = 0;
    ++stateVersion;
}

void SamplerProcessor::clearPad (int pad) {
    pushUndo();
    juce::ValueTree blank ("PAD");
    padFromVT (pad, blank);
    for (int li = 0; li < NumLayers; ++li) clearLayer (pad, li, false);
    FxProcessor::clearChain (pads[pad].fx);
    ++stateVersion;
}

// ---------------- kits / state ----------------
const juce::String SamplerProcessor::getProgramName (int i) { return presetName (i); }

void SamplerProcessor::loadKit (int idx) {
    pushUndo();
    if (applyPreset (*this, idx)) return;
    initKitNoUndo();
}

void SamplerProcessor::loadStateAsync (const juce::ValueTree& kit, const juce::File& baseDir, std::function<void()> onApplied) {
    const int gen = ++loadGen;
    auto apply = [this, kit, baseDir, onApplied] {
        pushUndo();
        applyStateVT (kit, baseDir);   // samples are pool hits now: no decoding here
        if (onApplied) onApplied();
        ++stateVersion;
    };
    if (! asyncLoads) { apply(); return; }
    std::vector<juce::Array<juce::File>> cands;   // per layer, in restoreLayer's order
    for (auto pad : kit)
        if (pad.hasType ("PAD"))
            for (auto lv : pad)
                if (lv.hasType ("LAYER")) cands.push_back (layerCandidates (lv, baseDir));
    ++pendingLoads;
    loader.addJob ([this, gen, cands, apply, token = alive] {
        std::vector<SampleData::Ptr> hold;   // keeps the decoded samples in the pool until applied
        for (auto& list : cands) {
            if (loadGen.load() != gen) break;   // superseded: stop decoding
            for (auto& f : list)
                if (auto d = loadSampleFile (f)) { hold.push_back (d); break; }
        }
        juce::MessageManager::callAsync ([this, gen, apply, token, hold] {
            if (! *token) return;
            --pendingLoads;
            if (loadGen.load() == gen) apply();
        });
    });
}

void SamplerProcessor::loadKitAsync (int idx) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == idx) {
            auto vt = readKitFile (pr.file);
            if (! vt.isValid()) break;
            auto name = pr.name;
            loadStateAsync (vt, pr.file.getParentDirectory(), [this, idx, name] { currentKit = idx; kitName = name; });
            return;
        }
    loadKit (idx);   // not loadable: the synchronous path falls back to INIT
}

bool SamplerProcessor::loadUserKitAsync (const juce::File& file) {
    auto vt = readKitFile (file);
    if (! vt.isValid()) return false;
    auto name = file.getFileNameWithoutExtension();
    loadStateAsync (vt, file.getParentDirectory(), [this, name] { currentKit = -1; kitName = name; });
    return true;
}

void SamplerProcessor::initKit() { pushUndo(); initKitNoUndo(); }

void SamplerProcessor::initKitNoUndo() {
    currentKit = -1;
    kitName = "Init";
    kitBaseDir = juce::File();
    juce::ValueTree blank ("PAD");
    for (int p = 0; p < NumPads; ++p) {
        padFromVT (p, blank);
        for (int li = 0; li < NumLayers; ++li) { pads[p].layers[li].set (nullptr); pads[p].layers[li].path = {}; pads[p].layers[li].missing = false; pads[p].layers[li].resetParams(); }
        FxProcessor::clearChain (pads[p].fx);
    }
    FxProcessor::clearChain (globalFx);
    revDecay = 0.5f; revDamp = 0.5f; dlyTime = 0.35f; dlyFb = 0.35f; dlySync = -1;
    if (auto* m = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("master")))
        m->setValueNotifyingHost (m->convertTo0to1 (0.8f));
    ++stateVersion;
}

juce::ValueTree SamplerProcessor::fullStateVT() {
    juce::ValueTree v ("DRUMSAMPLER");
    v.setProperty ("version", 1, nullptr);
    v.setProperty ("kit", currentKit, nullptr);
    v.setProperty ("kitName", kitName, nullptr);
    v.setProperty ("master", masterRaw ? masterRaw->load() : masterGain.load(), nullptr);
    v.setProperty ("limiter", limiterOn.load(), nullptr);
    v.setProperty ("selected", selectedPad.load(), nullptr);
    v.setProperty ("revDecay", revDecay.load(), nullptr); v.setProperty ("revDamp", revDamp.load(), nullptr);
    v.setProperty ("dlyTime", dlyTime.load(), nullptr);   v.setProperty ("dlyFb", dlyFb.load(), nullptr);
    v.setProperty ("dlySync", dlySync.load(), nullptr);
    for (int p = 0; p < NumPads; ++p) v.addChild (padToVT (p), -1, nullptr);
    juce::ValueTree g ("GLOBALFX");
    g.addChild (globalFx.toVT(), -1, nullptr);
    v.addChild (g, -1, nullptr);
    return v;
}

void SamplerProcessor::applyStateVT (const juce::ValueTree& v, const juce::File& baseDir) {
    currentKit = (int) v.getProperty ("kit", -1);
    kitName = v.getProperty ("kitName", "Kit").toString();
    if (baseDir != juce::File()) kitBaseDir = baseDir;
    if (auto* m = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("master")))
        m->setValueNotifyingHost (m->convertTo0to1 ((float) v.getProperty ("master", 0.8f)));
    limiterOn = (bool) v.getProperty ("limiter", true);
    revDecay = (float) v.getProperty ("revDecay", 0.5f); revDamp = (float) v.getProperty ("revDamp", 0.5f);
    dlyTime = (float) v.getProperty ("dlyTime", 0.35f);  dlyFb = (float) v.getProperty ("dlyFb", 0.35f);
    dlySync = juce::jlimit (-1, TempoSync::NumDivisions - 1, (int) v.getProperty ("dlySync", -1)); // older files: free time
    selectedPad = juce::jlimit (0, NumPads - 1, (int) v.getProperty ("selected", 0));
    bool seen[NumPads] = {};
    juce::ValueTree blank ("PAD");
    for (auto c : v) {
        if (c.hasType ("PAD")) {
            int p = (int) c.getProperty ("id", -1);
            if (p < 0 || p >= NumPads) continue;
            seen[p] = true;
            // missing children in a hand-edited file must not inherit the previous kit
            for (int li = 0; li < NumLayers; ++li) { pads[p].layers[li].set (nullptr); pads[p].layers[li].path = {}; pads[p].layers[li].resetParams(); }
            FxProcessor::clearChain (pads[p].fx);
            padFromVT (p, c, baseDir);
        } else if (c.hasType ("GLOBALFX") && c.getNumChildren() > 0) {
            globalFx.fromVT (c.getChild (0));
            FxProcessor::normaliseChainOrder (globalFx);
        }
    }
    for (int p = 0; p < NumPads; ++p)
        if (! seen[p]) {
            padFromVT (p, blank);
            for (int li = 0; li < NumLayers; ++li) { pads[p].layers[li].set (nullptr); pads[p].layers[li].path = {}; pads[p].layers[li].resetParams(); }
            FxProcessor::clearChain (pads[p].fx);
        }
    if (! v.getChildWithName ("GLOBALFX").isValid()) FxProcessor::clearChain (globalFx);
    maintenance();
    ++stateVersion;
}

void SamplerProcessor::loadFullStateVT (const juce::ValueTree& v, const juce::File& baseDir, bool undoable) {
    if (! v.isValid() || ! v.hasType ("DRUMSAMPLER")) return;
    if (undoable) pushUndo();
    applyStateVT (v, baseDir);
}

void SamplerProcessor::getStateInformation (juce::MemoryBlock& dest) {
    juce::MemoryOutputStream mos (dest, true);
    juce::ValueTree root ("ROOT");
    auto st = fullStateVT();
    if (kitBaseDir != juce::File()) st.setProperty ("kitDir", kitBaseDir.getFullPathName(), nullptr);
    root.addChild (st, -1, nullptr);
    juce::ValueTree lk ("LOCKS"); // randomize locks: session-only
    for (int p = 0; p < NumPads; ++p) {
        juce::String bits;
        for (int k = 0; k < NumLocks; ++k) bits << (locks[p][k] ? "1" : "0");
        lk.setProperty ("p" + juce::String (p), bits, nullptr);
    }
    root.addChild (lk, -1, nullptr);
    root.writeToStream (mos);
}

void SamplerProcessor::setStateInformation (const void* data, int size) {
    juce::MemoryInputStream mis (data, (size_t) size, false);
    auto root = juce::ValueTree::readFromStream (mis);
    if (! root.isValid()) return;
    for (auto c : root) {
        if (c.hasType ("DRUMSAMPLER")) {
            auto dir = c.getProperty ("kitDir").toString();
            loadFullStateVT (c, juce::File::isAbsolutePath (dir) ? juce::File (dir) : juce::File(), false);
        }
        if (c.hasType ("LOCKS"))
            for (int p = 0; p < NumPads; ++p) {
                auto bits = c.getProperty ("p" + juce::String (p)).toString();
                for (int k = 0; k < NumLocks; ++k) locks[p][k] = k < bits.length() && bits[k] == '1';
            }
    }
}

// ---------------- undo ----------------
void SamplerProcessor::pushUndoEntry (juce::ValueTree entry) {
    undoStack.push_back (std::move (entry));
    if (undoStack.size() > MaxUndo) undoStack.erase (undoStack.begin());
    redoStack.clear();   // a new edit forks history
}
void SamplerProcessor::pushUndo() { pushUndoEntry (fullStateVT()); }

juce::ValueTree SamplerProcessor::paramEntry (int index) const {
    juce::ValueTree e ("PARAM");
    e.setProperty ("index", index, nullptr);
    if (auto* prm = getParameters()[index]) e.setProperty ("value", prm->getValue(), nullptr);
    return e;
}
void SamplerProcessor::applyParamEntry (const juce::ValueTree& e) {
    auto* prm = getParameters()[(int) e.getProperty ("index", -1)];
    if (prm == nullptr) return;
    const juce::ScopedValueSetter<bool> svs (gestureUndo.applying, true);
    prm->beginChangeGesture();   // hosts writing automation see a normal edit
    prm->setValueNotifyingHost ((float) e.getProperty ("value", prm->getValue()));
    prm->endChangeGesture();
    ++stateVersion;
}

void SamplerProcessor::undo() {
    if (undoStack.empty()) return;
    auto v = undoStack.back(); undoStack.pop_back();
    if (v.hasType ("PARAM")) { redoStack.push_back (paramEntry ((int) v.getProperty ("index"))); applyParamEntry (v); return; }
    redoStack.push_back (fullStateVT());
    applyStateVT (v, {});
}
void SamplerProcessor::redo() {
    if (redoStack.empty()) return;
    auto v = redoStack.back(); redoStack.pop_back();
    if (v.hasType ("PARAM")) { undoStack.push_back (paramEntry ((int) v.getProperty ("index"))); applyParamEntry (v); return; }
    undoStack.push_back (fullStateVT());
    applyStateVT (v, {});
}

// gestures arrive on the message thread (they come from a UI, never from host automation)
void SamplerProcessor::GestureUndo::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) {
    if (applying || ! juce::MessageManager::existsAndIsCurrentThread() || index < 0 || index >= (int) startValue.size()) return;
    if (auto* prm = proc.getParameters()[index]) startValue[(size_t) index] = prm->getValue();
}
void SamplerProcessor::GestureUndo::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) {
    if (applying || ! juce::MessageManager::existsAndIsCurrentThread() || index < 0 || index >= (int) startValue.size()) return;
    const float was = startValue[(size_t) index];
    startValue[(size_t) index] = -1.f;
    auto* prm = proc.getParameters()[index];
    if (was < 0.f || prm == nullptr || std::abs (prm->getValue() - was) < 1.0e-6f) return; // a click without a change
    auto e = proc.paramEntry (index);
    e.setProperty ("value", was, nullptr);
    proc.pushUndoEntry (e);
}

// ---------------- randomize / mutate ----------------
static float spreadFor (int i) {
    switch (i) {
        case PP_Tune: case PP_PitchAmt: return 0.12f;  // pitch stays near the source
        case PP_Cutoff: return 0.35f;
        default: return 0.45f;
    }
}

// FX knobs that only colour the sound (no dynamics, levels or timing):
// COMP / TRANSIENT are skipped entirely, as are DIST OUT and DELAY TIME.
static bool isTimbreFxKnob (int type, int k) {
    switch (type) {
        case FX_EQ: return k <= 4;
        case FX_DIST: return k == 0 || k == 1 || k == 4;
        case FX_FILTER: return k == 0 || k == 1 || k == 4;
        case FX_REVERB: return k <= 4;
        case FX_DELAY: return k >= 2 && k <= 4;
        default: return false;
    }
}

static void jitterFxTimbre (FxChainState& ch, juce::Random& rng, float amount) {
    for (auto& sl : ch.slots) {
        int t = sl.type.load();
        if (sl.bypass || t == FX_NONE) continue;
        std::atomic<float>* knobs[5] = { &sl.p0, &sl.p1, &sl.p2, &sl.p3, &sl.p4 };
        for (int k = 0; k < 5; ++k)
            if (isTimbreFxKnob (t, k))
                knobs[k]->store (juce::jlimit (0.f, 1.f, knobs[k]->load() + (rng.nextFloat() * 2.f - 1.f) * amount));
    }
}

void SamplerProcessor::randomizePad (int p, float intensity) {
    pushUndo();
    for (int i = 0; i < NumPadParams; ++i) {
        if (! isTimbreParam (i) || isLocked (p, i)) continue;
        if (i == PP_SendRev || i == PP_SendDly) continue; // sends: own roll below
        float cur = ranges[i].convertTo0to1 (getPadParam (p, i));
        float wild = ranges[i].convertTo0to1 (padParamMeta (i).def) + (liveRng.nextFloat() * 2.f - 1.f) * spreadFor (i) * 2.f * intensity;
        setPadParam (p, i, ranges[i].convertFrom0to1 (juce::jlimit (0.f, 1.f, cur * (1.f - intensity) + wild * intensity)));
    }
    if (! isLocked (p, PP_SendRev)) setPadParam (p, PP_SendRev, liveRng.nextFloat() < 0.4f ? liveRng.nextFloat() * 30.f * intensity : 0.f);
    if (! isLocked (p, PP_SendDly)) setPadParam (p, PP_SendDly, liveRng.nextFloat() < 0.2f ? liveRng.nextFloat() * 25.f * intensity : 0.f);
    if (! isLocked (p, LockFMode) && liveRng.nextFloat() < 0.25f * intensity)
        if (auto* fm = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (padFilterModeID (p))))
            fm->setValueNotifyingHost (fm->convertTo0to1 ((float) liveRng.nextInt (3)));
    if (! isLocked (p, LockLayers))
        for (auto& L : pads[p].layers) {
            if (! L.hasSample()) continue;
            L.v[LP_Tune] = juce::jlimit (-24.f, 24.f, (liveRng.nextFloat() * 2.f - 1.f) * 3.f * intensity);
        }
    // macros are left alone: LENGTH etc. reach the envelope
    if (! isLocked (p, LockFx)) jitterFxTimbre (pads[p].fx, liveRng, 0.35f * intensity);
    kitName = "Randomized Pad " + juce::String (p + 1);
    ++stateVersion;
}

// fresh musical FX chain from a generic pool (pads have no drum type)
void SamplerProcessor::randomizePadFx (int p, float intensity) {
    auto& ch = pads[p].fx;
    auto r = [this] (float a, float b) { return a + liveRng.nextFloat() * (b - a); };
    static const int pool[] = { FX_EQ, FX_DIST, FX_FILTER, FX_REVERB, FX_DELAY }; // no dynamics processors
    static const int distAlgos[] = { 0, 1, 2, 4, 5, 6 };
    int maxFx = 1 + (int) std::round (2.f * intensity);
    int n = liveRng.nextInt (maxFx + 1);
    for (int s = 0; s < FxChainState::NumSlots; ++s) {
        auto& sl = ch.slots[(size_t) s];
        if (s >= n) { sl.type.store (FX_NONE); sl.bypass.store (false); continue; }
        int type = pool[liveRng.nextInt ((int) std::size (pool))];
        FxProcessor::defaultSlot (type, sl);
        switch (type) {
            case FX_EQ:        sl.setParams (r (.35f,.65f), r (.35f,.65f), r (.35f,.65f), r (.35f,.65f), .5f, 1.f); break;
            case FX_DIST: {
                int a = distAlgos[liveRng.nextInt ((int) std::size (distAlgos))];
                sl.setParams (r (.05f,.3f), r (.35f,.7f), (a + .5f) / 8.f, r (.4f,.5f), 0.f, r (.25f,.6f));
                break;
            }
            case FX_REVERB:    sl.setParams (r (.2f,.6f), r (.3f,.9f), r (.3f,.8f), r (0.f,.3f), r (.3f,.7f), r (.04f,.12f)); break;
            case FX_DELAY:     sl.setParams (r (.25f,.5f), r (.15f,.4f), r (.3f,.6f), r (.1f,.4f), r (0.f,.3f), r (.05f,.15f)); break;
            case FX_FILTER:    sl.setParams (r (.5f,.85f), r (.05f,.3f), (float) liveRng.nextInt (3) / 3.f + 0.1f, liveRng.nextBool() ? 0.f : 1.f, r (0.f,.2f), 1.f); break;
            default: break;
        }
    }
    ch.order.reset();
}

void SamplerProcessor::randomizeKit (float intensity) {
    pushUndo();
    auto saved = undoStack;  // randomizePad pushes too: keep one undo step
    for (int p = 0; p < NumPads; ++p) {
        if (numLoadedLayers (p) == 0) continue;
        randomizePad (p, intensity);
        if (! isLocked (p, LockFx)) randomizePadFx (p, intensity);
    }
    undoStack = saved;
    maintenance();
    kitName = "Randomized Kit";
    ++stateVersion;
}

void SamplerProcessor::mutatePad (int p, float amount) {
    pushUndo();
    for (int i = 0; i < NumPadParams; ++i) {   // timbre only (see isTimbreParam)
        if (! isTimbreParam (i) || isLocked (p, i)) continue;
        float n = ranges[i].convertTo0to1 (getPadParam (p, i)) + (liveRng.nextFloat() * 2.f - 1.f) * amount * 0.25f;
        setPadParam (p, i, ranges[i].convertFrom0to1 (juce::jlimit (0.f, 1.f, n)));
    }
    if (! isLocked (p, LockLayers))
        for (auto& L : pads[p].layers)
            if (L.hasSample()) L.v[LP_Fine] = juce::jlimit (-100.f, 100.f, L.v[LP_Fine].load() + (liveRng.nextFloat() * 2.f - 1.f) * 40.f * amount);
    if (! isLocked (p, LockFx)) jitterFxTimbre (pads[p].fx, liveRng, 0.32f * amount);
    kitName = "Mutated Pad " + juce::String (p + 1);
    ++stateVersion;
}

void SamplerProcessor::mutateKit (float amount) {
    pushUndo();
    auto saved = undoStack;
    for (int p = 0; p < NumPads; ++p) if (numLoadedLayers (p) > 0) mutatePad (p, amount);
    undoStack = saved;
    kitName = "Mutated Kit";
    ++stateVersion;
}

} // namespace mds

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new mds::SamplerProcessor(); }
