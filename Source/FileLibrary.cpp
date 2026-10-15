#include "FileLibrary.h"
#include "Presets.h"

namespace mds {

// JUCE puts Linux settings at ~/<folderName>: keep them under ~/.config like other apps
#if JUCE_LINUX
static juce::String settingsFolder = ".config/Pastel Drum Sampler";
#else
static juce::String settingsFolder = "Pastel Drum Sampler";
#endif
void FileLibrary::setSettingsFolderForTest (const juce::String& folderName) { settingsFolder = folderName; }

FileLibrary::FileLibrary() {
    juce::PropertiesFile::Options opts;
    opts.applicationName = "PastelDrumSampler";
    opts.filenameSuffix = ".settings";
    opts.folderName = settingsFolder;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    props.setStorageParameters (opts);
   #if JUCE_LINUX
    // one-time move from the old location (~/Pastel Drum Sampler)
    auto legacyDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Pastel Drum Sampler");
    auto legacy = legacyDir.getChildFile ("PastelDrumSampler.settings");
    if (settingsFolder.startsWith (".config/") && legacy.existsAsFile()) {
        auto dest = opts.getDefaultFile();
        if (! dest.existsAsFile() && dest.getParentDirectory().createDirectory()) legacy.moveFileTo (dest);
        if (legacyDir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0) legacyDir.deleteFile();
    }
   #endif
    load();
}

juce::File FileLibrary::factorySamplesDir() { return getFactoryDir().getChildFile ("Samples"); }

void FileLibrary::load() {
    shortcuts.clear();
    if (auto* f = props.getUserSettings()) {
        for (int i = 0; i < 64; ++i) {
            auto d = f->getValue ("shortcut" + juce::String (i), "");
            if (d.isEmpty()) break;
            if (juce::File (d).isDirectory() && ! shortcuts.contains (d)) shortcuts.add (d);
        }
        currentDir = f->getValue ("currentDir", "");
    }
    if (! juce::File::isAbsolutePath (currentDir) || ! juce::File (currentDir).isDirectory()) {
        if (factorySamplesDir().isDirectory()) currentDir = factorySamplesDir().getFullPathName();
        else if (shortcuts.size() > 0) currentDir = shortcuts[0];
        else currentDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getFullPathName();
    }
}

void FileLibrary::save() {
    if (auto* f = props.getUserSettings()) {
        for (int i = 0; i < 64; ++i) f->removeValue ("shortcut" + juce::String (i));
        for (int i = 0; i < shortcuts.size(); ++i) f->setValue ("shortcut" + juce::String (i), shortcuts[i]);
        f->setValue ("currentDir", currentDir);
        f->saveIfNeeded();
    }
}

juce::StringArray FileLibrary::getShortcuts() const { return shortcuts; }

void FileLibrary::addShortcut (const juce::String& path) {
    juce::File d (path);
    if (! d.isDirectory()) return;
    auto full = d.getFullPathName();
    if (! shortcuts.contains (full)) {
        shortcuts.add (full);
        save();
        sendChangeMessage();
    }
}

void FileLibrary::removeShortcut (int index) {
    if (index >= 0 && index < shortcuts.size()) {
        shortcuts.remove (index);
        save();
        sendChangeMessage();
    }
}

void FileLibrary::setCurrentDir (const juce::String& path) {
    if (juce::File::isAbsolutePath (path) && juce::File (path).isDirectory() && path != currentDir) {
        currentDir = juce::File (path).getFullPathName();
        save();
        sendChangeMessage();
    }
}

bool FileLibrary::isAudioFile (const juce::File& f) {
    return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

std::vector<FileLibrary::Entry> FileLibrary::listEntries (const juce::String& filter) const {
    std::vector<Entry> out;
    juce::File dir (currentDir);
    if (! dir.isDirectory()) return out;
    auto children = dir.findChildFiles (juce::File::findFilesAndDirectories, false);
    juce::String flt = filter.trim();
    for (auto& c : children) {
        if (c.getFileName().startsWith (".")) continue;
        if (flt.isNotEmpty() && ! c.getFileName().containsIgnoreCase (flt)) continue;
        if (c.isDirectory())
            out.push_back ({ c.getFullPathName(), c.getFileName(), true, {} });
        else if (isAudioFile (c)) {
            auto kind = c.getFileExtension().substring (1).toUpperCase();
            if (kind == "AIFF") kind = "AIF";
            out.push_back ({ c.getFullPathName(), c.getFileName(), false, kind });
        }
    }
    std::sort (out.begin(), out.end(), [] (const Entry& a, const Entry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name.compareNatural (b.name) < 0;
    });
    return out;
}

} // namespace mds
