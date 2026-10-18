// End-to-end tests (dev only, never shipped): drives the real processor the
// way a host + the UI do (APVTS params, MIDI, bus layouts, kit files) and
// measures the rendered audio. Exit code 0 = all good.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FileLibrary.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <functional>

using namespace mds;

static int fails = 0;
#define CHECK(cond, ...) do { bool ok_ = (cond); printf ("%s  ", ok_ ? "  ok " : "FAIL "); printf (__VA_ARGS__); printf ("\n"); if (! ok_) ++fails; } while (0)

static constexpr double SR = 48000.0;
static constexpr int BS = 256;

static juce::File tmpRoot;

// ---- test audio files ----
static juce::File writeWav (const juce::String& name, double sr, int nch, int len, std::function<float (int ch, int i)> gen) {
    auto f = tmpRoot.getChildFile ("src").getChildFile (name);
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::AudioBuffer<float> b (nch, len);
    for (int c = 0; c < nch; ++c) for (int i = 0; i < len; ++i) b.setSample (c, i, gen (c, i));
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) nch, 24, {}, 0));
    if (w != nullptr) { os.release(); w->writeFromAudioSampleBuffer (b, 0, len); }
    return f;
}
static juce::File sineFile (const juce::String& name, float amp, double secs, float hz = 220.f, double sr = 44100.0) {
    return writeWav (name, sr, 1, (int) (secs * sr), [=] (int, int i) { return amp * std::sin (juce::MathConstants<float>::twoPi * hz * (float) (i / sr)); });
}

// ---- rendering ----
using Events = std::vector<std::pair<int, juce::MidiMessage>>;
static juce::AudioBuffer<float> render (SamplerProcessor& proc, int n, const Events& ev = {}) {
    int ch = proc.getTotalNumOutputChannels();
    juce::AudioBuffer<float> out (ch, n);
    out.clear();
    for (int pos = 0; pos < n; pos += BS) {
        int len = juce::jmin (BS, n - pos);
        juce::AudioBuffer<float> blk (ch, len);
        juce::MidiBuffer mb;
        for (auto& e : ev) if (e.first >= pos && e.first < pos + len) mb.addEvent (e.second, e.first - pos);
        proc.processBlock (blk, mb);
        for (int c = 0; c < ch; ++c) out.copyFrom (c, pos, blk, c, 0, len);
    }
    return out;
}
static Events hit (int note, int at = 0, float vel = 1.f) { return { { at, juce::MidiMessage::noteOn (1, note, vel) } }; }
static int busCh (SamplerProcessor& proc, int bus) { return proc.getChannelIndexInProcessBlockBuffer (false, bus, 0); }
static float peak (const juce::AudioBuffer<float>& b, int ch, int from = 0, int to = -1) {
    if (to < 0) to = b.getNumSamples();
    if (ch < 0 || ch >= b.getNumChannels() || to <= from) return 0.f;
    return b.getMagnitude (ch, from, to - from);
}
static float rms (const juce::AudioBuffer<float>& b, int ch, int from, int to) {
    if (to <= from) return 0.f;
    return b.getRMSLevel (ch, from, to - from);
}
static int firstAbove (const juce::AudioBuffer<float>& b, int ch, float thr) {
    for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (ch, i)) > thr) return i;
    return -1;
}
static int lastAbove (const juce::AudioBuffer<float>& b, int ch, float thr) {
    for (int i = b.getNumSamples(); --i >= 0;) if (std::abs (b.getSample (ch, i)) > thr) return i;
    return -1;
}
static bool allFinite (const juce::AudioBuffer<float>& b) {
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}
static void flush() { juce::MessageManager::getInstance()->runDispatchLoopUntil (20); }

// fresh processor, all buses on, empty kit, limiter off (measure raw levels)
static std::unique_ptr<SamplerProcessor> makeProc (bool aux = true) {
    auto p = std::make_unique<SamplerProcessor>();
    if (aux) p->enableAllBuses();
    p->prepareToPlay (SR, BS);
    p->initKitNoUndo();
    p->limiterOn = false;
    if (auto* m = p->apvts.getParameter ("master")) m->setValueNotifyingHost (m->convertTo0to1 (1.f));
    p->pullParamsForTest();
    return p;
}

