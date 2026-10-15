#pragma once
#include <JuceHeader.h>

// ============================================================
// FileLibrary (from Pastel Soundfont Player): folder shortcuts
// (persisted in the user settings) + a simple file browser over the
// selected folder. No background scanning: the browser lists the
// current folder on demand. Here it lists audio files instead of
// soundfonts, and always offers the factory sample folder first.
// ============================================================
namespace mds {

class FileLibrary : public juce::ChangeBroadcaster {
public:
    struct Entry {
        juce::String path;
        juce::String name;
        bool isDir = false;
        juce::String kind;   // "WAV", "FLAC", ... (files only)
    };

    FileLibrary();
    ~FileLibrary() override = default;

    // shortcuts (user folders; the factory folder is listed separately)
    juce::StringArray getShortcuts() const;
    void addShortcut (const juce::String& path);
    void removeShortcut (int index);
    static juce::File factorySamplesDir();

    // browser state
    juce::String getCurrentDir() const { return currentDir; }
    void setCurrentDir (const juce::String& path);
    std::vector<Entry> listEntries (const juce::String& filter = "") const;
    static bool isAudioFile (const juce::File& f);

    static void setSettingsFolderForTest (const juce::String& folderName);

private:
    void load();
    void save();

    juce::ApplicationProperties props;
    juce::StringArray shortcuts;
    juce::String currentDir;
};

} // namespace mds
