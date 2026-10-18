#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FileLibrary.h"
#include "Visuals.h"

namespace mds {

class PresetBrowser;
class SamplerEditor;

struct PastelLNF : juce::LookAndFeel_V4 {
    PastelLNF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Colour accent = juce::Colour (0xfff49ac1);
};

// slider that hands right-clicks to a callback (lock / macro menu) instead of dragging
struct LockSlider : juce::Slider {
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; } juce::Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e); }
};

void drawPadlock (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c);

// compact labelled knob (right-click = lock / macro menu when wired)
struct Knob : juce::Component {
    Knob (const juce::String& label, juce::Colour ac);
    void attach (juce::AudioProcessorValueTreeState& ap, const juce::String& id);
    void setManual (float v, std::function<void(float)> cb, std::function<juce::String(float)> fmt = {});
    void setValue01 (float v01, bool notify = true);
    void setBipolar (bool b) { s.getProperties().set ("bipolar", b); repaint(); }
    void resized() override;
    LockSlider s; juce::Label lab, val;
    bool locked = false;  // padlock
    int macroCount = 0;   // macros driving this knob (small dot badge)
    void paintOverChildren (juce::Graphics&) override;
    juce::Colour accent;
    std::function<void(float)> onChange;
    std::function<juce::String(float)> fmtr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// one pad: number, name, waveform, layer dots, meter, S / M
struct PadView : juce::Component, juce::FileDragAndDropTarget, juce::DragAndDropTarget {
    PadView (int idx, SamplerProcessor& pr, SamplerEditor& ed);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    void tick();   // timer: hit flash + meter
    int idx; SamplerProcessor& proc; SamplerEditor& ed;
    juce::TextButton solo { "S" }, mute { "M" };
    bool dragOver = false;
    float flash = 0.f; int lastHits = 0; float lastMeter = 0.f;
};

// layer slot in the layer strip (click = select, drop = load into it)
struct LayerCell : juce::Component, juce::FileDragAndDropTarget, juce::DragAndDropTarget {
    LayerCell (int layer, SamplerEditor& ed) : li (layer), ed (ed) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    int li; SamplerEditor& ed; bool dragOver = false;
};

// waveform of the selected layer: draggable START / END, playhead, file drop
struct WaveView : juce::Component, juce::FileDragAndDropTarget, juce::DragAndDropTarget {
    explicit WaveView (SamplerEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = 0; }
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    juce::Rectangle<float> plotArea() const;
    SamplerEditor& ed; int dragging = 0; bool dragOver = false;
};

// the four macros of the selected pad
struct MacroPanel : juce::Component {
    explicit MacroPanel (SamplerEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
    void showTargetMenu (int k);
    SamplerEditor& ed;
    std::unique_ptr<Knob> knobs[NumMacros];
    juce::Label names[NumMacros];
    juce::Rectangle<int> targetRects[NumMacros];
};

// sample browser (ported from Pastel Soundfont Player): folder shortcuts,
// search, folders first, click = preview, double-click = load, drag to pads
struct SampleListModel : juce::ListBoxModel {
    std::vector<FileLibrary::Entry> entries;
    std::function<void (int)> onSelect, onClick, onDoubleClick;
    juce::Colour accent = juce::Colour (0xfff49ac1);
    int getNumRows() override { return (int) entries.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged (int row) override { if (onSelect) onSelect (row); }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (onClick) onClick (row); }
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick (row); }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override;
};

struct BrowserPanel : juce::Component, juce::ChangeListener {
    explicit BrowserPanel (SamplerEditor& e);
    ~BrowserPanel() override { lib.removeChangeListener (this); }
    void resized() override;
    void paint (juce::Graphics&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshShortcuts(); refreshList(); }
    void refreshShortcuts();
    void refreshList();
    void refreshHint();
    void loadSelected();
    void preview (int row);
    SamplerEditor& ed;
    FileLibrary lib;
    juce::TextEditor search;
    juce::ComboBox dirBox;
    juce::TextButton addDirB { "+ ADD" }, delDirB { "DEL" }, upB { "UP" }, scanB { "SCAN" }, loadB { "LOAD >" }, auditionB { "PREVIEW" };
    juce::Label pathLab, hint;
    juce::ListBox list;
    SampleListModel model;
    int lastPreviewRow = -1;
};

class SamplerEditor : public juce::AudioProcessorEditor, public juce::Timer, public juce::DragAndDropContainer {
public:
    // fx chain view: chip strip (slots) + roomy tabbed detail editor
    struct FxChainView : juce::Component {
        FxChainView (SamplerEditor& ed, bool global);
        void refresh(); // rebind after pad/kit/type/selection change
        void resized() override;
        float getParam (int slot, int k) const;
        void setParam (int slot, int k, float v);
        void selectSlot (int slot);
        void moveSlot (int slot, int dir); // reorder active chip (< > buttons)
        FxChainState& chain();
        int selected = 0;
        struct Chip : juce::TextButton {
            Chip (FxChainView& v, int slot);
            FxChainView& view; int slot;
        };
        SamplerEditor& ed; bool isGlobal;
        juce::ComboBox typeBox;
        juce::TextButton bypassB { "ON" }, addB { "+ ADD" }, delB { "DEL" }, upB { "<" }, dnB { ">" };
        juce::Label emptyLab { {}, "EMPTY - click + ADD to insert an effect" };
        std::vector<std::unique_ptr<Knob>> knobs;
        std::vector<std::unique_ptr<Chip>> chips;
    };

