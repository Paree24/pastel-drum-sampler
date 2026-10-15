#pragma once
#include <JuceHeader.h>
#include "Presets.h"

namespace mds {

class SamplerProcessor;
class PastelLNF;

// Preset browser overlay: search + FACTORY/USER filter + list, factory data
// files plus user presets saved on disk. Inline name field + SAVE (no pop-up).
// COLLECT SAMPLES (remembered per session) copies the kit's samples into a
// "<Name> Samples" folder beside it; off = the kit references files in place.
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::TextEditor::Listener {
public:
    explicit PresetBrowser (SamplerProcessor& p, PastelLNF& l);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    std::function<void()> onClose;
    std::function<void()> onPresetChanged; // fired after any load (factory or user)

private:
    struct Row { bool isUser = false; int index = -1; juce::String name, genre; juce::File file; bool shadowed = false; };

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRow) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void textEditorTextChanged (juce::TextEditor&) override;

    void rebuildRows();
    void refreshBankButtons();
    void clickSave();
    void setOverwriteArmed (bool on);
    void saveAs (const juce::String& name);

    SamplerProcessor& proc;
    PastelLNF& lnf;
    juce::TextEditor search, nameBox; // nameBox: inline save name
    bool overwriteArmed = false;
    juce::ListBox list { "presets", this };
    juce::TextButton closeBtn { "X" };
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton overBtn { "OVER" };
    juce::TextButton delBtn { "DEL" };
    juce::ToggleButton collectBtn { "COLLECT SAMPLES" }; // save: copy samples next to the kit
    std::vector<std::unique_ptr<juce::TextButton>> bankBtns; // 0 Factory, 1 User
    std::vector<Row> rows;
    int activeBank = 0;
    int currentFactory = -1;
    juce::Rectangle<int> panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace mds
