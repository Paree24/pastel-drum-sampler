#include "Presets.h"
#include "PluginProcessor.h"
#include <map>
#include <algorithm>

namespace mds {

// ---------------- file-backed factory + user presets ----------------
// Filenames: "NNN Name [Genre].xml" (factory) or "Name.xml" (user).

static juce::File customFactoryDir, customFactoryOverrideDir, customUserDir;
static bool useCustomFactoryDir = false, useCustomFactoryOverrideDir = false, useCustomUserDir = false;

void setFactoryDirsForTest (const juce::File& bundleDir, const juce::File& overrideDir) {
    customFactoryDir = bundleDir; useCustomFactoryDir = true;
    customFactoryOverrideDir = overrideDir; useCustomFactoryOverrideDir = true;
}
void setUserPresetDirForTest (const juce::File& dir) { customUserDir = dir; useCustomUserDir = true; }

juce::File getUserPresetDir() {
    if (useCustomUserDir) return customUserDir;
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Pastel Drum Sampler Presets");
}

static juce::File factoryBundleDir() {
    if (useCustomFactoryDir) return customFactoryDir;
    // VST3 bundle layout: <bundle>/Contents/<arch>/Pastel Drum Sampler.so -> ../Resources/Factory
    auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto res = exe.getParentDirectory().getParentDirectory().getChildFile ("Resources/Factory");
    if (res.isDirectory()) return res;
    // standalone: Factory next to the executable
    auto sib = exe.getParentDirectory().getChildFile ("Factory");
    if (sib.isDirectory()) return sib;
    // installed shared data
    auto share = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile (".local/share/PastelDrumSampler/Factory");
    if (share.isDirectory()) return share;
    // dev fallbacks (repo / build tree)
    const char* cands[] = { "./Factory", "../Factory", "../../Factory",
                            "../../../Factory", "../../../../Factory" };
    for (auto c : cands) {
        juce::File d (c);
        if (d.isDirectory() && d.findChildFiles (juce::File::findFiles, false, "*.xml").size() > 0)
            return d;
    }
    return juce::File();
}

juce::File getFactoryDir() { return factoryBundleDir(); }

static juce::File factoryOverrideDir() {
    if (useCustomFactoryOverrideDir) return customFactoryOverrideDir;
    return getUserPresetDir().getChildFile ("Factory");
}

// "NNN Name [Genre].xml" (genre tag optional, any style name).
static bool parseFactoryFileName (const juce::String& fn, int& indexOut, juce::String& nameOut,
                                  juce::String& genreOut) {
    if (! fn.endsWithIgnoreCase (".xml") || fn.length() < 8) return false;
    if (! juce::CharacterFunctions::isDigit (fn[0]) || ! juce::CharacterFunctions::isDigit (fn[1])
        || ! juce::CharacterFunctions::isDigit (fn[2]) || fn[3] != ' ') return false;
    indexOut = fn.substring (0, 3).getIntValue();
    juce::String rest = fn.substring (4, fn.length() - 4);
    genreOut = {};
    int ob = rest.lastIndexOfChar ('[');
    if (ob > 0 && rest.endsWithChar (']')) {
        genreOut = rest.substring (ob + 1, rest.length() - 1).trim();
        rest = rest.substring (0, ob).trim();
    }
    nameOut = rest;
    return indexOut >= 0 && indexOut < 1000 && nameOut.isNotEmpty();
}

static std::vector<FactoryPreset> factory;
static bool factoryScanned = false;

void rescanFactoryPresets() {
    factory.clear();
    std::map<int, FactoryPreset> byIndex;
    auto collect = [&] (const juce::File& dir, bool shadowed) {
        if (! dir.isDirectory()) return;
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml")) {
            int idx; juce::String nm, genre;
            if (! parseFactoryFileName (f.getFileName(), idx, nm, genre)) continue;
            byIndex[idx] = { nm, genre, f, idx, shadowed };
        }
    };
    collect (factoryBundleDir(), false);
    collect (factoryOverrideDir(), true); // user overwrites shadow shipped files
    for (auto& kv : byIndex) factory.push_back (kv.second);
    factoryScanned = true;
}

const std::vector<FactoryPreset>& getFactoryPresets() {
    if (! factoryScanned) rescanFactoryPresets();
    return factory;
}

juce::String presetGenreOf (int index) {
    if (index < 0) return "User";
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) return pr.genre;
    return "Init";
}

bool deleteFactoryShadow (int index) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == index && pr.shadowed) {
            bool ok = pr.file.deleteFile();
            rescanFactoryPresets();
            return ok;
        }
    return false;
}

std::vector<UserPreset> scanUserPresets() {
    std::vector<UserPreset> out;
    auto dir = getUserPresetDir();
    if (! dir.isDirectory()) return out;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
        out.push_back ({ f.getFileNameWithoutExtension(), f });
    std::sort (out.begin(), out.end(),
               [] (const UserPreset& a, const UserPreset& b)
               { return a.name.compareIgnoreCase (b.name) < 0; });
    return out;
}

juce::String sanitisePresetName (juce::String name) {
    name = name.trim().substring (0, 64);
    name = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-");
    name = name.trim();
    return name.isEmpty() ? juce::String ("Untitled") : name;
}

juce::String presetName (int index) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) return pr.name;
    return "Init";
}

bool deleteUserPreset (const juce::File& file) {
    if (file.getParentDirectory() != getUserPresetDir()) return false; // safety: own dir only
    return file.existsAsFile() && file.deleteFile();
}

} // namespace mds