    SamplerEditor (SamplerProcessor&);
    ~SamplerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    SamplerProcessor& proc;
    PastelLNF lnf;
    juce::TooltipWindow tooltips { this, 700 };   // child of the editor: setTooltip() texts show up in every host
    int sel() const { return proc.selectedPad.load(); }
    int selLayer() const { return proc.selectedLayer.load(); }
    void selectPad (int p);
    void selectLayer (int l);
    void refreshAll();
    void applyPadAccent (int pad);            // after any structural change
    void showParamMenu (int pad, int param, Knob* k);
    void chooseFilesForLayer (int pad, int layer);
    void stepLayerFile (int dir); // < > : neighbouring file in the same folder
    juce::String paramLabel (int param) const { return padParamMeta (param).label; }

    // headless UI-logic test hooks
    FxChainView* partFxViewForTest() { return partFxView.get(); }
    int pageForTest() const { return page; }
    void setPageForTest (int p) { page = p; rebuildPage(); }
    void setViewForTest (int v) { mainView = v; layoutMainView(); }
    BrowserPanel* browserForTest() { return browserPanel.get(); }
    void loadFileFromBrowser (const juce::File& f, int pad, int layer); // layer < 0: replace the pad's layers

private:
    // header (shared with the drum synth)
    juce::TextButton presetBtn { "Init" };
    juce::TextButton prevB { "<" }, nextB { ">" }, randPartB { "RANDOMIZE" }, mutPartB { "MUTATE" },
                     randKitB { "KIT RND" }, undoB { "UNDO" }, redoB { "REDO" }, initB { "INIT" };
    std::unique_ptr<PresetBrowser> browser;
    juce::ComboBox scopeBox; // PAD / KIT for rand/mut
    juce::Slider masterDial; juce::Label masterLab, kitLab, voiceLab;
    juce::Slider masterFader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterDialAtt, masterFaderAtt;
    juce::TextButton limitB { "LIMITER" };
    juce::Label outTitle { {}, "OUTPUT" };
    MeterComp outMeter { [this] { return proc.outLevel.load(); }, false };
    juce::Rectangle<int> footerRect, padArea;

    // pads
    std::vector<std::unique_ptr<PadView>> padViews;

    // right panel
    juce::TextButton viewSample { "SAMPLE" }, viewPartFx { "PART FX" }, viewGlobFx { "GLOBAL FX" }, viewBrowser { "BROWSER" }, trigB { "TRIGGER" };
    std::unique_ptr<BrowserPanel> browserPanel;
    int mainView = 0;
    juce::Rectangle<int> padNumRect;
    juce::Label nameLab;
    juce::Label outLab { {}, "OUT" }, chokeLab { {}, "CHOKE" }, voiceModeLab { {}, "VOICE" };
    juce::ComboBox outBox, chokeBox, voiceBox;

    juce::Component samplePanel, fxPanel, globalPanel;
    juce::TextButton tabs[5] { juce::TextButton ("SAMPLE"), juce::TextButton ("AMP"), juce::TextButton ("PITCH"),
                               juce::TextButton ("FILTER"), juce::TextButton ("FX SEND") };
    int page = 0;
    juce::Label lockHint { {}, "RIGHT-CLICK = LOCK / MACRO" };
    juce::Label fmodeLab { {}, "MODE" };
    juce::ComboBox fmodeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> fmodeAtt;
    juce::TextButton stackB { "STACK" }, rrB { "ROUND ROBIN" }, velB { "VELOCITY" };
    juce::Label layersLab { {}, "LAYERS" };
    std::vector<std::unique_ptr<LayerCell>> layerCells;
    WaveView wave { *this };
    juce::TextButton prevFileB { "<" }, nextFileB { ">" }, loadB { "LOAD..." }, revB { "REVERSE" }, normB { "NORMALIZE" }, clearB { "CLEAR" };
    juce::TextButton scopeB { "SCOPE" };   // waveform <-> scope + spectrum of the selected pad's output
    ScopeSource selectedScope() const;
    ScopeView padScope { [this] { return selectedScope(); } };
    SpectrumView padSpectrum { [this] { return selectedScope(); } };
    std::vector<std::unique_ptr<Knob>> row1, row2;
    juce::Label row1Lab, row2Lab;
    std::unique_ptr<MacroPanel> macroPanel;

    std::unique_ptr<FxChainView> partFxView, globFxView;
    juce::Label fxTitle { {}, "EFFECTS CHAIN" }, fxHint { {}, "USE < > TO REORDER" };
    juce::Label gloTitle { {}, "GLOBAL EFFECTS" };
    juce::TextButton partFxPower { "ON" }, globPower { "ON" }, partFxLock { "LOCK" };

    std::unique_ptr<juce::FileChooser> chooser;
    int lastSel = -1, lastLayer = -1, lastVer = -1, lastKit = -2;
    juce::String lastKitName;

    void rebuildPage();           // knob rows for the current page
    void refreshHeader();         // name, routing combos, layer strip
    void refreshFxRows();
    void layoutMainView();
    std::unique_ptr<Knob> makePadKnob (int p, int param);
    std::unique_ptr<Knob> makeLayerKnob (int p, int layer, int lp);
    std::unique_ptr<Knob> makeAtomicKnob (const juce::String& label, std::atomic<float>& target, std::function<juce::String (float)> fmt, int lockKey);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEditor)
};

} // namespace mds