int main() {
    juce::ScopedJuceInitialiser_GUI juceInit;
    setvbuf (stdout, nullptr, _IONBF, 0);
    tmpRoot = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PastelSamplerE2E");
    tmpRoot.deleteRecursively();
    tmpRoot.createDirectory();
    setUserPresetDirForTest (tmpRoot.getChildFile ("UserKits"));
    FileLibrary::setSettingsFolderForTest (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PastelSamplerE2ESettings").getFullPathName()); // never touch the user's shortcuts
    setFactoryDirsForTest (tmpRoot.getChildFile ("NoFactory"), tmpRoot.getChildFile ("NoFactoryOverride"));
    rescanFactoryPresets();

    auto quiet = sineFile ("quiet.wav", 0.2f, 0.3);
    auto loud = sineFile ("loud.wav", 0.6f, 0.3);
    auto longF = sineFile ("long.wav", 0.5f, 1.0);
    auto ramp = writeWav ("ramp.wav", 44100.0, 1, 22050, [] (int, int i) { return (float) i / 22050.f * 0.8f * (i % 2 ? 1.f : -1.f); });
    auto stereo = writeWav ("stereoL.wav", 44100.0, 2, 13230, [] (int c, int i) { return c == 0 ? 0.5f * std::sin (0.05f * (float) i) : 0.f; });

    // ---- 1. load + MIDI note mapping ----
    {
        auto pp = makeProc(); auto& p = *pp;
        CHECK (p.loadLayer (0, 0, loud), "load wav into pad 1 / layer 1");
        CHECK (p.numLoadedLayers (0) == 1 && p.padDisplayName (0) == "loud", "pad name follows the file (%s)", p.padDisplayName (0).toRawUTF8());
        auto b = render (p, 20000, hit (36));
        CHECK (peak (b, 0) > 0.4f, "note 36 plays pad 1 on Main (peak %.3f)", peak (b, 0));
        CHECK (peak (b, busCh (p, 1)) < 1.0e-6f, "aux out 2 stays silent");
        auto c = render (p, 20000, hit (37));
        CHECK (peak (c, 0) < 1.0e-6f, "note 37 (empty pad 2) is silent");
        CHECK (allFinite (b), "output finite");
        CHECK (! p.loadLayer (0, 1, tmpRoot.getChildFile ("nope.wav")), "missing file refused, layer untouched");
    }
    // ---- 2. sample-accurate MIDI ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, loud);
        auto b = render (p, 4000, hit (36, 1000));
        int on = firstAbove (b, 0, 1.0e-5f) - p.getLatencySamples();   // the limiter's lookahead (reported to the host)
        CHECK (p.getLatencySamples() > 0 && on >= 1000 && on < 1010, "note at sample 1000 starts at %d (+%d latency)", on, p.getLatencySamples());
    }
    // ---- 3. multi-out routing ----
    {
        auto pp = makeProc(); auto& p = *pp;
        CHECK (p.getBusCount (false) == 1 + NumAuxOuts, "1 main + %d aux stereo buses (%d)", NumAuxOuts, p.getBusCount (false));
        p.loadLayer (0, 0, loud);
        p.pads[0].output = 2; // Out 3
        auto b = render (p, 20000, hit (36));
        CHECK (peak (b, 0) < 1.0e-6f, "routed pad leaves Main silent (%.4f)", peak (b, 0));
        CHECK (peak (b, busCh (p, 2)) > 0.4f && peak (b, busCh (p, 2) + 1) > 0.4f, "pad 1 lands on Out 3 L+R (%.3f)", peak (b, busCh (p, 2)));
        CHECK (peak (b, busCh (p, 1)) < 1.0e-6f, "other aux bus silent");
        p.pads[0].output = NumAuxOuts; // last bus
        auto c = render (p, 20000, hit (36));
        CHECK (peak (c, busCh (p, NumAuxOuts)) > 0.4f, "Out %d works", NumAuxOuts + 1);
        // disabled aux falls back to Main
        auto qq = makeProc (false); auto& q = *qq;
        q.loadLayer (0, 0, loud);
        q.pads[0].output = 3;
        auto d = render (q, 20000, hit (36));
        CHECK (q.getTotalNumOutputChannels() == 2 && peak (d, 0) > 0.4f, "disabled aux bus falls back to Main (%.3f)", peak (d, 0));
        CHECK (q.isBusesLayoutSupported (q.getBusesLayout()), "default layout supported");
    }
    // ---- 4. layers: stack vs round robin ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, quiet); p.loadLayer (0, 1, loud);
        p.pads[0].layerMode = LM_Stack;
        auto s = render (p, 15000, hit (36));
        CHECK (peak (s, 0) > 0.75f, "stack plays both layers (peak %.3f ~ 0.8)", peak (s, 0));
        p.pads[0].layerMode = LM_RoundRobin; p.pads[0].rrNext = 0;
        float pk[4];
        for (int i = 0; i < 4; ++i) { auto b = render (p, 15000, hit (36)); pk[i] = peak (b, 0); render (p, 6000); }
        CHECK (std::abs (pk[0] - 0.2f) < 0.03f && std::abs (pk[1] - 0.6f) < 0.05f && std::abs (pk[2] - 0.2f) < 0.03f && std::abs (pk[3] - 0.6f) < 0.05f,
               "round robin alternates L1/L2 (%.2f %.2f %.2f %.2f)", pk[0], pk[1], pk[2], pk[3]);
        p.loadLayer (0, 3, longF); // gaps skipped: L1, L2, L4
        p.pads[0].rrNext = 0;
        int nonEmpty = 0;
        for (int i = 0; i < 3; ++i) { auto b = render (p, 4000, hit (36)); nonEmpty += peak (b, 0) > 0.1f ? 1 : 0; render (p, 60000); }
        CHECK (nonEmpty == 3, "round robin skips the empty layer (3/3 hits sounded)");
    }
    // ---- 5. choke groups, mono, gated ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, longF);
        p.loadLayer (1, 0, quiet);
        int at = (int) (0.1 * SR);
        Events ev = hit (36); ev.push_back ({ at, juce::MidiMessage::noteOn (1, 37, 1.f) });
        auto free = render (p, (int) (0.5 * SR), ev);
        p.pads[0].choke = 1; p.pads[1].choke = 1;
        auto choked = render (p, (int) (0.5 * SR), ev);
        int w0 = (int) (0.42 * SR), w1 = (int) (0.5 * SR); // after pad 2's own sample ended
        CHECK (rms (free, 0, w0, w1) > 0.2f && rms (choked, 0, w0, w1) < 1.0e-4f, "choke group cuts pad 1 (%.3f -> %.5f)", rms (free, 0, w0, w1), rms (choked, 0, w0, w1));
        // mono: second hit of the same pad cuts the first (no overlap build-up)
        p.pads[0].choke = 0; p.pads[1].choke = 0;
        Events twice = hit (36); twice.push_back ({ 2000, juce::MidiMessage::noteOn (1, 36, 1.f) });
        p.pads[0].voiceMode = VM_Poly;
        auto poly = render (p, 30000, twice);
        render (p, 60000);
        p.pads[0].voiceMode = VM_Mono;
        auto mono = render (p, 30000, twice);
        render (p, 60000);
        CHECK (peak (poly, 0, 3000) > peak (mono, 0, 3000) * 1.3f, "mono retrigger: no overlap (poly %.2f vs mono %.2f)", peak (poly, 0, 3000), peak (mono, 0, 3000));
        // gated: note-off -> RELEASE
        p.pads[0].voiceMode = VM_Gated;
        p.setPadParam (0, PP_Release, 20.f);
        p.pullParamsForTest();
        Events g = hit (36); g.push_back ({ (int) (0.1 * SR), juce::MidiMessage::noteOff (1, 36) });
        auto gated = render (p, (int) (0.4 * SR), g);
        CHECK (rms (gated, 0, (int) (0.2 * SR), (int) (0.4 * SR)) < 1.0e-4f && rms (gated, 0, 0, (int) (0.09 * SR)) > 0.2f, "gated pad releases on note-off");
        p.pads[0].voiceMode = VM_Poly;
        auto oneShot = render (p, (int) (0.4 * SR), g);
        CHECK (rms (oneShot, 0, (int) (0.2 * SR), (int) (0.4 * SR)) > 0.2f, "poly pad ignores note-off (one-shot)");
    }
    // ---- 6. sample edits: start/end, reverse, tune, normalize, pan ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, longF); // 1 s
        p.pads[0].layers[0].v[LP_End] = 0.5f;
        auto b = render (p, (int) SR, hit (36));
        int last = lastAbove (b, 0, 1.0e-4f);
        CHECK (std::abs (last - (int) (0.5 * SR)) < 400, "END 50%% stops at ~0.5 s (%d)", last);
        p.pads[0].layers[0].v[LP_End] = 1.f;
        p.pads[0].layers[0].v[LP_Tune] = 12.f;
        auto t = render (p, (int) SR, hit (36));
        last = lastAbove (t, 0, 1.0e-4f);
        CHECK (std::abs (last - (int) (0.5 * SR)) < 400, "+12 st plays twice as fast (%d)", last);
        p.pads[0].layers[0].v[LP_Tune] = 0.f;
        p.loadLayer (0, 0, ramp); // quiet start, loud end
        auto fwd = render (p, 4000, hit (36));
        p.pads[0].layers[0].reverse = true;
        auto rev = render (p, 4000, hit (36));
        CHECK (peak (rev, 0, 0, 4000) > peak (fwd, 0, 0, 4000) * 5.f, "REVERSE starts from the loud end (%.3f vs %.3f)", peak (rev, 0, 0, 4000), peak (fwd, 0, 0, 4000));
        p.pads[0].layers[0].reverse = false;
        render (p, (int) SR); // let every voice finish before measuring again
        p.loadLayer (0, 0, quiet);
        p.pads[0].layers[0].normalize = true;
        auto n = render (p, 15000, hit (36));
        CHECK (peak (n, 0) > 0.9f && peak (n, 0) < 1.01f, "NORMALIZE lifts a 0.2 peak to ~1 (%.3f)", peak (n, 0));
        render (p, (int) SR);
        p.pads[0].layers[0].normalize = false;
        p.pads[0].layers[0].v[LP_Pan] = -100.f;
        auto l = render (p, 15000, hit (36));
        CHECK (peak (l, 1) < 1.0e-5f && peak (l, 0) > 0.15f, "layer pan hard left (L %.3f R %.5f)", peak (l, 0), peak (l, 1));
        render (p, (int) SR);
        p.pads[0].layers[0].v[LP_Pan] = 0.f;
        p.loadLayer (0, 0, stereo);
        auto st = render (p, 15000, hit (36));
        CHECK (peak (st, 0) > 0.3f && peak (st, 1) < 1.0e-5f, "stereo files keep their channels");
    }
    // ---- 7. pad params, velocity ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, loud);
        auto full = render (p, 15000, hit (36, 0, 1.f));
        auto soft = render (p, 15000, hit (36, 0, 0.3f));
        CHECK (peak (soft, 0) < peak (full, 0) * 0.6f, "VEL>AMP: soft hits are quieter (%.2f vs %.2f)", peak (soft, 0), peak (full, 0));
        p.setPadParam (0, PP_Level, -60.f); p.pullParamsForTest();
        CHECK (peak (render (p, 15000, hit (36)), 0) < 1.0e-6f, "pad LEVEL -inf is silent");
        p.setPadParam (0, PP_Level, 0.f);
        p.setPadParam (0, PP_Hold, 50.f); p.setPadParam (0, PP_Decay, 20.f); p.pullParamsForTest();
        auto shortHit = render (p, 15000, hit (36));
        CHECK (rms (shortHit, 0, 6000, 12000) < 1.0e-4f, "HOLD + DECAY shorten the hit");
        p.setPadParam (0, PP_Hold, HoldFullMs); p.setPadParam (0, PP_Cutoff, 200.f); p.pullParamsForTest();
        auto dark = render (p, 15000, hit (36));
        CHECK (peak (dark, 0) < peak (full, 0) * 0.8f, "CUTOFF 200 Hz darkens a 220 Hz tone a bit (%.2f)", peak (dark, 0));
        p.setPadParam (0, PP_Cutoff, 20000.f); p.pullParamsForTest();
    }
    // ---- 8. macros ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, loud);
        CHECK (p.pads[0].macros[0].name == "PITCH" && p.macroDepth (0, 0, PP_Tune) > 0.f, "default macros are generic (PITCH/LENGTH/COLOR/SPACE)");
        p.setMacroTarget (0, 3, PP_Level, 1.f); // SPACE also drives level, full depth
        p.macroParam (0, 3)->setValueNotifyingHost (0.f); // bipolar -1 -> level floor
        p.pullParamsForTest();
        CHECK (p.pads[0].eff[PP_Level] <= -59.f, "macro down pulls LEVEL to the floor (%.1f dB)", p.pads[0].eff[PP_Level]);
        CHECK (peak (render (p, 15000, hit (36)), 0) < 1.0e-5f, "macro-muted pad is silent");
        p.macroParam (0, 3)->setValueNotifyingHost (0.5f);
        p.pullParamsForTest();
        CHECK (std::abs (p.pads[0].eff[PP_Level]) < 0.01f, "macro centre = neutral");
        CHECK (std::abs (p.getPadParam (0, PP_Level)) < 0.01f, "macros never write the base parameter");
        p.setMacroTarget (0, 3, PP_Level, 0.f);
        CHECK (p.macroDepth (0, 3, PP_Level) == 0.f, "depth 0 removes the target");
        // LENGTH macro: audible over its whole range on any sample length
        auto dec = writeWav ("decay.wav", 44100.0, 1, 26460, [] (int, int i) { return 0.8f * std::exp (-(float) i / 6000.f) * std::sin (0.07f * (float) i); });
        p.loadLayer (0, 0, dec);
        auto lenRender = [&] (float macro) {
            p.macroParam (0, 1)->setValueNotifyingHost (macro);
            p.pullParamsForTest();
            render (p, 48000);
            return render (p, 36000, hit (36));
        };
        auto full = lenRender (0.5f), half = lenRender (0.25f), shortest = lenRender (0.f), longer = lenRender (1.f);
        int eFull = lastAbove (full, 0, 1.0e-3f), eHalf = lastAbove (half, 0, 1.0e-3f), eShort = lastAbove (shortest, 0, 1.0e-3f);
        CHECK (eShort < eHalf && eHalf < eFull * 0.6, "LENGTH down shortens the sound step by step (%d < %d < %d samples)", eShort, eHalf, eFull);
        float tailFull = rms (full, 0, 9000, 20000), tailLong = rms (longer, 0, 9000, 20000);
        CHECK (tailLong > tailFull * 1.6f && peak (longer, 0, 0, 2000) < peak (full, 0, 0, 2000) * 1.15f,
               "LENGTH up lifts the tail (x%.1f), not the hit", tailLong / tailFull);
        p.macroParam (0, 1)->setValueNotifyingHost (0.5f);
        p.loadLayer (0, 0, loud);
    }
    // ---- 9. FX: lazy reverb memory, tails, stereo chain ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, loud);
        auto dry = render (p, (int) (0.8 * SR), hit (36));
        FxProcessor::defaultSlot (FX_REVERB, p.pads[0].fx.slots[0]);
        p.pads[0].fx.slots[0].p5 = 0.6f;
        auto before = render (p, (int) (0.8 * SR), hit (36));
        CHECK (rms (before, 0, (int) (0.5 * SR), (int) (0.8 * SR)) < 1.0e-5f, "reverb slot stays dry until memory exists");
        p.maintenance();
        CHECK (p.pads[0].fxDsp[0].heavyReady.load() && ! p.pads[0].fxDsp[1].heavyReady.load(), "only the reverb slot got reverb/delay memory");
        auto wet = render (p, (int) (0.8 * SR), hit (36));
        CHECK (rms (wet, 0, (int) (0.5 * SR), (int) (0.8 * SR)) > 1.0e-3f && rms (dry, 0, (int) (0.5 * SR), (int) (0.8 * SR)) < 1.0e-5f,
               "pad reverb tail rings past the sample (%.4f)", rms (wet, 0, (int) (0.5 * SR), (int) (0.8 * SR)));
        float dL = 0, dR = 0; // decorrelated stereo tail
        for (int i = (int) (0.4 * SR); i < (int) (0.8 * SR); ++i) { dL += std::abs (wet.getSample (0, i) - wet.getSample (1, i)); dR += std::abs (wet.getSample (0, i)); }
        CHECK (dL > dR * 0.05f, "reverb tail is stereo");
        p.setPadParam (0, PP_SendRev, 80.f); p.setPadParam (0, PP_SendDly, 50.f);
        for (int t : { FX_EQ, FX_COMP, FX_TRANSIENT, FX_DIST, FX_DELAY, FX_FILTER })
            for (int s = 1; s < FxChainState::NumSlots; ++s) if (p.pads[0].fx.slots[s].type.load() == FX_NONE) { FxProcessor::defaultSlot (t, p.pads[0].fx.slots[s]); break; }
        FxProcessor::defaultSlot (FX_DELAY, p.globalFx.slots[0]);
        FxProcessor::defaultSlot (FX_DIST, p.globalFx.slots[1]);
        p.maintenance(); p.pullParamsForTest();
        auto all = render (p, (int) SR, hit (36));
        CHECK (allFinite (all) && peak (all, 0) > 0.05f, "every FX type + sends + global chain: finite audio");
    }
    // ---- 10. state round trip (DAW project) ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (3, 0, quiet); p.loadLayer (3, 2, loud);
        p.pads[3].layers[2].v[LP_Start] = 0.25f; p.pads[3].layers[2].reverse = true;
        p.pads[3].output = 5; p.pads[3].choke = 2; p.pads[3].voiceMode = VM_Gated; p.pads[3].layerMode = LM_RoundRobin;
        p.pads[3].name = "MY PAD"; p.pads[3].macros[2].name = "GRIT";
        p.setPadParam (3, PP_Drive, 42.f);
        p.setMacroTarget (3, 2, PP_Punch, -0.5f);
        FxProcessor::defaultSlot (FX_COMP, p.pads[3].fx.slots[4]);
        p.setLocked (3, PP_Tune, true);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        auto qq = makeProc(); auto& q = *qq;
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        auto& r = q.pads[3];
        CHECK (r.layers[0].path == quiet.getFullPathName() && r.layers[2].hasSample() && ! r.layers[1].hasSample(), "layers restored");
        CHECK (std::abs (r.layers[2].v[LP_Start].load() - 0.25f) < 1e-4f && r.layers[2].reverse.load(), "layer settings restored");
        CHECK (r.output == 5 && r.choke == 2 && r.voiceMode == VM_Gated && r.layerMode == LM_RoundRobin, "routing / choke / modes restored");
        CHECK (r.name == "MY PAD" && r.macros[2].name == "GRIT", "names restored");
        CHECK (std::abs (q.getPadParam (3, PP_Drive) - 42.f) < 0.01f && std::abs (q.macroDepth (3, 2, PP_Punch) + 0.5f) < 1e-3f, "params + macro targets restored");
        CHECK (r.fx.slots[4].type.load() == FX_COMP && q.isLocked (3, PP_Tune), "FX chain + locks restored");
    }
    // ---- 11. kit files: collect vs reference ----
    {
        auto pp = makeProc(); auto& p = *pp;
        auto movable = sineFile ("movable.wav", 0.5f, 0.2);
        p.loadLayer (0, 0, movable); p.loadLayer (1, 0, loud);
        CHECK (saveUserPreset ("Collected Kit", p, true), "save kit with COLLECT SAMPLES");
        auto kitFile = getUserPresetDir().getChildFile ("Collected Kit.xml");
        auto colDir = getUserPresetDir().getChildFile ("Collected Kit Samples");
        CHECK (kitFile.existsAsFile() && colDir.getChildFile ("movable.wav").existsAsFile() && colDir.getChildFile ("loud.wav").existsAsFile(), "samples copied next to the kit");
        CHECK (p.pads[0].layers[0].path == colDir.getChildFile ("movable.wav").getFullPathName(), "after collecting, the kit plays the copies");
        CHECK (saveUserPreset ("Referenced Kit", p, false) && ! getUserPresetDir().getChildFile ("Referenced Kit Samples").exists(), "save without collect copies nothing");
        // move the whole kit folder: relative paths must follow
        auto moved = tmpRoot.getChildFile ("MovedKits");
        getUserPresetDir().moveFileTo (moved);
        auto qq = makeProc(); auto& q = *qq;
        CHECK (loadPresetFile (q, moved.getChildFile ("Collected Kit.xml")), "load moved kit");
        CHECK (q.pads[0].layers[0].hasSample() && q.pads[0].layers[0].path.startsWith (moved.getFullPathName()), "collected kit resolves samples after the move");
        // a referenced file that disappears is flagged, not dropped
        auto qq2 = makeProc(); auto& q2 = *qq2;
        auto gone = sineFile ("gone.wav", 0.5f, 0.2);
        q2.loadLayer (0, 0, gone);
        auto f = tmpRoot.getChildFile ("gone.xml");
        savePresetFile (q2, f, false);
        gone.deleteFile();
        auto qq3 = makeProc(); auto& q3 = *qq3;
        loadPresetFile (q3, f);
        CHECK (q3.pads[0].layers[0].missing && ! q3.pads[0].layers[0].hasSample() && q3.pads[0].layers[0].path.endsWith ("gone.wav"), "missing sample flagged, path kept");
        CHECK (peak (render (q3, 8000, hit (36)), 0) < 1.0e-6f, "missing sample plays silence");
        moved.moveFileTo (getUserPresetDir());
    }
    // ---- 12. undo, randomize, mutate, locks, copy/paste ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, loud); p.loadLayer (5, 0, quiet);
        p.setLocked (0, PP_Drive, true);
        float drive0 = p.getPadParam (0, PP_Drive), tone0 = p.getPadParam (0, PP_Tone);
        p.randomizeKit (1.f);
        CHECK (p.getPadParam (0, PP_Drive) == drive0, "locked param survives KIT RND");
        bool changed = false;
        for (int i = 0; i < NumPadParams; ++i) if (! padParamMeta (i).isEnv && std::abs (p.getPadParam (5, i) - padParamMeta (i).def) > 1e-3f) changed = true;
        CHECK (changed, "KIT RND changes loaded pads");
        CHECK (p.pads[0].layers[0].hasSample() && p.pads[5].layers[0].hasSample(), "randomize never touches which samples are loaded");
        {
            // behaviour + dynamics must survive KIT RND / MUTATE untouched
            auto snapshot = [&p] {
                std::vector<float> v;
                for (int pad = 0; pad < NumPads; ++pad) {
                    for (int i = 0; i < NumPadParams; ++i) if (! isTimbreParam (i)) v.push_back (p.getPadParam (pad, i));
                    for (auto& L : p.pads[pad].layers) { v.push_back (L.v[LP_Level].load()); v.push_back (L.v[LP_Pan].load()); }
                    for (int k = 0; k < NumMacros; ++k) v.push_back (p.macroParam (pad, k)->get());
                    v.push_back ((float) p.pads[pad].voiceMode.load()); v.push_back ((float) p.pads[pad].choke.load());
                }
                return v;
            };
            p.undo();
            auto before = snapshot();
            p.setPadParam (5, PP_Comp, 37.f); p.setPadParam (5, PP_Pan, -20.f); p.setPadParam (5, PP_Decay, 900.f);
            before = snapshot();
            for (int r = 0; r < 4; ++r) { p.randomizeKit (1.f); p.mutateKit (0.5f); p.randomizePad (5, 1.f); p.mutatePad (5, 1.f); }
            bool dynFx = false;
            for (auto& pe : p.pads) for (auto& sl : pe.fx.slots) dynFx |= sl.type.load() == FX_COMP || sl.type.load() == FX_TRANSIENT;
            CHECK (snapshot() == before && ! dynFx, "RANDOMIZE / MUTATE never touch level, pan, envelopes, comp, punch, velocity, macros, voice/choke (and add no dynamics FX)");
            for (int r = 0; r < 16; ++r) p.undo();   // 4 rounds x 4 undo steps
        }
        p.randomizeKit (1.f);
        p.undo();
        CHECK (std::abs (p.getPadParam (0, PP_Tone) - tone0) < 1e-3f, "one UNDO reverts the whole KIT RND");
        p.mutatePad (0, 0.3f); p.undo();
        CHECK (std::abs (p.getPadParam (0, PP_Tone) - tone0) < 1e-3f, "UNDO reverts MUTATE");
        p.randomizePad (0, 1.f);
        float toneR = p.getPadParam (0, PP_Tone);
        p.undo();
        p.redo();
        CHECK (std::abs (p.getPadParam (0, PP_Tone) - toneR) < 1e-3f && p.undoStack.size() > 0, "REDO re-applies what UNDO took back");
        p.undo();
        p.mutatePad (0, 0.3f);
        CHECK (p.redoStack.empty(), "a new edit clears REDO");
        p.copyPad (0); p.pastePad (9);
        CHECK (p.pads[9].layers[0].hasSample() && p.padDisplayName (9) == "loud", "copy / paste pad");
        p.clearPad (9);
        CHECK (! p.pads[9].layers[0].hasSample() && p.padDisplayName (9).isEmpty(), "clear pad");
        p.undo();
        CHECK (p.pads[9].layers[0].hasSample(), "UNDO restores a cleared pad");
        juce::Array<juce::File> drop { quiet, loud, longF, ramp, stereo };
        p.loadPadFiles (2, drop);
        CHECK (p.numLoadedLayers (2) == 4, "dropping 5 files fills 4 layers");
    }
    // ---- 12b. limiter, voice steal, velocity layers, tempo sync, knob undo, async kits ----
    {
        auto pp = makeProc(); auto& p = *pp;
        auto hot = sineFile ("hot.wav", 0.99f, 0.3);
        for (int l = 0; l < NumLayers; ++l) p.loadLayer (0, l, hot);
        p.setPadParam (0, PP_Level, 6.f);
        p.limiterOn = true;
        auto b = render (p, 12000, hit (36));
        CHECK (peak (b, 0) <= 0.9662f && peak (b, 0) > 0.9f && allFinite (b), "lookahead limiter holds -0.3 dBFS (peak %.4f)", peak (b, 0));
        // aux buses get the same lookahead delay as Main
        p.limiterOn = false; p.setPadParam (0, PP_Level, 0.f);
        p.pads[1].output = 0; p.loadLayer (1, 0, hot);
        p.pads[2].output = 3; p.loadLayer (2, 0, hot);
        render (p, 48000);
        Events two { { 500, juce::MidiMessage::noteOn (1, 37, 1.f) }, { 500, juce::MidiMessage::noteOn (1, 38, 1.f) } };
        auto c = render (p, 4000, two);
        CHECK (firstAbove (c, 0, 1.0e-4f) == firstAbove (c, busCh (p, 3), 1.0e-4f), "aux bus stays sample-aligned with Main (%d vs %d)",
               firstAbove (c, 0, 1.0e-4f), firstAbove (c, busCh (p, 3), 1.0e-4f));
    }
    {
        auto pp = makeProc(); auto& p = *pp;
        auto longHot = sineFile ("steal.wav", 0.3f, 2.0, 100.f);
        p.loadLayer (0, 0, longHot);
        Events many;
        for (int k = 0; k < MaxVoices + 8; ++k) many.push_back ({ k * 64, juce::MidiMessage::noteOn (1, 36, 1.f) });
        auto b = render (p, (MaxVoices + 8) * 64 + 2000, many);
        float maxStep = 0.f;
        for (int i = 1; i < b.getNumSamples(); ++i) maxStep = juce::jmax (maxStep, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
        CHECK (p.activeVoices.load() <= MaxVoices && allFinite (b), "voice pool capped at %d (%d)", MaxVoices, p.activeVoices.load());
        CHECK (maxStep < 1.5f, "stolen voices fade, no hard cut (max step %.3f)", maxStep);
    }
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (0, 0, quiet); p.loadLayer (0, 1, loud);
        p.setPadParam (0, PP_VelAmp, 0.f);
        p.setLayerMode (0, LM_Velocity);
        CHECK (p.pads[0].layers[0].hiVel == 63 && p.pads[0].layers[1].loVel == 64, "VELOCITY mode splits full-range layers (0-%d / %d-127)",
               p.pads[0].layers[0].hiVel.load(), p.pads[0].layers[1].loVel.load());
        auto soft = render (p, 6000, hit (36, 0, 40.f / 127.f));
        render (p, 48000);
        auto hard = render (p, 6000, hit (36, 0, 1.f));
        CHECK (std::abs (peak (soft, 0) - 0.2f) < 0.03f && std::abs (peak (hard, 0) - 0.6f) < 0.05f, "velocity picks the layer (soft %.3f, hard %.3f)", peak (soft, 0), peak (hard, 0));
        auto st = p.padToVT (0);
        auto qq = makeProc(); qq->padFromVT (0, st);
        CHECK (qq->pads[0].layerMode == LM_Velocity && qq->pads[0].layers[1].loVel == 64, "velocity ranges saved with the kit");
    }
    {
        auto pp = makeProc(); auto& p = *pp;
        CHECK (std::abs (TempoSync::divisionSeconds (1, 120.0) - 0.25) < 1e-9 && std::abs (TempoSync::divisionSeconds (2, 100.0) - 0.45) < 1e-9, "tempo divisions (1/8 @120 = 250 ms)");
        p.loadLayer (0, 0, writeWav ("click.wav", 48000.0, 1, 64, [] (int, int i) { return i < 8 ? 0.9f : 0.f; }));
        p.setPadParam (0, PP_SendDly, 100.f);
        p.dlySync = 1; p.dlyFb = 0.f;   // 1/8 at the 120 BPM fallback (no playhead) = 12000 samples
        auto b = render (p, 20000, hit (36));
        int echo = -1;
        for (int i = 6000; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > 0.05f) { echo = i - p.getLatencySamples(); break; }
        CHECK (std::abs (echo - 12000) < 40, "synced delay return lands on the 1/8 (%d)", echo);
        auto st = p.fullStateVT();
        CHECK ((int) st.getProperty ("dlySync") == 1, "delay sync saved");
    }
    {
        auto pp = makeProc(); auto& p = *pp;
        p.undoStack.clear();
        juce::AudioProcessorParameter* prm = p.padParam (3, PP_Cutoff);
        float before = prm->getValue();
        prm->beginChangeGesture(); prm->setValueNotifyingHost (0.2f); prm->setValueNotifyingHost (0.3f); prm->endChangeGesture();
        CHECK (p.undoStack.size() == 1, "one knob gesture = one undo step (%d)", (int) p.undoStack.size());
        p.undo();
        CHECK (std::abs (prm->getValue() - before) < 1e-5f && p.undoStack.empty(), "UNDO puts the knob back");
        p.redo();
        CHECK (std::abs (prm->getValue() - 0.3f) < 1e-5f, "REDO turns it again");
        prm->beginChangeGesture(); prm->endChangeGesture();
        CHECK (p.undoStack.size() == 1, "a click without a change records nothing");
        // a failed user-kit load keeps the history
        p.redoStack.push_back (p.fullStateVT());
        auto bad = tmpRoot.getChildFile ("bad.xml"); bad.replaceWithText ("<nope/>");
        CHECK (! loadUserPreset (bad, p) && p.redoStack.size() == 1 && p.undoStack.size() == 1, "a failed kit load leaves undo / redo alone");
        // FX order: packed, atomic, survives save/load
        auto& ch = p.pads[0].fx;
        ch.order.store ({ 3, 1, 2, 0, 4, 5, 6, 7, 8, 9 });
        FxChainState back; back.fromVT (ch.toVT());
        CHECK (back.order[0] == 3 && back.order[3] == 0 && back.order[9] == 9, "chain order round-trips");
        // background kit load
        p.loadLayer (5, 0, loud);
        auto kitFile = tmpRoot.getChildFile ("Async Kit.xml");
        savePresetFile (p, kitFile);
        p.initKitNoUndo();
        CHECK (p.loadUserKitAsync (kitFile) && p.isLoadingKit(), "kit load starts in the background");
        for (int t = 0; t < 250 && p.isLoadingKit(); ++t) flush();
        CHECK (! p.isLoadingKit() && p.pads[5].layers[0].hasSample() && p.kitName == "Async Kit", "background kit load lands on the message thread");
    }
    // ---- 13. UI audition + editor smoke ----
    {
        auto pp = makeProc(); auto& p = *pp;
        p.loadLayer (4, 0, loud);
        p.auditionPad (4, 1.f);
        CHECK (peak (render (p, 6000), 0) > 0.4f, "pad click auditions through the audio thread");
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* se = dynamic_cast<SamplerEditor*> (ed.get());
        CHECK (se != nullptr, "editor created");
        if (se != nullptr) {
            for (int pg = 0; pg < 5; ++pg) { se->setPageForTest (pg); flush(); }
            for (int v = 0; v < 3; ++v) { se->setViewForTest (v); flush(); }
            p.setLayerMode (4, LM_Velocity); se->setPageForTest (0); flush(); se->setPageForTest (4);
            for (int pad = 0; pad < NumPads; ++pad) se->selectPad (pad);
            se->selectPad (4); se->selectLayer (2);
            se->timerCallback();
            se->setSize (1100, 700); se->setSize (1600, 1000);
            CHECK (se->pageForTest() == 4 && p.selectedPad.load() == 4 && p.selectedLayer.load() == 2, "editor pages / views / pads / layers / resize");
            // ---- in-plugin sample browser ----
            auto* br = se->browserForTest();
            auto dir = tmpRoot.getChildFile ("src");
            dir.getChildFile ("Sub Folder").createDirectory();
            dir.getChildFile ("notes.txt").replaceWithText ("x");
            se->setViewForTest (3);
            br->lib.setCurrentDir (dir.getFullPathName());
            br->refreshList();
            auto& ent = br->model.entries;
            bool dirFirst = ! ent.empty() && ent[0].isDir && ent[0].name == "Sub Folder";
            bool onlyAudio = true;
            for (auto& en : ent) if (! en.isDir && ! SamplerProcessor::isAudioFile (juce::File (en.path))) onlyAudio = false;
            CHECK (dirFirst && onlyAudio && ent.size() > 3, "browser lists folders first, then audio files only (%d entries)", (int) ent.size());
            br->search.setText ("loud", true);
            flush();   // TextEditor change notifications are asynchronous
            CHECK (br->model.entries.size() == 1 && br->model.entries[0].name == "loud.wav", "browser search filters the folder");
            juce::SparseSet<int> one; one.addRange ({ 0, 1 });
            CHECK (br->model.getDragSourceDescription (one).toString() == "file:" + loud.getFullPathName(), "browser rows drag as files");
            // preview plays through Main without touching any pad
            render (p, 48000);
            p.previewFile (loud);
            auto pv = render (p, 8000);
            p.stopPreview();
            auto pv2 = render (p, 8000);
            CHECK (peak (pv, 0) > 0.2f && peak (pv2, 0, 100) < 1.0e-6f, "click-to-preview plays, and stops (%.3f)", peak (pv, 0));
            br->search.setText ("", true);
            flush();
            p.selectedPad = 6; p.selectedLayer = 2;
            for (int r = 0; r < (int) br->model.entries.size(); ++r)
                if (br->model.entries[(size_t) r].name == "quiet.wav") br->list.selectRow (r);
            br->loadSelected();
            CHECK (p.pads[6].layers[2].path == quiet.getFullPathName(), "LOAD / double-click loads into the selected pad + layer");
            se->loadFileFromBrowser (longF, 11, -1);
            CHECK (p.pads[11].layers[0].path == longF.getFullPathName() && p.numLoadedLayers (11) == 1 && p.selectedPad == 11,
                   "dropping a browser file on a pad replaces that pad");
            render (p, 48000 * 2);   // the pad audition from the drop above rings out first
            se->setViewForTest (3);
            p.previewFile (longF);
            render (p, 2000);
            se->setViewForTest (0);
            auto after = render (p, 8000);
            CHECK (peak (after, 0, 200) < 1.0e-6f, "leaving the browser tab stops the preview");
        }
    }

    // ---- 14. shipped factory kits (repo Factory/ folder) ----
    {
        juce::File fac;
        for (auto c : { "Factory", "../Factory", "../../Factory" })
            if (juce::File::getCurrentWorkingDirectory().getChildFile (c).isDirectory()) { fac = juce::File::getCurrentWorkingDirectory().getChildFile (c); break; }
        setFactoryDirsForTest (fac, tmpRoot.getChildFile ("NoFactoryOverride"));
        rescanFactoryPresets();
        CHECK (getFactoryPresets().size() == 20, "20 factory kits found (%d)", (int) getFactoryPresets().size());
        auto pp = makeProc(); auto& p = *pp;
        for (auto& fk : getFactoryPresets()) {
            p.loadKit (fk.index);
            p.limiterOn = false;
            int loaded = 0, sounding = 0, layers = 0, rr = 0;
            for (int pad = 0; pad < NumPads; ++pad) {
                loaded += p.pads[pad].layers[0].hasSample() ? 1 : 0;
                layers += p.numLoadedLayers (pad);
                rr += p.numLoadedLayers (pad) > 1 && p.pads[pad].layerMode == LM_RoundRobin ? 1 : 0;
                render (p, 24000);
                auto b = render (p, 48000 * 3, hit (36 + pad)); // risers swell for seconds
                bool ok = peak (b, 0) > 0.01f && allFinite (b);
                if (! ok) printf ("      silent: pad %d %s (peak %.4f)\n", pad + 1, p.padDisplayName (pad).toRawUTF8(), peak (b, 0));
                sounding += ok ? 1 : 0;
                for (auto& v : p.pads[pad].layers) if (v.missing) ++loaded; // count missing as failures below
            }
            bool hats = p.pads[3].choke == 1 && p.pads[6].choke == 1;
            CHECK (loaded == NumPads && sounding == NumPads && layers > NumPads && rr > 0 && hats,
                   "%s%s: %d/16 pads resolve, %d/16 sound, %d layers, %d RR pads, hats choke",
                   fk.name.toRawUTF8(), fk.genre.isEmpty() ? "" : (" [" + fk.genre + "]").toRawUTF8(), loaded, sounding, layers, rr);
            render (p, 48000 * 4);
        }
        bool noAbs = true;
        for (auto& fk : getFactoryPresets())
            if (fk.file.loadFileAsString().contains ("/home/")) noAbs = false;
        CHECK (noAbs, "factory kit files hold no build-machine paths");
        setFactoryDirsForTest (tmpRoot.getChildFile ("NoFactory"), tmpRoot.getChildFile ("NoFactoryOverride"));
        rescanFactoryPresets();
    }

    tmpRoot.deleteRecursively();
    printf (fails == 0 ? "ALL E2E TESTS PASSED\n" : "%d E2E TEST(S) FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
