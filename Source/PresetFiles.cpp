#include "Presets.h"
#include "PluginProcessor.h"
#include <map>

// File read/write operations that need the live processor.

namespace mds {

static juce::String relPath (const juce::File& f, const juce::File& dir) {
    return f.getRelativePathFrom (dir).replaceCharacter ('\\', '/');
}

// Copy every referenced sample into "<stem> Samples/" next to the kit file.
// Same file name from different sources gets a numbered copy; a file that
// is already inside the folder is left where it is.
static void collectSamples (juce::ValueTree& st, const juce::File& kitFile) {
    auto dir = kitFile.getParentDirectory().getChildFile (kitFile.getFileNameWithoutExtension() + " Samples");
    std::map<juce::String, juce::File> done; // source path -> collected copy
    for (auto pad : st) {
        if (! pad.hasType ("PAD")) continue;
        for (auto lv : pad) {
            if (! lv.hasType ("LAYER")) continue;
            auto path = lv.getProperty ("path").toString();
            if (! juce::File::isAbsolutePath (path)) continue;
            juce::File src (path);
            if (! src.existsAsFile()) continue;
            juce::File dest;
            if (src.isAChildOf (dir)) dest = src;
            else if (done.count (path)) dest = done[path];
            else {
                if (! dir.isDirectory() && ! dir.createDirectory()) continue;
                dest = dir.getChildFile (src.getFileName());
                int n = 2;
                while (dest.exists() && ! dest.hasIdenticalContentTo (src))
                    dest = dir.getChildFile (src.getFileNameWithoutExtension() + " (" + juce::String (n++) + ")" + src.getFileExtension());
                if (! dest.exists() && ! src.copyFileTo (dest)) continue;
                done[path] = dest;
            }
            lv.setProperty ("path", dest.getFullPathName(), nullptr);
            lv.setProperty ("rel", relPath (dest, kitFile.getParentDirectory()), nullptr);
        }
    }
}

juce::ValueTree readKitFile (const juce::File& file) { // invalid tree = not a kit
    auto xml = juce::parseXML (file);
    if (xml == nullptr) return {};
    auto vt = juce::ValueTree::fromXml (*xml);
    return vt.isValid() && vt.hasType ("DRUMSAMPLER") ? vt : juce::ValueTree();
}

bool loadPresetFile (SamplerProcessor& proc, const juce::File& file) {
    auto vt = readKitFile (file);
    if (! vt.isValid()) return false;
    proc.loadFullStateVT (vt, file.getParentDirectory(), false);
    return true;
}

bool savePresetFile (SamplerProcessor& proc, const juce::File& file, bool collect) {
    auto st = proc.fullStateVT();
    if (collect) collectSamples (st, file);
    // samples living under the kit's folder are also referenced relatively
    auto kitDir = file.getParentDirectory();
    for (auto pad : st)
        for (auto lv : pad)
            if (lv.hasType ("LAYER") && ! lv.hasProperty ("rel")) {
                auto path = lv.getProperty ("path").toString();
                if (juce::File::isAbsolutePath (path) && juce::File (path).isAChildOf (kitDir))
                    lv.setProperty ("rel", relPath (juce::File (path), kitDir), nullptr);
            }
    if (auto xml = st.createXml())
        if (xml->writeTo (file)) {
            if (collect) proc.loadFullStateVT (st, kitDir, false); // now playing the collected copies
            proc.kitBaseDir = kitDir;
            return true;
        }
    return false;
}

bool applyPreset (SamplerProcessor& proc, int index) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) {
            if (! loadPresetFile (proc, pr.file)) return false;
            proc.currentKit = index;
            proc.kitName = pr.name;
            return true;
        }
    return false;
}

bool overwriteFactoryPreset (int index, SamplerProcessor& proc) {
    const FactoryPreset* row = nullptr;
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) row = &pr;
    if (row == nullptr) return false;
    auto dir = getUserPresetDir().getChildFile ("Factory");
    if (! dir.isDirectory() && ! dir.createDirectory()) return false;
    auto file = dir.getChildFile (row->file.getFileName());
    if (file.exists() && ! file.deleteFile()) return false;
    proc.kitName = row->name; // keep the slotted name/genre on overwrite
    bool ok = savePresetFile (proc, file);
    if (ok) rescanFactoryPresets();
    return ok;
}

bool saveUserPreset (const juce::String& name, SamplerProcessor& proc, bool collect) {
    auto clean = sanitisePresetName (name);
    auto dir = getUserPresetDir();
    if (! dir.isDirectory() && ! dir.createDirectory()) return false;
    auto file = dir.getChildFile (clean + ".xml");
    if (file.exists() && ! file.deleteFile()) return false;
    proc.kitName = clean;
    proc.currentKit = -1;
    return savePresetFile (proc, file, collect);
}

bool loadUserPreset (const juce::File& file, SamplerProcessor& proc) {
    auto vt = readKitFile (file);
    if (! vt.isValid()) return false;   // nothing changed: undo + redo history stay as they were
    proc.pushUndo();
    proc.loadFullStateVT (vt, file.getParentDirectory(), false);
    proc.currentKit = -1; // user sound, not a factory slot
    proc.kitName = file.getFileNameWithoutExtension();
    return true;
}

} // namespace mds
