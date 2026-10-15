// Factory kit builder (dev only, never shipped): turns the kit generator's
// manifest (tools/kitgen/build.py -> kits.json) into sampler kit XMLs, built
// through the real processor so the files always match the plugin's format.
//   FactoryBuild <kits.json> <factoryDir>
// Kits reference their samples relatively ("Samples/<Kit>/..."), so the
// Factory folder works wherever the plugin bundle is installed.
#include "PluginProcessor.h"
#include <cstdio>

using namespace mds;

static int paramIndex (const juce::String& key) {
    for (int i = 0; i < NumPadParams; ++i) if (key == padParamMeta (i).key) return i;
    return -1;
}

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { printf ("usage: FactoryBuild <kits.json> <factoryDir>\n"); return 1; }
    juce::File manifestFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    juce::File factory (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    auto manifest = juce::JSON::parse (manifestFile);
    auto* kits = manifest["kits"].getArray();
    if (kits == nullptr) { printf ("bad manifest %s\n", manifestFile.getFullPathName().toRawUTF8()); return 1; }
    setFactoryDirsForTest (juce::File(), juce::File()); // never load existing factory kits while building
    for (auto old : factory.findChildFiles (juce::File::findFiles, false, "*.xml")) old.deleteFile();
    int fails = 0;
    for (auto& kit : *kits) {
        SamplerProcessor proc;
        proc.prepareToPlay (44100.0, 512);
        proc.initKitNoUndo();
        auto* pads = kit["pads"].getArray();
        for (int p = 0; p < NumPads && pads != nullptr && p < pads->size(); ++p) {
            auto& pad = pads->getReference (p);
            auto* files = pad["files"].getArray();
            for (int li = 0; files != nullptr && li < files->size() && li < NumLayers; ++li) {
                auto f = factory.getChildFile (files->getReference (li).toString());
                if (! proc.loadLayer (p, li, f, false)) { printf ("FAILED to load %s\n", f.getFullPathName().toRawUTF8()); ++fails; }
            }
            auto& pe = proc.pads[p];
            pe.name = pad["name"].toString();
            pe.layerMode = pad["mode"].toString() == "stack" ? LM_Stack : LM_RoundRobin;
            pe.choke = (int) pad["choke"];
            pe.voiceMode = (int) pad["voice"];
            if (auto* obj = pad["params"].getDynamicObject())
                for (auto& prop : obj->getProperties()) {
                    int idx = paramIndex (prop.name.toString());
                    if (idx < 0) { printf ("unknown param %s\n", prop.name.toString().toRawUTF8()); ++fails; continue; }
                    proc.setPadParam (p, idx, (float) prop.value);
                }
        }
        proc.kitName = kit["name"].toString();
        int index = (int) kit["index"];
        auto genre = kit["genre"].toString();   // optional tag, shown in the browser; factory kits use none
        auto kitFile = factory.getChildFile (juce::String (index).paddedLeft ('0', 3) + " " + proc.kitName
                                             + (genre.isNotEmpty() ? " [" + genre + "]" : juce::String()) + ".xml");
        if (! savePresetFile (proc, kitFile, false)) { printf ("save failed\n"); return 1; }
        // shipped files carry kit-relative paths only (no build-machine paths)
        auto xml = juce::parseXML (kitFile);
        for (auto* pad : xml->getChildWithTagNameIterator ("PAD"))
            for (auto* lv : pad->getChildWithTagNameIterator ("LAYER"))
                if (lv->hasAttribute ("rel")) lv->setAttribute ("path", "");
        xml->setAttribute ("kit", index);
        xml->writeTo (kitFile);
        printf ("wrote %s\n", kitFile.getFileName().toRawUTF8());
    }
    return fails == 0 ? 0 : 1;
}
