#pragma once
#include <JuceHeader.h>
#include <vector>

// ============================================================
// Kits: XML data files (full state: pads, layers, macros, chains).
//   Factory: Factory/NNN Name [Genre].xml shipped in the bundle (samples
//   next to them, kit-relative), per-user overwrites shadow them.
//   User: Documents/Pastel Drum Sampler Presets/Name.xml
// Sample references: absolute path + optional kit-relative path ("rel").
// Saving with COLLECT copies every sample into "<Name> Samples/" next to
// the kit file and stores relative paths, so the kit folder can be moved.
// ============================================================

namespace mds {

class SamplerProcessor;

// ---- factory presets: data files ----
struct FactoryPreset {
    juce::String name, genre;
    juce::File file;
    int index = -1;
    bool shadowed = false; // user-overwritten copy shadows the shipped file
};

const std::vector<FactoryPreset>& getFactoryPresets(); // sorted by index
void rescanFactoryPresets();
void setFactoryDirsForTest (const juce::File& bundleDir, const juce::File& overrideDir);
juce::File getFactoryDir(); // shipped Factory folder (kits + Samples/)
bool applyPreset (SamplerProcessor& proc, int index); // factory by index
juce::String presetName (int index);
juce::String presetGenreOf (int index); // genre tag, or "User"/"Init"
bool overwriteFactoryPreset (int index, SamplerProcessor& proc);
bool deleteFactoryShadow (int index); // restores the shipped file (true if one existed)

// ---- loading / saving kit state files (factory + user share the format) ----
bool loadPresetFile (SamplerProcessor& proc, const juce::File& file); // no undo step (callers push)
// collect: copy samples into "<file stem> Samples/" and reference them relatively
bool savePresetFile (SamplerProcessor& proc, const juce::File& file, bool collect = false);

// ---- user presets (XML files on disk) ----
struct UserPreset { juce::String name; juce::File file; };
void setUserPresetDirForTest (const juce::File& dir); // harness override, else Documents
juce::File getUserPresetDir();
std::vector<UserPreset> scanUserPresets();
juce::String sanitisePresetName (juce::String name);
bool saveUserPreset (const juce::String& name, SamplerProcessor& proc, bool collect = false);
bool loadUserPreset (const juce::File& file, SamplerProcessor& proc);
bool deleteUserPreset (const juce::File& file); // kit file only: collected samples are never deleted

} // namespace mds
