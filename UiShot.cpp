// UI snapshot tool (dev only, never shipped): opens the real editor
// offscreen, loads a few generated samples, clicks through pages/views and
// writes PNGs for inspection.   UiShot <outDir> [kitFile.xml]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FileLibrary.h"
#include <cstdio>

using namespace mds;

static void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (60); }

static void shot (juce::Component& c, const juce::File& f) {
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    printf ("wrote %s (%dx%d)\n", f.getFileName().toRawUTF8(), img.getWidth(), img.getHeight());
}

static juce::File makeWav (const juce::File& dir, const juce::String& name, double secs, float decay, float hz, float noise) {
    auto f = dir.getChildFile (name + ".wav");
    const double sr = 44100.0;
    int n = (int) (secs * sr);
    juce::AudioBuffer<float> b (2, n);
    juce::Random rng (name.hashCode());
    for (int i = 0; i < n; ++i) {
        float t = (float) (i / sr), env = std::exp (-t * decay);
        float v = env * (0.7f * std::sin (juce::MathConstants<float>::twoPi * hz * t * (1.f + 2.f * std::exp (-t * 40.f)))
                         + noise * (rng.nextFloat() * 2.f - 1.f));
        b.setSample (0, i, v); b.setSample (1, i, v * 0.9f);
    }
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
    return f;
}

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    FileLibrary::setSettingsFolderForTest (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PastelSamplerUiShotSettings").getFullPathName()); // never touch the user's shortcuts
    juce::File out = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("uishot");
    out.createDirectory();
    SamplerProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    if (argc > 2) loadPresetFile (proc, juce::File (argv[2]));
    else {
        auto dir = out.getChildFile ("samples"); dir.createDirectory();
        struct S { const char* n; double s; float d, hz, nz; } defs[] = {
            { "Kick_808_03", 0.6, 6.f, 50.f, 0.02f }, { "Snare_Tight_04", 0.4, 12.f, 190.f, 0.6f }, { "Clap_Stack", 0.4, 14.f, 900.f, 0.8f },
            { "Rim_Dry", 0.1, 40.f, 1200.f, 0.2f }, { "Hat_Closed_02", 0.12, 45.f, 6000.f, 0.9f }, { "Hat_Pedal", 0.15, 35.f, 5000.f, 0.9f },
            { "Hat_Open_Long", 0.8, 4.f, 7000.f, 0.9f }, { "Ride_Bell", 1.2, 2.5f, 3100.f, 0.5f }, { "Crash_Wash", 1.6, 2.f, 4000.f, 0.95f },
            { "Tom_Hi", 0.4, 8.f, 220.f, 0.1f }, { "Tom_Mid", 0.45, 7.f, 160.f, 0.1f }, { "Tom_Floor", 0.6, 6.f, 110.f, 0.1f },
            { "Conga_Slap", 0.3, 10.f, 330.f, 0.3f }, { nullptr, 0, 0, 0, 0 }, { "Vox_Chop", 0.5, 3.f, 440.f, 0.2f }, { "Riser_FX", 1.0, 1.f, 800.f, 0.6f },
        };
        for (int p = 0; p < NumPads; ++p) if (defs[p].n) proc.loadLayer (p, 0, makeWav (dir, defs[p].n, defs[p].s, defs[p].d, defs[p].hz, defs[p].nz), false);
        proc.loadLayer (1, 1, makeWav (dir, "Snare_Tight_05", 0.4, 11.f, 200.f, 0.6f), false);
        proc.loadLayer (1, 2, makeWav (dir, "Snare_Tight_06", 0.4, 13.f, 185.f, 0.6f), false);
        proc.pads[1].layerMode = LM_RoundRobin;
        proc.loadLayer (2, 1, makeWav (dir, "Clap_Layer_B", 0.4, 14.f, 1100.f, 0.8f), false);
        proc.loadLayer (4, 1, makeWav (dir, "Hat_Closed_03", 0.12, 45.f, 6200.f, 0.9f), false);
        proc.pads[4].layerMode = LM_RoundRobin;
        proc.pads[1].output = 2;
        proc.pads[15].mute = true;
        proc.pads[1].layers[0].v[LP_Start] = 0.03f; proc.pads[1].layers[0].v[LP_End] = 0.8f;
        proc.setLocked (1, PP_Drive, true);
        proc.setPadParam (1, PP_Drive, 25.f); proc.setPadParam (1, PP_Comp, 40.f); proc.setPadParam (1, PP_Punch, 60.f);
        proc.kitName = "Steel Roller";
    }
    proc.selectedPad = 1;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* e = dynamic_cast<SamplerEditor*> (ed.get());
    e->setVisible (true);
    pump();
    // play a few pads so meters / flash / playhead show
    proc.auditionPad (1, 1.f); proc.auditionPad (4, 1.f);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb;
    for (int i = 0; i < 6; ++i) proc.processBlock (buf, mb);
    e->timerCallback();
    pump();
    const char* names[5] = { "sample", "amp", "pitch", "filter", "fxsend" };
    for (int pg = 0; pg < 5; ++pg) { e->setPageForTest (pg); pump(); shot (*e, out.getChildFile (juce::String ("0") + juce::String (pg + 1) + "_" + names[pg] + ".png")); }
    e->setPageForTest (0);
    FxProcessor::defaultSlot (FX_COMP, proc.pads[1].fx.slots[0]);
    FxProcessor::defaultSlot (FX_REVERB, proc.pads[1].fx.slots[1]);
    proc.maintenance();
    e->setViewForTest (1); e->refreshAll(); pump(); shot (*e, out.getChildFile ("06_partfx.png"));
    e->setViewForTest (2); e->refreshAll(); pump(); shot (*e, out.getChildFile ("07_globalfx.png"));
    e->setViewForTest (3); e->refreshAll(); pump();
    if (auto* br = e->browserForTest()) {
        br->lib.setCurrentDir (FileLibrary::factorySamplesDir().getChildFile ("Heavy Water").getFullPathName());
        br->refreshList(); br->list.selectRow (1); pump();
    }
    shot (*e, out.getChildFile ("07b_browser.png"));
    e->setViewForTest (0);
    e->selectPad (13); pump(); shot (*e, out.getChildFile ("08_empty_pad.png"));
    e->setSize (1100, 700); pump(); shot (*e, out.getChildFile ("09_small.png"));
    return 0;
}
