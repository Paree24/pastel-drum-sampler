#include "PluginEditor.h"
#include "PresetBrowser.h"

namespace mds {

static const juce::Colour kBg (0xff101014), kPanel (0xff17171d), kPanelSel (0xff22222c), kRaised (0xff232330),
                          kEdge (0xff2a2a36), kText (0xffe8e8ee), kLab (0xff9a9aa8), kDim (0xff6a6a78),
                          kAccent (0xfff49ac1), kGreen (0xffa3e635), kAmber (0xfffbbf24), kRed (0xfff87171);

// ================= LNF =================
PastelLNF::PastelLNF() {
    setColour (juce::ResizableWindow::backgroundColourId, kBg);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c1c22));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kLab);
    setColour (juce::TextButton::buttonColourId, kRaised);
    setColour (juce::TextButton::textColourOnId, kBg);
    setColour (juce::Label::textColourId, juce::Colour (0xffb8b8c4));
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::trackColourId, juce::Colour (0xff2a2a34));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1c22));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::headerTextColourId, kLab);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, kText);
    setColour (juce::TextEditor::backgroundColourId, kRaised);
    setColour (juce::TextEditor::textColourId, kText);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::Label::textWhenEditingColourId, kText);
    setColour (juce::Label::backgroundWhenEditingColourId, kRaised);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1c1c22));
    setColour (juce::TooltipWindow::textColourId, kText);
}
void PastelLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float start, float end, juce::Slider& sl) {
    auto ac = sl.findColour (juce::Slider::rotarySliderFillColourId);
    auto cx = x + w * 0.5f, cy = y + h * 0.5f - 6.f, r = juce::jmin (w, h) * 0.32f;
    g.setColour (juce::Colour (0xff1e1e26));
    g.fillEllipse (cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2);
    g.setColour (juce::Colour (0xff2c2c38));
    g.drawEllipse (cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2, 1.2f);
    juce::Path track;
    track.addArc (cx - r, cy - r, r * 2, r * 2, start, end, true);
    g.setColour (juce::Colour (0xff33333f));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    float ang = start + (end - start) * pos;
    // bipolar knobs fill from the centre (tune, pan, macros...)
    float from = (bool) sl.getProperties()["bipolar"] ? (start + end) * 0.5f : start;
    if (std::abs (ang - from) > 0.01f) {
        juce::Path fill;
        fill.addArc (cx - r, cy - r, r * 2, r * 2, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (ac);
        g.strokePath (fill, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.drawLine (cx, cy, cx + std::cos (ang - juce::MathConstants<float>::halfPi) * r * 0.75f,
                cy + std::sin (ang - juce::MathConstants<float>::halfPi) * r * 0.75f, 2.f);
}
void PastelLNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float, float, juce::Slider::SliderStyle style, juce::Slider& sl) {
    if (style != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 1, style, sl); return; }
    auto ac = sl.findColour (juce::Slider::rotarySliderFillColourId);
    float cx = x + w * 0.5f;
    g.setColour (juce::Colour (0xff26262f));
    g.fillRoundedRectangle (cx - 3, (float) y + 4, 6, (float) h - 8, 3.f);
    float fh = (h - 12) * pos;
    g.setColour (ac.withAlpha (0.85f));
    g.fillRoundedRectangle (cx - 3, (float) (y + h - 6) - fh, 6, fh, 3.f);
    g.setColour (juce::Colour (0xffd8d8e2));
    g.fillRoundedRectangle (cx - 11, (float) (y + h - 8) - fh, 22, 7, 2.f);
}
void PastelLNF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.f);
    g.setColour (juce::Colour (0xff34343f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.f, 1.f);
    juce::Path arrow;
    float ax = (float) w - 14.f, ay = h * 0.5f - 2.f;
    arrow.startNewSubPath (ax, ay); arrow.lineTo (ax + 4.f, ay + 4.f); arrow.lineTo (ax + 8.f, ay);
    g.setColour (kLab);
    g.strokePath (arrow, juce::PathStrokeType (1.4f));
}
void PastelLNF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hov, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced (1);
    juce::Colour c = bg;
    if (b.getToggleState()) c = b.findColour (juce::Slider::rotarySliderFillColourId);
    else if (down) c = c.brighter (0.15f);
    else if (hov) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, 5.f, 1.f);
}

// ================= Knob =================
Knob::Knob (const juce::String& label, juce::Colour ac) : accent (ac) {
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, ac);
    lab.setText (label, juce::dontSendNotification);
    lab.setFont (juce::Font (11.f, juce::Font::bold));
    lab.setColour (juce::Label::textColourId, kLab);
    lab.setJustificationType (juce::Justification::centred);
    lab.setInterceptsMouseClicks (false, false);
    val.setFont (juce::Font (11.5f));
    val.setColour (juce::Label::textColourId, ac.brighter (0.25f));
    val.setJustificationType (juce::Justification::centred);
    val.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (s); addAndMakeVisible (lab); addAndMakeVisible (val);
    s.onValueChange = [this] {
        float v = (float) s.getValue();
        if (fmtr) val.setText (fmtr (v), juce::dontSendNotification);
        else val.setText (juce::String (v, 1), juce::dontSendNotification);
        if (onChange) onChange (v);
    };
}
void Knob::attach (juce::AudioProcessorValueTreeState& ap, const juce::String& id) {
    if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (ap.getParameter (id))) {
        juce::NormalisableRange<double> r ((double) pf->range.start, (double) pf->range.end,
                                           (double) pf->range.interval, (double) pf->range.skew,
                                           pf->range.symmetricSkew);
        s.setNormalisableRange (r);
        s.setDoubleClickReturnValue (true, (double) pf->convertFrom0to1 (static_cast<juce::AudioProcessorParameter*> (pf)->getDefaultValue()));
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ap, id, s);
        s.onValueChange();
    }
}
void Knob::setManual (float v, std::function<void(float)> cb, std::function<juce::String(float)> fmt) {
    onChange = cb; fmtr = fmt;
    s.setNormalisableRange ({ 0.0, 1.0 });
    s.setValue (v, juce::dontSendNotification);
    s.onValueChange();
}
void Knob::setValue01 (float v01, bool notify) {
    s.setValue (s.getMinimum() + v01 * (s.getMaximum() - s.getMinimum()),
                notify ? juce::sendNotification : juce::dontSendNotification);
}
void drawPadlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    auto body = r.removeFromBottom (r.getHeight() * 0.58f);
    g.setColour (c);
    g.fillRoundedRectangle (body, 1.5f);
    juce::Path shackle;
    float w = body.getWidth() * 0.62f, x = body.getCentreX() - w * 0.5f;
    shackle.addRoundedRectangle (x, r.getY() + 0.5f, w, r.getHeight() + body.getHeight() * 0.5f, w * 0.5f);
    g.strokePath (shackle, juce::PathStrokeType (1.6f));
}
void Knob::paintOverChildren (juce::Graphics& g) {
    // badges sit on the knob ring itself (same geometry as PastelLNF), not the cell corners
    auto sb = s.getBounds().toFloat();
    float cx = sb.getCentreX(), cy = sb.getCentreY() - 6.f, r = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.32f;
    if (locked) drawPadlock (g, { cx + r - 1.f, cy - r - 9.f, 10.f, 12.f }, kAmber);
    for (int i = 0; i < macroCount; ++i) { // one dot per macro driving this knob
        g.setColour (accent);
        g.fillEllipse (cx - r - 6.f - i * 7.f, cy - r - 5.f, 5.f, 5.f);
    }
}
void Knob::resized() {
    auto r = getLocalBounds();
    if (r.getHeight() < 30) { s.setBounds (r); lab.setBounds (0, 0, 0, 0); val.setBounds (0, 0, 0, 0); return; }
    val.setBounds (r.removeFromBottom (14));
    lab.setBounds (r.removeFromBottom (14));
    s.setBounds (r);
}

// ================= FX chain view (shared with the drum synth) =================
static juce::String fxKnobText (int type, int k, float v01) {
    if ((type == FX_DIST && k == 3) || (type == FX_TRANSIENT && k == 4))
        return juce::String ((v01 * 2.f - 1.f) * 12.f, 1) + "dB";
    if (type == FX_EQ && k <= 3) return juce::String (v01 * 30.f - 15.f, 1) + "dB";
    if (type == FX_EQ && k == 4) return juce::String ((v01 * 2.f - 1.f) * 6.f, 1) + "dB";
    if (type == FX_COMP && k == 1) return juce::String (1.f + v01 * 19.f, 1) + ":1";
    if (type == FX_COMP && k == 2) return juce::String ((0.0005f + v01 * v01 * 0.15f) * 1000.f, 1) + "ms";
    if (type == FX_COMP && k == 3) return juce::String ((0.01f + v01 * v01 * 0.6f) * 1000.f, 0) + "ms";
    if (type == FX_COMP && k == 4) return "+" + juce::String (v01 * 6.f, 1) + "dB";
    if (type == FX_REVERB && k == 3) return juce::String (v01 * 200.f, 0) + "ms";
    if (type == FX_REVERB && k == 0) return juce::String (FxProcessor::rt60FromKnob (v01), 2) + "s";
    if (type == FX_DELAY && k == 3) return juce::String ((int) (20.f * std::pow (100.f, v01))) + "Hz";
    if (type == FX_TRANSIENT && k == 2) return juce::String (1.f * std::pow (60.f, v01), 1) + "ms";
    if (type == FX_DIST && k == 2) {
        static const char* algos[8] = { "Soft","Hard","Fuzz","Fold","Asym","Bit","Shape","Rect" };
        return algos[juce::jlimit (0, 7, (int) (v01 * 7.999f))];
    }
    if (type == FX_FILTER && k == 0) return juce::String ((int) (20.f * std::pow (900.f, v01))) + "Hz";
    if (type == FX_FILTER && k == 2) {
        static const char* modes[4] = { "LP","HP","BP","Notch" };
        return modes[juce::jlimit (0, 3, (int) (v01 * 3.999f))];
    }
    if (type == FX_FILTER && k == 3) return v01 < 0.5f ? "12dB" : "24dB";
    if (k == 5) return juce::String ((int) (v01 * 100)) + "%";
    switch (type) {
        case FX_COMP: if (k == 0) return juce::String (-48 + v01 * 48, 1) + "dB"; break;
        case FX_DELAY: if (k == 0) return juce::String ((0.02f + v01 * v01 * 1.2f) * 1000, 0) + "ms"; break;
        default: break;
    }
    return juce::String ((int) (v01 * 100)) + "%";
}

SamplerEditor::FxChainView::FxChainView (SamplerEditor& e, bool g) : ed (e), isGlobal (g) {
    for (int i = 0; i < 8; ++i) if (isUsableFx (i)) typeBox.addItem (fxTypeName (i), i + 1);
    typeBox.onChange = [this] {
        auto& ch = chain();
        if (selected < 0 || ch.slots[(size_t) selected].type.load() == FX_NONE) return;
        ch.slots[(size_t) selected].type = typeBox.getSelectedId() - 1;
        ed.proc.maintenance(); // reverb/delay memory before the audio thread needs it
        refresh();
    };
    bypassB.setClickingTogglesState (true);
    bypassB.setColour (juce::Slider::rotarySliderFillColourId, kGreen);
    bypassB.onClick = [this] {
        auto& ch = chain();
        if (selected < 0) return;
        ch.slots[(size_t) selected].bypass.store (! bypassB.getToggleState());
        refresh();
    };
    addB.onClick = [this] {
        auto& ch = chain();
        int free = -1;
        for (int i = 0; i < FxChainState::NumSlots; ++i)
            if (ch.slots[(size_t) i].type.load() == FX_NONE) { free = i; break; }
        if (free < 0) return; // chain full
        auto* view = this; // (MSVC resolves `this` in a nested capture to the outer lambda)
        juce::PopupMenu menu;
        for (int t = 0; t < 8; ++t) if (isUsableFx (t)) menu.addItem (t + 1, fxTypeName (t));
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (addB.getScreenBounds()),
            [sp = juce::Component::SafePointer<FxChainView> (view), free] (int id) {
                if (id <= 0 || sp == nullptr) return;
                FxProcessor::defaultSlot (id - 1, sp->chain().slots[(size_t) free]);
                sp->ed.proc.maintenance();
                sp->selectSlot (free);
            });
    };
    delB.onClick = [this] {
        auto& ch = chain();
        if (selected < 0) return;
        FxProcessor::defaultSlot (FX_NONE, ch.slots[(size_t) selected]);
        selected = -1;
        refresh();
    };
    upB.onClick = [this] { moveSlot (selected, -1); };
    dnB.onClick = [this] { moveSlot (selected, +1); };
    emptyLab.setJustificationType (juce::Justification::centred);
    emptyLab.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (typeBox); addAndMakeVisible (bypassB);
    addAndMakeVisible (addB); addAndMakeVisible (delB);
    addAndMakeVisible (upB); addAndMakeVisible (dnB);
    addAndMakeVisible (emptyLab);
    for (int i = 0; i < FxProcessor::MaxKnobs; ++i) {
        knobs.push_back (std::make_unique<Knob> ("", kAccent));
        addAndMakeVisible (*knobs.back());
    }
    refresh();
}

SamplerEditor::FxChainView::Chip::Chip (FxChainView& v, int s) : view (v), slot (s) {
    onClick = [this] { view.selectSlot (slot); };
}

FxChainState& SamplerEditor::FxChainView::chain() {
    return isGlobal ? ed.proc.globalFx : ed.proc.pads[ed.sel()].fx;
}
float SamplerEditor::FxChainView::getParam (int slot, int k) const {
    auto& ch = isGlobal ? ed.proc.globalFx : ed.proc.pads[ed.sel()].fx;
    auto& sl = ch.slots[(size_t) slot];
    switch (k) {
        case 0: return sl.p0.load(); case 1: return sl.p1.load(); case 2: return sl.p2.load();
        case 3: return sl.p3.load(); case 4: return sl.p4.load(); default: return sl.p5.load();
    }
}
void SamplerEditor::FxChainView::setParam (int slot, int k, float v) {
    auto& ch = isGlobal ? ed.proc.globalFx : ed.proc.pads[ed.sel()].fx;
    auto& sl = ch.slots[(size_t) slot];
    v = juce::jlimit (0.f, 1.f, v);
    switch (k) {
        case 0: sl.p0 = v; break; case 1: sl.p1 = v; break; case 2: sl.p2 = v; break;
        case 3: sl.p3 = v; break; case 4: sl.p4 = v; break; default: sl.p5 = v; break;
    }
}
void SamplerEditor::FxChainView::selectSlot (int slot) { selected = slot; refresh(); }

static std::vector<int> activeSlotsInOrder (FxChainState& ch) {
    std::vector<int> out;
    bool seen[FxChainState::NumSlots] = {};
    for (int k = 0; k < FxChainState::NumSlots; ++k) {
        int s = ch.order[(size_t) k];
        if (s < 0 || s >= FxChainState::NumSlots || seen[(size_t) s]) continue;
        seen[(size_t) s] = true;
        if (ch.slots[(size_t) s].type.load() != FX_NONE) out.push_back (s);
    }
    return out;
}

void SamplerEditor::FxChainView::moveSlot (int slot, int dir) {
    if (slot < 0) return;
    auto& ch = chain();
    auto act = activeSlotsInOrder (ch);
    int idx = -1;
    for (size_t i = 0; i < act.size(); ++i) if (act[i] == slot) idx = (int) i;
    if (idx < 0) return;
    int nidx = juce::jlimit (0, (int) act.size() - 1, idx + dir);
    if (nidx == idx) return;
    int posA = -1, posB = -1;
    for (int k = 0; k < FxChainState::NumSlots; ++k) {
        if (ch.order[(size_t) k] == act[(size_t) idx]) posA = k;
        if (ch.order[(size_t) k] == act[(size_t) nidx]) posB = k;
    }
    if (posA < 0 || posB < 0) return;
    auto ord = ch.order.load();
    std::swap (ord[(size_t) posA], ord[(size_t) posB]);
    ch.order.store (ord);   // one atomic store: the audio thread never sees a half-done swap
    refresh();
}

void SamplerEditor::FxChainView::refresh() {
    auto& ch = chain();
    if (selected < 0 || selected >= FxChainState::NumSlots || ch.slots[(size_t) selected].type.load() == FX_NONE) {
        auto act = activeSlotsInOrder (ch);
        selected = act.empty() ? -1 : act[0];
    }
    for (auto& c : chips) removeChildComponent (c.get());
    chips.clear();
    auto act = activeSlotsInOrder (ch);
    static const juce::uint32 kcols[8] = { 0xfff49ac1,0xfff87171,0xfffb923c,0xffa3e635,0xff22d3ee,0xffa78bfa,0xff34d399,0xff7dd3fc };
    for (size_t i = 0; i < act.size(); ++i) {
        int s = act[i];
        auto cp = std::make_unique<Chip> (*this, s);
        int t = ch.slots[(size_t) s].type.load();
        cp->setButtonText (juce::String ((int) i + 1) + " " + fxTypeName (t));
        cp->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (kcols[t < 8 ? t : 0]));
        cp->setClickingTogglesState (true);
        cp->setToggleState (s == selected, juce::dontSendNotification);
        addAndMakeVisible (*cp);
        chips.push_back (std::move (cp));
    }
    bool hasSel = selected >= 0 && ch.slots[(size_t) selected].type.load() != FX_NONE;
    int t = hasSel ? ch.slots[(size_t) selected].type.load() : 0;
    int nk = FxProcessor::numKnobs (t);
    typeBox.setVisible (hasSel); bypassB.setVisible (hasSel);
    for (auto& k : knobs) k->setVisible (hasSel);
    emptyLab.setVisible (! hasSel);
    if (hasSel) {
        auto& sl = ch.slots[(size_t) selected];
        typeBox.setSelectedId (t + 1, juce::dontSendNotification);
        bypassB.setToggleState (! sl.bypass.load(), juce::dontSendNotification);
        bypassB.setButtonText (sl.bypass.load() ? "OFF" : "ON");
        auto c = juce::Colour (kcols[t < 8 ? t : 0]);
        for (int k = 0; k < nk; ++k) {
            knobs[(size_t) k]->setVisible (true);
            knobs[(size_t) k]->s.setColour (juce::Slider::rotarySliderFillColourId, c);
            knobs[(size_t) k]->lab.setText (FxProcessor::knobLabel (t, k), juce::dontSendNotification);
            knobs[(size_t) k]->val.setColour (juce::Label::textColourId, c.brighter (0.2f));
            int slot = selected;
            knobs[(size_t) k]->setManual (getParam (slot, k),
                [this, slot, k] (float nv) { setParam (slot, k, nv); },
                [t, k] (float nv) { return fxKnobText (t, k, nv); });
        }
        for (int k = nk; k < (int) knobs.size(); ++k) knobs[(size_t) k]->setVisible (false);
    }
    resized();
    repaint();
}

void SamplerEditor::FxChainView::resized() {
    auto r = getLocalBounds();
    if (r.getWidth() < 60 || r.getHeight() < 60) return;
    int perRow = 5;
    int rows = juce::jmax (1, ((int) chips.size() + perRow - 1) / perRow);
    auto strip = r.removeFromTop (30 * rows);
    int chipW = juce::jmax (40, juce::jmin (100, (strip.getWidth() - 220) / perRow - 4));
    for (size_t i = 0; i < chips.size(); ++i) {
        int rr = (int) i / perRow, cc = (int) i % perRow;
        chips[i]->setBounds (4 + cc * (chipW + 4), strip.getY() + 3 + rr * 30, chipW, 24);
    }
    auto topRow = juce::Rectangle<int> (strip.getX(), strip.getY(), strip.getWidth(), 30);
    dnB.setBounds (topRow.removeFromRight (40).reduced (2));
    upB.setBounds (topRow.removeFromRight (40).reduced (2));
    delB.setBounds (topRow.removeFromRight (56).reduced (2));
    addB.setBounds (topRow.removeFromRight (76).reduced (2));
    auto dt = r;
    auto typeRow = dt.removeFromTop (28);
    typeBox.setBounds (typeRow.removeFromLeft (170).reduced (2));
    bypassB.setBounds (typeRow.removeFromLeft (70).reduced (2));
    emptyLab.setBounds (dt);
    int cols = 3, krows = 2;
    int kcw = dt.getWidth() / cols, kh = dt.getHeight() / krows;
    int vi = 0;
    for (auto& k : knobs) {
        if (! k->isVisible()) continue;
        k->setBounds (dt.getX() + kcw * (vi % cols), dt.getY() + kh * (vi / cols), kcw, kh);
        ++vi;
    }
}

// ================= helpers =================
static bool anyAudio (const juce::StringArray& files) {
    for (auto& f : files) if (SamplerProcessor::isAudioFile (juce::File (f))) return true;
    return false;
}
static juce::Array<juce::File> audioFiles (const juce::StringArray& files) {
    juce::Array<juce::File> out;
    for (auto& f : files) if (SamplerProcessor::isAudioFile (juce::File (f))) out.add (juce::File (f));
    return out;
}
// drags from the in-plugin browser carry "file:<absolute path>"
static juce::File draggedFile (const juce::DragAndDropTarget::SourceDetails& d) {
    auto s = d.description.toString();
    if (! s.startsWith ("file:")) return {};
    auto path = s.substring (5);
    return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
}
static bool isAudioDrag (const juce::DragAndDropTarget::SourceDetails& d) {
    auto f = draggedFile (d);
    return f != juce::File() && SamplerProcessor::isAudioFile (f);
}

static SampleData::Ptr firstSample (const SamplerProcessor::PadEngine& pe) {
    for (auto& L : pe.layers) if (auto d = L.get()) return d;
    return nullptr;
}
static bool anyMissing (const SamplerProcessor::PadEngine& pe) {
    for (auto& L : pe.layers) if (L.missing) return true;
    return false;
}
// mirrored peak shape of an overview, fitted into r
static juce::Path overviewPath (const SampleData& d, juce::Rectangle<float> r, float from = 0.f, float to = 1.f) {
    juce::Path p;
    const int cols = juce::jmax (2, (int) r.getWidth());
    const int n = (int) d.ovMax.size();
    if (n == 0) return p;
    float mid = r.getCentreY(), half = r.getHeight() * 0.5f;
    float scale = d.peak > 1.0e-4f ? 1.f / juce::jmax (0.25f, d.peak) : 1.f; // quiet files still read
    std::vector<float> amp ((size_t) cols);
    for (int x = 0; x < cols; ++x) {
        float f0 = from + (to - from) * (float) x / cols, f1 = from + (to - from) * (float) (x + 1) / cols;
        int b0 = juce::jlimit (0, n - 1, (int) (f0 * n)), b1 = juce::jlimit (b0, n - 1, (int) (f1 * n));
        float a = 0.f;
        for (int b = b0; b <= b1; ++b) a = juce::jmax (a, std::abs (d.ovMax[(size_t) b]), std::abs (d.ovMin[(size_t) b]));
        amp[(size_t) x] = juce::jmin (1.f, a * scale);
    }
    p.startNewSubPath (r.getX(), mid);
    for (int x = 0; x < cols; ++x) p.lineTo (r.getX() + (float) x, mid - amp[(size_t) x] * half);
    for (int x = cols - 1; x >= 0; --x) p.lineTo (r.getX() + (float) x, mid + amp[(size_t) x] * half);
    p.closeSubPath();
    return p;
}
static void drawHatch (juce::Graphics& g, juce::Rectangle<float> r) {
    g.saveState();
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (juce::Colour (0xff2a2a34));
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 7.f)
        g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 2.f);
    g.restoreState();
    juce::Path border; border.addRoundedRectangle (r, 4.f);
    juce::Path dashed;
    const float dash[] = { 3.f, 3.f };
    juce::PathStrokeType (1.f).createDashedStroke (dashed, border, dash, 2);
    g.setColour (juce::Colour (0xff34343f));
    g.fillPath (dashed);
}

// ================= PadView =================
PadView::PadView (int i, SamplerProcessor& pr, SamplerEditor& e) : idx (i), proc (pr), ed (e) {
    for (auto* b : { &solo, &mute }) {
        b->setClickingTogglesState (true);
        b->setColour (juce::Slider::rotarySliderFillColourId, padColour (i));
        b->setColour (juce::TextButton::textColourOffId, kText);
        addAndMakeVisible (*b);
    }
    solo.setTooltip ("Solo"); mute.setTooltip ("Mute");
    solo.onClick = [this] { proc.pads[idx].solo = solo.getToggleState(); };
    mute.onClick = [this] { proc.pads[idx].mute = mute.getToggleState(); };
}

void PadView::paint (juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced (2.f);
    auto col = padColour (idx);
    auto& pe = proc.pads[idx];
    bool isSel = proc.selectedPad.load() == idx;
    if (isSel)
        for (int k = 4; k >= 1; --k) {
            g.setColour (col.withAlpha (0.06f * (5 - k)));
            g.drawRoundedRectangle (r.expanded ((float) k * 0.5f), 8.f + k * 0.5f, 1.5f);
        }
    g.setColour (isSel ? kPanelSel : kPanel);
    g.fillRoundedRectangle (r, 8.f);
    if (isSel) {
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.16f), 0, r.getY(), col.withAlpha (0.03f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.f);
    }
    if (flash > 0.01f) {
        g.setGradientFill (juce::ColourGradient (kAmber.withAlpha (0.32f * flash), 0, r.getY(), kAmber.withAlpha (0.08f * flash), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.f);
    }
    g.setColour (dragOver ? kText : col.withAlpha (isSel ? 0.95f : 0.42f));
    g.drawRoundedRectangle (r, 8.f, dragOver ? 2.f : (isSel ? 1.6f : 1.f));

    float x = r.getX() + 10.f, w = r.getWidth() - 20.f, y = r.getY();
    g.setColour (col);
    g.setFont (juce::Font (16.f, juce::Font::bold));
    g.drawText (juce::String (idx + 1), (int) x, (int) y + 4, 40, 20, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText (noteName (padMidiNote (idx)), (int) (x + w - 40), (int) y + 4, 40, 20, juce::Justification::centredRight);
    auto name = proc.padDisplayName (idx);
    bool missing = anyMissing (pe);
    g.setColour (name.isEmpty() ? kDim : kText);
    g.setFont (juce::Font (11.5f, juce::Font::bold));
    g.drawText (name.isEmpty() ? juce::String ("EMPTY") : name, (int) x, (int) y + 26, (int) w, 15, juce::Justification::centredLeft, true);

    // waveform thumbnail / drop zone
    float bottomBlock = 46.f;
    auto wr = juce::Rectangle<float> (x, y + 46.f, w, juce::jmax (12.f, r.getHeight() - 46.f - bottomBlock - 6.f));
    if (auto d = firstSample (pe)) {
        g.setColour (col.withAlpha (isSel || flash > 0.05f ? 0.78f : 0.48f));
        g.fillPath (overviewPath (*d, wr));
    } else {
        drawHatch (g, wr);
        g.setColour (missing ? kRed : kDim);
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText (missing ? "MISSING FILE" : "DROP SAMPLE", wr, juce::Justification::centred);
    }

    // layer dots + layer mode
    float dy = r.getBottom() - bottomBlock + 4.f;
    int n = 0;
    for (int l = 0; l < NumLayers; ++l) {
        bool has = pe.layers[l].hasSample();
        n += has ? 1 : 0;
        auto dot = juce::Rectangle<float> (x + l * 11.f, dy, 7.f, 7.f);
        if (has) { g.setColour (col.withAlpha (0.9f)); g.fillEllipse (dot); }
        else { g.setColour (pe.layers[l].missing ? kRed : juce::Colour (0xff3a3a46)); g.drawEllipse (dot, 1.f); }
    }
    if (n > 1) {
        g.setColour (col.withAlpha (0.85f));
        g.setFont (juce::Font (8.5f, juce::Font::bold));
        g.drawText (pe.layerMode.load() == LM_RoundRobin ? "RR" : (pe.layerMode.load() == LM_Velocity ? "VEL" : "STACK"), (int) (x + w - 60), (int) dy - 3, 60, 13, juce::Justification::centredRight);
    }
    int o = pe.output.load();
    if (o > 0) {
        g.setColour (kLab);
        g.setFont (juce::Font (8.5f, juce::Font::bold));
        g.drawText (juce::String::charToString (0x2192) + "OUT " + juce::String (o + 1), (int) (x + 46), (int) dy - 3, 60, 13, juce::Justification::centredLeft);
    }
    // meter
    float my = dy + 12.f;
    g.setColour (juce::Colour (0xff2a2a34));
    g.fillRoundedRectangle (x, my, w, 3.f, 1.5f);
    float m = juce::jlimit (0.f, 1.f, lastMeter);
    if (m > 0.002f) {
        g.setColour (col);
        g.fillRoundedRectangle (x, my, w * std::sqrt (m), 3.f, 1.5f);
    }
    g.setColour (col.withAlpha (isSel ? 0.9f : 0.55f));
    g.fillRoundedRectangle (x, r.getBottom() - 5.f, w, 2.f, 1.f);
}

void PadView::resized() {
    auto r = getLocalBounds().reduced (2);
    int y = r.getBottom() - 28;
    solo.setBounds (r.getX() + 10, y, 26, 19);
    mute.setBounds (r.getX() + 40, y, 26, 19);
}

void PadView::mouseDown (const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        juce::PopupMenu m;
        m.addSectionHeader ("PAD " + juce::String (idx + 1));
        m.addItem (1, "Load samples...");
        m.addItem (2, "Copy pad");
        m.addItem (3, "Paste pad", proc.padClipboard.isValid());
        m.addSeparator();
        m.addItem (4, "Clear pad");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
            [sp = juce::Component::SafePointer<PadView> (this)] (int id) {
                if (sp == nullptr) return;
                auto& p = sp->proc; int i = sp->idx; auto& edr = sp->ed;
                if (id == 1) { edr.selectPad (i); edr.chooseFilesForLayer (i, -1); }
                else if (id == 2) p.copyPad (i);
                else if (id == 3) { p.pastePad (i); edr.selectPad (i); }
                else if (id == 4) { p.clearPad (i); edr.selectPad (i); }
            });
        return;
    }
    ed.selectPad (idx);
    proc.auditionPad (idx, 0.85f);
}

bool PadView::isInterestedInFileDrag (const juce::StringArray& f) { return anyAudio (f); }
void PadView::filesDropped (const juce::StringArray& f, int, int) {
    dragOver = false;
    proc.loadPadFiles (idx, audioFiles (f));
    proc.selectedLayer = 0;
    ed.selectPad (idx);
}

bool PadView::isInterestedInDragSource (const SourceDetails& d) { return isAudioDrag (d); }
void PadView::itemDropped (const SourceDetails& d) { dragOver = false; ed.loadFileFromBrowser (draggedFile (d), idx, -1); }

void PadView::tick() {
    auto& pe = proc.pads[idx];
    int h = pe.hits.load();
    bool dirty = false;
    if (h != lastHits) { lastHits = h; flash = 1.f; dirty = true; }
    else if (flash > 0.01f) { flash *= 0.82f; dirty = true; if (flash <= 0.01f) flash = 0.f; }
    float m = pe.meter.load();
    if (std::abs (m - lastMeter) > 0.004f) { lastMeter = m; dirty = true; }
    if (solo.getToggleState() != pe.solo.load()) solo.setToggleState (pe.solo.load(), juce::dontSendNotification);
    if (mute.getToggleState() != pe.mute.load()) mute.setToggleState (pe.mute.load(), juce::dontSendNotification);
    if (dirty) repaint();
}

// ================= LayerCell =================
void LayerCell::paint (juce::Graphics& g) {
    auto& proc = ed.proc;
    int p = ed.sel();
    auto& pe = proc.pads[p];
    auto& L = pe.layers[li];
    auto col = padColour (p);
    bool selL = ed.selLayer() == li, has = L.hasSample();
    auto r = getLocalBounds().toFloat().reduced (1.f);
    g.setColour (selL ? kPanelSel : juce::Colour (0xff18181e));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (dragOver ? kText : (selL ? col : kEdge));
    g.drawRoundedRectangle (r, 4.f, selL || dragOver ? 1.3f : 1.f);
    g.setFont (juce::Font (10.f, juce::Font::bold));
    g.setColour (has ? col : kDim);
    g.drawText ("L" + juce::String (li + 1), r.withTrimmedLeft (7.f), juce::Justification::centredLeft);
    juce::String nm = L.path.isEmpty() ? juce::String ("+ add layer")
                    : (juce::File::isAbsolutePath (L.path) ? juce::File (L.path).getFileNameWithoutExtension() : L.path);
    if (L.missing) nm = "MISSING " + nm;
    g.setColour (L.missing ? kRed : (has ? kText : kDim));
    g.setFont (juce::Font (10.f));
    float velW = 0.f;
    if (has && pe.layerMode.load() == LM_Velocity) {   // velocity range, right-aligned
        velW = 46.f;
        g.setColour (col.withAlpha (0.85f));
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText (juce::String (L.loVel.load()) + "-" + juce::String (L.hiVel.load()), r.withTrimmedRight (5.f), juce::Justification::centredRight);
        g.setColour (L.missing ? kRed : kText);
        g.setFont (juce::Font (10.f));
    }
    g.drawText (nm, r.withTrimmedLeft (25.f).withTrimmedRight (14.f + velW), juce::Justification::centredLeft, true);
    if (has && pe.layerMode.load() == LM_RoundRobin && proc.numLoadedLayers (p) > 1) {
        // next round-robin layer to fire
        int nxt = pe.rrNext.load();
        for (int k = 0; k < NumLayers; ++k) { int c = (nxt + k) % NumLayers; if (pe.layers[c].hasSample()) { nxt = c; break; } }
        if (nxt == li) { g.setColour (kGreen); g.fillEllipse (r.getRight() - 11.f, r.getCentreY() - 3.f, 6.f, 6.f); }
    }
}
void LayerCell::mouseDown (const juce::MouseEvent& e) {
    ed.selectLayer (li);
    if (e.mods.isPopupMenu()) {
        juce::PopupMenu m;
        m.addItem (1, "Load sample...");
        m.addItem (2, "Clear layer", ed.proc.pads[ed.sel()].layers[li].path.isNotEmpty());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (getScreenBounds()),
            [sp = juce::Component::SafePointer<LayerCell> (this)] (int id) {
                if (sp == nullptr) return;
                if (id == 1) sp->ed.chooseFilesForLayer (sp->ed.sel(), sp->li);
                if (id == 2) { sp->ed.proc.clearLayer (sp->ed.sel(), sp->li); sp->ed.refreshAll(); }
            });
    }
}
bool LayerCell::isInterestedInFileDrag (const juce::StringArray& f) { return anyAudio (f); }
void LayerCell::filesDropped (const juce::StringArray& f, int, int) {
    dragOver = false;
    auto files = audioFiles (f);
    if (files.isEmpty()) return;
    ed.proc.loadLayer (ed.sel(), li, files[0]);
    ed.selectLayer (li);
    ed.refreshAll();
}

bool LayerCell::isInterestedInDragSource (const SourceDetails& d) { return isAudioDrag (d); }
void LayerCell::itemDropped (const SourceDetails& d) { dragOver = false; ed.loadFileFromBrowser (draggedFile (d), ed.sel(), li); }

// ================= WaveView =================
juce::Rectangle<float> WaveView::plotArea() const {
    return getLocalBounds().toFloat().reduced (8.f, 0.f).withTrimmedTop (26.f).withTrimmedBottom (10.f);
}

void WaveView::paint (juce::Graphics& g) {
    auto& proc = ed.proc;
    int p = ed.sel(), li = ed.selLayer();
    auto& L = proc.pads[p].layers[li];
    auto col = padColour (p);
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff16161c));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (dragOver ? kText : juce::Colour (0xff24242c));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.f, dragOver ? 2.f : 1.f);
    g.setFont (juce::Font (9.f, juce::Font::bold));
    g.setColour (col);
    g.drawText ("LAYER " + juce::String (li + 1), 10, 5, 60, 14, juce::Justification::centredLeft);
    auto d = L.get();
    auto pa = plotArea();
    if (d != nullptr) {
        juce::String info = juce::String (d->sampleRate / 1000.0, 1) + " kHz";
        if (d->bitDepth > 0) info << juce::String::fromUTF8 (" \xc2\xb7 ") << d->bitDepth << "-bit";
        info << juce::String::fromUTF8 (" \xc2\xb7 ") << (d->channels() > 1 ? "stereo" : "mono")
             << juce::String::fromUTF8 (" \xc2\xb7 ") << juce::String (juce::roundToInt (d->seconds() * 1000.0)) << " ms";
        g.setColour (kDim);
        g.setFont (juce::Font (9.f));
        g.drawText (info, 64, 5, 240, 14, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff2a2a34));
        g.drawHorizontalLine ((int) pa.getCentreY(), pa.getX(), pa.getRight());
        g.setColour (col.withAlpha (0.8f));
        g.fillPath (overviewPath (*d, pa));
        float s = L.v[LP_Start].load(), e = L.v[LP_End].load();
        float sx = pa.getX() + pa.getWidth() * s, ex = pa.getX() + pa.getWidth() * e;
        g.setColour (kBg.withAlpha (0.72f));
        g.fillRect (juce::Rectangle<float> (r.getX() + 1.f, pa.getY() - 2.f, sx - r.getX() - 1.f, pa.getHeight() + 4.f));
        g.fillRect (juce::Rectangle<float> (ex, pa.getY() - 2.f, r.getRight() - 1.f - ex, pa.getHeight() + 4.f));
        for (int k = 0; k < 2; ++k) {
            float mx = k == 0 ? sx : ex;
            g.setColour (kText);
            g.drawLine (mx, pa.getY() - 2.f, mx, pa.getBottom() + 4.f, 1.5f);
            juce::Path tri;
            tri.addTriangle (mx - 6.f, pa.getY() - 2.f, mx + 6.f, pa.getY() - 2.f, mx, pa.getY() + 6.f);
            g.fillPath (tri);
            g.setFont (juce::Font (9.f, juce::Font::bold));
            g.drawText (k == 0 ? "S" : "E", (int) (k == 0 ? mx + 6 : mx - 18), (int) pa.getY() + 4, 12, 12,
                        k == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight);
        }
        if (L.reverse.load()) {
            g.setColour (kAmber);
            g.setFont (juce::Font (8.5f, juce::Font::bold));
            g.drawText ("REVERSED", (int) pa.getRight() - 70, (int) pa.getBottom() - 12, 66, 12, juce::Justification::centredRight);
        }
        // playhead of the newest voice on this layer
        float pp = proc.pads[p].playPos.load();
        if (pp >= 0.f && proc.pads[p].playLayer.load() == li) {
            float px = pa.getX() + pa.getWidth() * pp;
            g.setColour (kGreen.withAlpha (0.9f));
            g.drawLine (px, pa.getY(), px, pa.getBottom() + 2.f, 1.5f);
        }
        g.setColour (kDim);
        for (int i = 0; i <= 8; ++i) {
            float tx = pa.getX() + pa.getWidth() * i / 8.f;
            g.drawLine (tx, r.getBottom() - 6.f, tx, r.getBottom() - 2.f, 1.f);
        }
    } else {
        g.setColour (L.missing ? kRed : kDim);
        g.setFont (juce::Font (12.f, juce::Font::bold));
        g.drawText (L.missing ? "MISSING: " + L.path : juce::String ("DROP A SAMPLE HERE  OR  CLICK LOAD..."),
                    pa.reduced (10.f, 0.f), juce::Justification::centred, true);
    }
}

void WaveView::mouseMove (const juce::MouseEvent& e) {
    auto pa = plotArea();
    auto& L = ed.proc.pads[ed.sel()].layers[ed.selLayer()];
    float sx = pa.getX() + pa.getWidth() * L.v[LP_Start].load(), ex = pa.getX() + pa.getWidth() * L.v[LP_End].load();
    bool near = L.hasSample() && (std::abs (e.position.x - sx) < 7.f || std::abs (e.position.x - ex) < 7.f);
    setMouseCursor (near ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveView::mouseDown (const juce::MouseEvent& e) {
    auto pa = plotArea();
    auto& L = ed.proc.pads[ed.sel()].layers[ed.selLayer()];
    if (! L.hasSample()) { ed.chooseFilesForLayer (ed.sel(), ed.selLayer()); return; }
    float sx = pa.getX() + pa.getWidth() * L.v[LP_Start].load(), ex = pa.getX() + pa.getWidth() * L.v[LP_End].load();
    float ds = std::abs (e.position.x - sx), de = std::abs (e.position.x - ex);
    if (ds < 8.f || de < 8.f) { dragging = ds <= de ? 1 : 2; ed.proc.pushUndo(); }
    else ed.proc.auditionPad (ed.sel());
}

void WaveView::mouseDrag (const juce::MouseEvent& e) {
    if (dragging == 0) return;
    auto pa = plotArea();
    auto& L = ed.proc.pads[ed.sel()].layers[ed.selLayer()];
    float f = juce::jlimit (0.f, 1.f, (e.position.x - pa.getX()) / pa.getWidth());
    if (dragging == 1) L.v[LP_Start] = juce::jmin (f, L.v[LP_End].load() - 0.002f);
    else L.v[LP_End] = juce::jmax (f, L.v[LP_Start].load() + 0.002f);
    ++ed.proc.stateVersion; // knobs follow
    repaint();
}

bool WaveView::isInterestedInFileDrag (const juce::StringArray& f) { return anyAudio (f); }
void WaveView::filesDropped (const juce::StringArray& f, int, int) {
    dragOver = false;
    auto files = audioFiles (f);
    if (files.isEmpty()) return;
    ed.proc.loadLayer (ed.sel(), ed.selLayer(), files[0]);
    ed.refreshAll();
}

bool WaveView::isInterestedInDragSource (const SourceDetails& d) { return isAudioDrag (d); }
void WaveView::itemDropped (const SourceDetails& d) { dragOver = false; ed.loadFileFromBrowser (draggedFile (d), ed.sel(), ed.selLayer()); }

// ================= Sample browser (from Pastel Soundfont Player) =================
void SampleListModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int) entries.size()) return;
    auto& e = entries[(size_t) row];
    if (selected) {
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (2.f, 1.f, (float) w - 4.f, (float) h - 2.f, 4.f);
    }
    auto badge = juce::Rectangle<float> (6.f, h * 0.5f - 8.f, 38.f, 16.f);
    g.setColour (e.isDir ? juce::Colour (0xff3a3a48) : accent.withAlpha (0.85f));
    g.fillRoundedRectangle (badge, 4.f);
    g.setColour (e.isDir ? kText : kBg);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText (e.isDir ? juce::String ("DIR") : e.kind, badge, juce::Justification::centred);
    g.setColour (e.isDir ? (selected ? kText : kLab) : kText);
    g.setFont (juce::Font (12.5f));
    g.drawText (e.name, 52, 0, w - 70, h, juce::Justification::centredLeft, true);
    if (e.isDir) { g.setColour (kDim); g.drawText (">", w - 18, 0, 12, h, juce::Justification::centred); }
}

juce::var SampleListModel::getDragSourceDescription (const juce::SparseSet<int>& rows) {
    if (rows.size() != 1) return {};
    int r = rows[0];
    if (r < 0 || r >= (int) entries.size() || entries[(size_t) r].isDir) return {};
    return "file:" + entries[(size_t) r].path;
}

BrowserPanel::BrowserPanel (SamplerEditor& e) : ed (e) {
    search.setTextToShowWhenEmpty ("Search this folder...", kDim);
    search.setColour (juce::TextEditor::outlineColourId, kEdge);
    search.setColour (juce::TextEditor::focusedOutlineColourId, kText);
    search.onTextChange = [this] { refreshList(); };
    addAndMakeVisible (search);
    dirBox.onChange = [this] {
        int id = dirBox.getSelectedId();
        if (id == 1) lib.setCurrentDir (FileLibrary::factorySamplesDir().getFullPathName());
        else if (id >= 2) lib.setCurrentDir (lib.getShortcuts()[id - 2]);
    };
    addAndMakeVisible (dirBox);
    addDirB.setTooltip ("Add a folder shortcut");
    addDirB.onClick = [this] {
        auto* panel = this; // (MSVC resolves `this` in a nested capture to the outer lambda)
        auto fc = std::make_shared<juce::FileChooser> ("Add folder shortcut", juce::File (lib.getCurrentDir()));
        fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [sp = juce::Component::SafePointer<BrowserPanel> (panel), fc] (const juce::FileChooser&) {
                auto d = fc->getResult();
                if (sp == nullptr || ! d.isDirectory()) return;
                sp->lib.addShortcut (d.getFullPathName());
                sp->lib.setCurrentDir (d.getFullPathName());
            });
    };
    delDirB.setTooltip ("Remove the selected shortcut (the factory folder stays)");
    delDirB.onClick = [this] { if (dirBox.getSelectedId() >= 2) lib.removeShortcut (dirBox.getSelectedId() - 2); };
    upB.setTooltip ("Parent folder");
    upB.onClick = [this] {
        auto parent = juce::File (lib.getCurrentDir()).getParentDirectory();
        if (parent.isDirectory()) lib.setCurrentDir (parent.getFullPathName());
    };
    scanB.setTooltip ("Re-read this folder");
    scanB.onClick = [this] { refreshList(); };
    loadB.onClick = [this] { loadSelected(); };
    auditionB.setClickingTogglesState (true);
    auditionB.setToggleState (true, juce::dontSendNotification);
    auditionB.setTooltip ("Play files as you select them");
    auditionB.setColour (juce::Slider::rotarySliderFillColourId, kGreen);
    auditionB.onClick = [this] { if (! auditionB.getToggleState()) ed.proc.stopPreview(); };
    for (auto* b : { &addDirB, &delDirB, &upB, &scanB, &loadB, &auditionB }) addAndMakeVisible (*b);
    pathLab.setFont (juce::Font (10.5f));
    pathLab.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (pathLab);
    hint.setFont (juce::Font (9.5f, juce::Font::bold));
    hint.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (hint);
    model.onSelect = [this] (int row) { preview (row); };
    model.onClick = [this] (int row) { if (row == lastPreviewRow) preview (row); };   // re-click replays
    model.onDoubleClick = [this] (int) { loadSelected(); };
    list.setModel (&model);
    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setMultipleSelectionEnabled (false);
    addAndMakeVisible (list);
    lib.addChangeListener (this);
    refreshShortcuts();
    refreshList();
}

void BrowserPanel::refreshShortcuts() {
    dirBox.clear (juce::dontSendNotification);
    dirBox.addItem ("Factory Samples", 1);
    auto sc = lib.getShortcuts();
    for (int i = 0; i < sc.size(); ++i)
        dirBox.addItem (juce::File (sc[i]).getFileName() + juce::String::fromUTF8 ("  \xe2\x80\x94  ") + sc[i], i + 2);
    auto cur = lib.getCurrentDir();
    int sel = sc.indexOf (cur);
    if (sel >= 0) dirBox.setSelectedId (sel + 2, juce::dontSendNotification);
    else if (cur == FileLibrary::factorySamplesDir().getFullPathName()) dirBox.setSelectedId (1, juce::dontSendNotification);
    else dirBox.setTextWhenNothingSelected (juce::File (cur).getFileName());
    delDirB.setEnabled (sel >= 0);
    pathLab.setText (cur, juce::dontSendNotification);
}

void BrowserPanel::refreshList() {
    model.entries = lib.listEntries (search.getText());
    model.accent = padColour (ed.sel());
    lastPreviewRow = -1;
    list.deselectAllRows();
    list.updateContent();
    list.repaint();
    refreshHint();
}

void BrowserPanel::refreshHint() {
    hint.setText ("CLICK = PREVIEW  " + juce::String::charToString (0x00b7) + "  DOUBLE-CLICK / LOAD = PAD " + juce::String (ed.sel() + 1)
                  + " LAYER " + juce::String (ed.selLayer() + 1) + "  " + juce::String::charToString (0x00b7) + "  DRAG ONTO ANY PAD OR LAYER",
                  juce::dontSendNotification);
}

void BrowserPanel::preview (int row) {
    lastPreviewRow = row;
    if (row < 0 || row >= (int) model.entries.size() || model.entries[(size_t) row].isDir || ! auditionB.getToggleState()) return;
    ed.proc.previewFile (juce::File (model.entries[(size_t) row].path));
}

void BrowserPanel::loadSelected() {
    int row = list.getSelectedRow();
    if (row < 0 || row >= (int) model.entries.size()) return;
    auto& e = model.entries[(size_t) row];
    if (e.isDir) { lib.setCurrentDir (e.path); return; }
    ed.loadFileFromBrowser (juce::File (e.path), ed.sel(), ed.selLayer());
}

void BrowserPanel::paint (juce::Graphics& g) {
    g.setColour (kPanel);
    g.fillRoundedRectangle (list.getBounds().toFloat().expanded (2.f), 5.f);
}

void BrowserPanel::resized() {
    auto r = getLocalBounds().reduced (4);
    auto row1 = r.removeFromTop (26);
    scanB.setBounds (row1.removeFromRight (52)); row1.removeFromRight (4);
    upB.setBounds (row1.removeFromRight (44)); row1.removeFromRight (4);
    delDirB.setBounds (row1.removeFromRight (46)); row1.removeFromRight (4);
    addDirB.setBounds (row1.removeFromRight (60)); row1.removeFromRight (6);
    dirBox.setBounds (row1);
    r.removeFromTop (6);
    auto row2 = r.removeFromTop (26);
    auditionB.setBounds (row2.removeFromRight (78)); row2.removeFromRight (4);
    loadB.setBounds (row2.removeFromRight (70)); row2.removeFromRight (6);
    search.setBounds (row2);
    r.removeFromTop (4);
    pathLab.setBounds (r.removeFromTop (16));
    hint.setBounds (r.removeFromBottom (18));
    r.removeFromBottom (4);
    list.setBounds (r.reduced (2));
}

// ================= MacroPanel =================
MacroPanel::MacroPanel (SamplerEditor& e) : ed (e) {
    for (int k = 0; k < NumMacros; ++k) {
        knobs[k] = std::make_unique<Knob> ("", kAccent);
        knobs[k]->setBipolar (true);
        knobs[k]->val.setVisible (false); // value is painted next to the name instead
        knobs[k]->lab.setVisible (false);
        knobs[k]->s.setTooltip ("Macro: centre = no change. Right-click a knob anywhere to assign it here.");
        addAndMakeVisible (*knobs[k]);
        auto& nl = names[k];
        nl.setEditable (false, true, false);
        nl.setFont (juce::Font (11.f, juce::Font::bold));
        nl.setColour (juce::Label::textColourId, kText);
        nl.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1c1c22));
        nl.setColour (juce::Label::outlineColourId, kEdge);
        nl.setTooltip ("Double-click to rename");
        nl.onTextChange = [this, k] {
            auto t = names[k].getText().trim().toUpperCase().substring (0, 14);
            ed.proc.pads[ed.sel()].macros[k].name = t.isEmpty() ? "MACRO " + juce::String (k + 1) : t;
            names[k].setText (ed.proc.pads[ed.sel()].macros[k].name, juce::dontSendNotification);
        };
        addAndMakeVisible (nl);
    }
}

void MacroPanel::refresh() {
    int p = ed.sel();
    auto col = padColour (p);
    for (int k = 0; k < NumMacros; ++k) {
        auto& kb = *knobs[k];
        kb.att.reset();
        kb.accent = col;
        kb.s.setColour (juce::Slider::rotarySliderFillColourId, col);
        kb.attach (ed.proc.apvts, padMacroID (p, k));
        kb.s.setDoubleClickReturnValue (true, 0.5);
        kb.s.onValueChange = [this] { repaint(); };
        names[k].setText (ed.proc.pads[p].macros[k].name, juce::dontSendNotification);
    }
    repaint();
}

void MacroPanel::paint (juce::Graphics& g) {
    int p = ed.sel();
    auto col = padColour (p);
    auto r = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (col.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.f);
    g.setColour (kText);
    g.setFont (juce::Font (12.f, juce::Font::bold));
    g.drawText ("MACROS", 12, 8, 70, 16, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("PAD " + juce::String (p + 1), 74, 8, 60, 16, juce::Justification::centredLeft);
    g.drawText ("RIGHT-CLICK ANY KNOB " + juce::String::charToString (0x2192) + " ASSIGN  " + juce::String::charToString (0x00b7) + "  CLICK TARGETS TO EDIT",
                getWidth() - 412, 8, 400, 16, juce::Justification::centredRight);
    float uw = (getWidth() - 24.f) / NumMacros;
    for (int k = 0; k < NumMacros; ++k) {
        float ux = 12.f + uw * k;
        if (k > 0) { g.setColour (juce::Colour (0xff26262f)); g.drawVerticalLine ((int) ux, 34.f, (float) getHeight() - 10.f); }
        float v = (float) knobs[k]->s.getValue();
        int pc = juce::roundToInt ((v - 0.5f) * 200.f);
        auto nb = names[k].getBounds();
        g.setColour (col);
        g.setFont (juce::Font (11.f));
        g.drawText ((pc > 0 ? "+" : "") + juce::String (pc) + "%", nb.getX(), nb.getBottom() + 4, nb.getWidth(), 14, juce::Justification::centredLeft);
        juce::StringArray labels;
        for (auto& t : ed.proc.pads[p].macros[k].t)
            if (t.param.load() >= 0) labels.add (ed.paramLabel (t.param.load()));
        g.setColour (kDim);
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText (juce::String (labels.size()) + (labels.size() == 1 ? " TARGET" : " TARGETS"), nb.getX(), nb.getBottom() + 19, nb.getWidth(), 12, juce::Justification::centredLeft);
        auto tr = targetRects[k];
        g.setColour (juce::Colour (0xff1c1c22));
        g.fillRoundedRectangle (tr.toFloat(), 3.f);
        g.setColour (labels.isEmpty() ? kDim : kLab);
        g.setFont (juce::Font (9.f));
        g.drawText (labels.isEmpty() ? juce::String ("+ add target") : labels.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")),
                    tr.reduced (6, 0), juce::Justification::centred, true);
    }
}

void MacroPanel::resized() {
    float uw = (getWidth() - 24.f) / NumMacros;
    for (int k = 0; k < NumMacros; ++k) {
        int ux = 12 + (int) (uw * k);
        int kh = juce::jmin (84, getHeight() - 56);
        knobs[k]->setBounds (ux + 2, 30, juce::jmin (84, (int) uw / 2), kh + 28); // + hidden label rows
        names[k].setBounds (ux + (int) uw / 2 + 2, 40, (int) uw / 2 - 12, 20);
        targetRects[k] = { ux + 6, getHeight() - 28, (int) uw - 14, 20 };
    }
}

void MacroPanel::mouseDown (const juce::MouseEvent& e) {
    for (int k = 0; k < NumMacros; ++k)
        if (targetRects[k].contains (e.getPosition())) { showTargetMenu (k); return; }
}

void MacroPanel::showTargetMenu (int k) {
    int p = ed.sel();
    auto& m = ed.proc.pads[p].macros[k];
    static const float depths[] = { 1.f, 0.5f, 0.25f, 0.1f, -0.1f, -0.25f, -0.5f, -1.f };
    auto pct = [] (float d) { int v = juce::roundToInt (d * 100.f); return (v > 0 ? "+" : "") + juce::String (v) + "%"; };
    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String (k + 1) + "  " + m.name);
    int n = 0;
    for (auto& t : m.t) {
        int prm = t.param.load();
        if (prm < 0) continue;
        juce::PopupMenu sub;
        for (int di = 0; di < (int) std::size (depths); ++di)
            sub.addItem (1000 + prm * 20 + di, pct (depths[di]), true, std::abs (t.depth.load() - depths[di]) < 1.0e-3f);
        sub.addSeparator();
        sub.addItem (1000 + prm * 20 + 19, "Remove");
        menu.addSubMenu (ed.paramLabel (prm) + "   " + pct (t.depth.load()), sub);
        ++n;
    }
    if (n == 0) menu.addItem (-1, "(no targets yet)", false);
    menu.addSeparator();
    juce::PopupMenu add;
    for (int prm = 0; prm < NumPadParams; ++prm)
        if (ed.proc.macroDepth (p, k, prm) == 0.f) add.addItem (5000 + prm, ed.paramLabel (prm));
    menu.addSubMenu ("Add target", add, n < MaxMacroTargets);
    menu.addItem (2, "Rename...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (targetRects[k])),
        [sp = juce::Component::SafePointer<MacroPanel> (this), p, k] (int id) {
            if (sp == nullptr || id <= 0) return;
            auto& pr = sp->ed.proc;
            if (id == 2) { sp->names[k].showEditor(); return; }
            if (id >= 5000) pr.setMacroTarget (p, k, id - 5000, 0.5f);
            else if (id >= 1000) {
                int prm = (id - 1000) / 20, di = (id - 1000) % 20;
                pr.setMacroTarget (p, k, prm, di == 19 ? 0.f : depths[di]);
            }
            sp->ed.refreshAll();
        });
}

// ================= Editor =================
SamplerEditor::SamplerEditor (SamplerProcessor& p) : juce::AudioProcessorEditor (p), proc (p) {
    setLookAndFeel (&lnf);

    // header
    presetBtn.onClick = [this] { rescanFactoryPresets(); browser->setVisible (true); };
    auto stepProgram = [this] (int dir) {
        auto& all = getFactoryPresets();
        if (all.empty()) return;
        int pos = 0;
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].index == proc.currentKit) { pos = (int) i; break; }
        int npos = (pos + dir + (int) all.size()) % (int) all.size();
        proc.loadKitAsync (all[(size_t) npos].index);
        refreshAll();
    };
    prevB.onClick = [this, stepProgram] { stepProgram (-1); };
    nextB.onClick = [this, stepProgram] { stepProgram (+1); };
    scopeBox.addItem ("PAD", 1); scopeBox.addItem ("KIT", 2);
    scopeBox.setSelectedId (1, juce::dontSendNotification);
    randPartB.onClick = [this] {
        if (scopeBox.getSelectedId() == 1) proc.randomizePad (sel(), 1.f); else proc.randomizeKit (1.f);
        proc.maintenance(); refreshAll();
    };
    mutPartB.onClick = [this] {
        if (scopeBox.getSelectedId() == 1) proc.mutatePad (sel(), 0.3f); else proc.mutateKit (0.2f);
        refreshAll();
    };
    randKitB.onClick = [this] { proc.randomizeKit (0.7f); proc.maintenance(); refreshAll(); };
    undoB.onClick = [this] { proc.undo(); refreshAll(); };
    redoB.onClick = [this] { proc.redo(); refreshAll(); };
    undoB.setTooltip ("Undo the last change: knob turns, kit and sample edits"); redoB.setTooltip ("Redo what UNDO took back");
    initB.onClick = [this] { proc.initKit(); refreshAll(); };
    randPartB.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    randPartB.setTooltip ("Randomize timbre only: tune, pitch env, filter, drive, tone, sends, effect colour.\n"
                          "Never level, pan, envelopes, compression, punch or velocity response.");
    mutPartB.setTooltip ("Nudge the same timbre settings a little");
    mutPartB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa78bfa));
    masterDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterDial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterDial.setColour (juce::Slider::rotarySliderFillColourId, kAmber);
    masterDialAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterDial);
    masterLab.setText ("MASTER", juce::dontSendNotification);
    masterLab.setFont (juce::Font (9.f, juce::Font::bold));
    limitB.setClickingTogglesState (true);
    limitB.setToggleState (proc.limiterOn.load(), juce::dontSendNotification);
    limitB.onClick = [this] { proc.limiterOn = limitB.getToggleState(); };
    kitLab.setFont (juce::Font (12.f, juce::Font::bold));
    kitLab.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a98));
    voiceLab.setFont (juce::Font (9.5f, juce::Font::bold));
    voiceLab.setColour (juce::Label::textColourId, kDim);
    voiceLab.setJustificationType (juce::Justification::centredRight);
    for (auto* c : std::initializer_list<juce::Component*> { &presetBtn, &prevB, &nextB, &scopeBox, &randPartB, &mutPartB, &randKitB,
                                                             &undoB, &redoB, &initB, &masterDial, &masterLab, &kitLab, &voiceLab })
        addAndMakeVisible (*c);

    // pads
    for (int i = 0; i < NumPads; ++i) { padViews.push_back (std::make_unique<PadView> (i, proc, *this)); addAndMakeVisible (*padViews.back()); }

    // right panel: view tabs + pad header
    for (auto* b : { &viewSample, &viewPartFx, &viewGlobFx, &viewBrowser }) {
        b->setClickingTogglesState (true);
        b->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        addAndMakeVisible (*b);
    }
    viewSample.onClick = [this] { mainView = 0; layoutMainView(); };
    viewPartFx.onClick = [this] { mainView = 1; layoutMainView(); };
    viewGlobFx.onClick = [this] { mainView = 2; layoutMainView(); };
    viewBrowser.onClick = [this] { mainView = 3; layoutMainView(); };
    browserPanel = std::make_unique<BrowserPanel> (*this);
    addChildComponent (*browserPanel);
    trigB.onClick = [this] { proc.auditionPad (sel(), 0.9f); };
    addAndMakeVisible (trigB);

    nameLab.setEditable (false, true, false);
    nameLab.setFont (juce::Font (13.f));
    nameLab.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1c1c22));
    nameLab.setColour (juce::Label::outlineColourId, kEdge);
    nameLab.setColour (juce::Label::textColourId, kText);
    nameLab.setTooltip ("Double-click to rename the pad (empty = file name)");
    nameLab.onTextChange = [this] { proc.pads[sel()].name = nameLab.getText().trim(); refreshHeader(); for (auto& pv : padViews) pv->repaint(); };
    addAndMakeVisible (nameLab);
    for (auto* l : { &outLab, &chokeLab, &voiceModeLab }) {
        l->setFont (juce::Font (9.5f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, kLab);
        l->setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (*l);
    }
    outBox.addItem ("Main", 1);
    for (int i = 0; i < NumAuxOuts; ++i) outBox.addItem ("Out " + juce::String (i + 2), i + 2);
    outBox.setTooltip ("Output bus. Aux outs must be enabled in the host; a disabled one falls back to Main.");
    outBox.onChange = [this] { proc.pads[sel()].output = outBox.getSelectedId() - 1; for (auto& pv : padViews) pv->repaint(); };
    chokeBox.addItem ("None", 1);
    for (int i = 1; i <= NumChokeGroups; ++i) chokeBox.addItem ("Group " + juce::String (i), i + 1);
    chokeBox.setTooltip ("Pads in the same choke group cut each other off");
    chokeBox.onChange = [this] { proc.pads[sel()].choke = chokeBox.getSelectedId() - 1; };
    voiceBox.addItem ("Poly", 1); voiceBox.addItem ("Mono", 2); voiceBox.addItem ("Gated", 3);
    voiceBox.setTooltip ("Poly: hits overlap. Mono: a new hit cuts the last. Gated: note-off starts RELEASE.");
    voiceBox.onChange = [this] { proc.pads[sel()].voiceMode = voiceBox.getSelectedId() - 1; };
    for (auto* b : { &outBox, &chokeBox, &voiceBox }) addAndMakeVisible (*b);

    // SAMPLE view: page tabs, layer strip, waveform, knob rows
    for (int t = 0; t < 5; ++t) {
        tabs[t].setClickingTogglesState (true);
        tabs[t].onClick = [this, t] { page = t; rebuildPage(); };
        samplePanel.addAndMakeVisible (tabs[t]);
    }
    lockHint.setFont (juce::Font (9.5f, juce::Font::bold));
    lockHint.setColour (juce::Label::textColourId, kDim);
    samplePanel.addAndMakeVisible (lockHint);
    fmodeLab.setFont (juce::Font (9.5f, juce::Font::bold));
    fmodeLab.setColour (juce::Label::textColourId, kLab);
    fmodeLab.setJustificationType (juce::Justification::centredRight);
    fmodeBox.addItemList ({ "LP", "BP", "HP" }, 1);
    samplePanel.addAndMakeVisible (fmodeLab); samplePanel.addAndMakeVisible (fmodeBox);
    layersLab.setFont (juce::Font (9.5f, juce::Font::bold));
    layersLab.setColour (juce::Label::textColourId, kLab);
    samplePanel.addAndMakeVisible (layersLab);
    for (auto* b : { &stackB, &rrB, &velB }) { b->setClickingTogglesState (true); samplePanel.addAndMakeVisible (*b); }
    stackB.setTooltip ("STACK: every loaded layer plays on each hit");
    rrB.setTooltip ("ROUND ROBIN: hits cycle through the loaded layers");
    velB.setTooltip ("VELOCITY: each layer plays only inside its velocity range (VEL LO / VEL HI below)");
    stackB.onClick = [this] { proc.setLayerMode (sel(), LM_Stack); refreshAll(); };
    rrB.onClick = [this] { proc.setLayerMode (sel(), LM_RoundRobin); refreshAll(); };
    velB.onClick = [this] { proc.setLayerMode (sel(), LM_Velocity); refreshAll(); };
    for (int l = 0; l < NumLayers; ++l) { layerCells.push_back (std::make_unique<LayerCell> (l, *this)); samplePanel.addAndMakeVisible (*layerCells.back()); }
    samplePanel.addAndMakeVisible (wave);
    for (auto* b : { &prevFileB, &nextFileB, &loadB, &revB, &normB, &clearB }) wave.addAndMakeVisible (*b);
    wave.addChildComponent (padScope); wave.addChildComponent (padSpectrum);
    wave.addAndMakeVisible (scopeB);
    scopeB.setClickingTogglesState (true);
    scopeB.setTooltip ("Show the selected pad's output as a scope and spectrum instead of the waveform");
    scopeB.onClick = [this] {
        bool on = scopeB.getToggleState();
        padScope.setVisible (on); padSpectrum.setVisible (on);
        wave.repaint();
    };
    prevFileB.setTooltip ("Previous file in this folder"); nextFileB.setTooltip ("Next file in this folder");
    prevFileB.onClick = [this] { stepLayerFile (-1); };
    nextFileB.onClick = [this] { stepLayerFile (+1); };
    loadB.onClick = [this] { chooseFilesForLayer (sel(), selLayer()); };
    revB.setClickingTogglesState (true); normB.setClickingTogglesState (true);
    revB.setColour (juce::Slider::rotarySliderFillColourId, kAmber);
    normB.setColour (juce::Slider::rotarySliderFillColourId, kGreen);
    revB.onClick = [this] { proc.pads[sel()].layers[selLayer()].reverse = revB.getToggleState(); wave.repaint(); };
    normB.onClick = [this] { proc.pads[sel()].layers[selLayer()].normalize = normB.getToggleState(); };
    clearB.onClick = [this] { proc.clearLayer (sel(), selLayer()); refreshAll(); };
    for (auto* l : { &row1Lab, &row2Lab }) { l->setFont (juce::Font (9.5f, juce::Font::bold)); samplePanel.addAndMakeVisible (*l); }
    addAndMakeVisible (samplePanel);

    macroPanel = std::make_unique<MacroPanel> (*this);
    addAndMakeVisible (*macroPanel);

    // PART FX view
    fxPanel.addAndMakeVisible (fxTitle); fxPanel.addAndMakeVisible (fxHint);
    fxPanel.addAndMakeVisible (partFxPower); fxPanel.addAndMakeVisible (partFxLock);
    partFxLock.setClickingTogglesState (true);
    partFxLock.setTooltip ("Lock this pad's effects chain from RANDOMIZE / MUTATE / KIT RND");
    partFxLock.setColour (juce::Slider::rotarySliderFillColourId, kAmber);
    partFxLock.onClick = [this] { proc.setLocked (sel(), SamplerProcessor::LockFx, partFxLock.getToggleState()); };
    partFxPower.setClickingTogglesState (true);
    partFxPower.setColour (juce::Slider::rotarySliderFillColourId, kGreen);
    partFxPower.onClick = [this] { proc.pads[sel()].fx.enabled.store (partFxPower.getToggleState()); refreshFxRows(); };
    partFxView = std::make_unique<FxChainView> (*this, false);
    fxPanel.addAndMakeVisible (*partFxView);
    addAndMakeVisible (fxPanel);

    // GLOBAL FX view
    globalPanel.addAndMakeVisible (gloTitle); globalPanel.addAndMakeVisible (globPower);
    globPower.setClickingTogglesState (true);
    globPower.setColour (juce::Slider::rotarySliderFillColourId, kGreen);
    globPower.onClick = [this] { proc.globalFx.enabled.store (globPower.getToggleState()); refreshFxRows(); };
    globFxView = std::make_unique<FxChainView> (*this, true);
    globalPanel.addAndMakeVisible (*globFxView);
    addAndMakeVisible (globalPanel);

    // footer
    addAndMakeVisible (outTitle); addAndMakeVisible (masterFader); addAndMakeVisible (limitB); addAndMakeVisible (outMeter);
    masterFader.setColour (juce::Slider::thumbColourId, kAccent);
    masterFader.setColour (juce::Slider::trackColourId, kAccent.withAlpha (0.5f));
    masterFader.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a2a34));
    masterFaderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterFader);

    browser = std::make_unique<PresetBrowser> (proc, lnf);
    browser->onClose = [this] { browser->setVisible (false); };
    browser->onPresetChanged = [this] { refreshAll(); };
    addAndMakeVisible (*browser); // last: on top
    browser->setVisible (false);

    setSize (1280, 800);
    setResizeLimits (1100, 700, 2560, 1600);
    setResizable (true, true);
    refreshAll();
    layoutMainView();
    startTimerHz (30);
}

SamplerEditor::~SamplerEditor() { stopTimer(); setLookAndFeel (nullptr); }

ScopeSource SamplerEditor::selectedScope() const {
    ScopeSource s;
    int p = sel();
    s.tap = &proc.pads[p].scope;
    s.colour = padColour (p);
    s.title = "PAD " + juce::String (p + 1);
    s.sampleRate = proc.currentSampleRate();
    return s;
}

// ---------------- knobs ----------------
std::unique_ptr<Knob> SamplerEditor::makePadKnob (int p, int param) {
    auto col = padColour (p);
    auto& m = padParamMeta (param);
    auto kb = std::make_unique<Knob> (m.label, col);
    kb->val.setColour (juce::Label::textColourId, col.brighter (0.2f));
    kb->attach (proc.apvts, padParamID (p, param));
    kb->s.onValueChange = [kbp = kb.get(), param] { kbp->val.setText (formatPadParam (param, (float) kbp->s.getValue()), juce::dontSendNotification); };
    kb->s.onValueChange();
    kb->setBipolar (m.min < 0.f && param != PP_Level);
    kb->locked = proc.isLocked (p, param);
    for (int k = 0; k < NumMacros; ++k) if (proc.macroDepth (p, k, param) != 0.f) ++kb->macroCount;
    kb->s.onRightClick = [this, p, param, kbp = kb.get()] { showParamMenu (p, param, kbp); };
    return kb;
}

std::unique_ptr<Knob> SamplerEditor::makeLayerKnob (int p, int li, int lp) {
    auto col = padColour (p);
    auto& m = layerParamMeta (lp);
    auto& L = proc.pads[p].layers[li];
    auto kb = std::make_unique<Knob> (m.label, col);
    kb->val.setColour (juce::Label::textColourId, col.brighter (0.2f));
    float span = m.max - m.min;
    auto d = L.get();
    double secs = d != nullptr ? d->seconds() : 0.0;
    kb->setManual ((L.v[lp].load() - m.min) / span,
        [this, p, li, lp, mn = m.min, span] (float v01) {
            auto& LL = proc.pads[p].layers[li];
            float v = mn + v01 * span;
            if (lp == LP_Start) v = juce::jmin (v, LL.v[LP_End].load() - 0.002f);
            if (lp == LP_End) v = juce::jmax (v, LL.v[LP_Start].load() + 0.002f);
            LL.v[lp] = v;
            wave.repaint();
        },
        [mn = m.min, span, lp, secs] (float v01) { return formatLayerParam (lp, mn + v01 * span, secs); });
    kb->s.setDoubleClickReturnValue (true, (m.def - m.min) / span);
    kb->setBipolar (m.min < 0.f && lp != LP_Level);
    kb->locked = proc.isLocked (p, SamplerProcessor::LockLayers);
    kb->s.onRightClick = [this, p] {
        proc.toggleLock (p, SamplerProcessor::LockLayers);
        for (auto& k : row1) { k->locked = proc.isLocked (p, SamplerProcessor::LockLayers); k->repaint(); }
    };
    return kb;
}

std::unique_ptr<Knob> SamplerEditor::makeAtomicKnob (const juce::String& label, std::atomic<float>& target,
                                                    std::function<juce::String (float)> fmt, int) {
    auto col = padColour (sel());
    auto kb = std::make_unique<Knob> (label, col);
    kb->val.setColour (juce::Label::textColourId, col.brighter (0.2f));
    kb->setManual (target.load(), [&target] (float v) { target = v; }, fmt);
    return kb;
}

void SamplerEditor::showParamMenu (int p, int param, Knob*) {
    static const float depths[] = { 1.f, 0.5f, 0.25f, 0.1f, -0.1f, -0.25f, -0.5f, -1.f };
    auto pct = [] (float d) { int v = juce::roundToInt (d * 100.f); return (v > 0 ? "+" : "") + juce::String (v) + "%"; };
    juce::PopupMenu m;
    m.addSectionHeader (paramLabel (param));
    m.addItem (1, "Lock (RANDOMIZE / MUTATE skip it)", true, proc.isLocked (p, param));
    m.addSeparator();
    m.addSectionHeader ("ASSIGN TO MACRO");
    for (int k = 0; k < NumMacros; ++k) {
        float cur = proc.macroDepth (p, k, param);
        juce::PopupMenu sub;
        for (int di = 0; di < (int) std::size (depths); ++di)
            sub.addItem (100 + k * 20 + di, pct (depths[di]), true, cur != 0.f && std::abs (cur - depths[di]) < 1.0e-3f);
        if (cur != 0.f) { sub.addSeparator(); sub.addItem (100 + k * 20 + 19, "Remove"); }
        m.addSubMenu (juce::String (k + 1) + "  " + proc.pads[p].macros[k].name + (cur != 0.f ? "   " + pct (cur) : juce::String()), sub);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Desktop::getMousePosition().getX() > 0
                         ? juce::Rectangle<int> (juce::Desktop::getMousePosition(), juce::Desktop::getMousePosition().translated (1, 1))
                         : getScreenBounds()),
        [sp = juce::Component::SafePointer<SamplerEditor> (this), p, param] (int id) {
            if (sp == nullptr || id <= 0) return;
            if (id == 1) sp->proc.toggleLock (p, param);
            else if (id >= 100) {
                int k = (id - 100) / 20, di = (id - 100) % 20;
                sp->proc.setMacroTarget (p, k, param, di == 19 ? 0.f : depths[di]);
            }
            sp->refreshAll();
        });
}

// ---------------- pages ----------------
void SamplerEditor::rebuildPage() {
    for (auto& k : row1) samplePanel.removeChildComponent (k.get());
    for (auto& k : row2) samplePanel.removeChildComponent (k.get());
    row1.clear(); row2.clear();
    int p = sel(), li = selLayer();
    auto col = padColour (p);
    for (int t = 0; t < 5; ++t) {
        tabs[t].setToggleState (t == page, juce::dontSendNotification);
        tabs[t].setColour (juce::Slider::rotarySliderFillColourId, col);
    }
    juce::String l1, l2;
    auto padRow = [&] (std::vector<std::unique_ptr<Knob>>& row, std::initializer_list<int> params) {
        for (int prm : params) row.push_back (makePadKnob (p, prm));
    };
    switch (page) {
        case 0:
            for (int lp = 0; lp < NumLayerParams; ++lp) row1.push_back (makeLayerKnob (p, li, lp));
            padRow (row2, { PP_Drive, PP_Comp, PP_Tone, PP_Punch });
            l1 = "LAYER " + juce::String (li + 1); l2 = "PAD";
            if (proc.pads[p].layerMode.load() == LM_Velocity) {   // this layer's velocity range
                for (int hi = 0; hi < 2; ++hi) {
                    auto& L = proc.pads[p].layers[li];
                    auto& tgt = hi ? L.hiVel : L.loVel;
                    auto kb = std::make_unique<Knob> (hi ? "VEL HI" : "VEL LO", col);
                    kb->val.setColour (juce::Label::textColourId, col.brighter (0.2f));
                    kb->setManual ((float) tgt.load() / 127.f, [this, &L, hi] (float v01) {
                        int v = juce::roundToInt (v01 * 127.f);
                        if (hi) L.hiVel = juce::jmax (v, L.loVel.load()); else L.loVel = juce::jmin (v, L.hiVel.load());
                        for (auto& c : layerCells) c->repaint();
                    }, [] (float v01) { return juce::String (juce::roundToInt (v01 * 127.f)); });
                    kb->s.setRange (0.0, 1.0, 1.0 / 127.0);
                    kb->s.setDoubleClickReturnValue (true, hi ? 1.0 : 0.0);
                    row2.push_back (std::move (kb));
                }
                l2 = "PAD  |  LAYER " + juce::String (li + 1) + " VELOCITY RANGE";
            }
            break;
        case 1:
            padRow (row1, { PP_Attack, PP_Hold, PP_Decay, PP_Release, PP_Level, PP_Pan });
            padRow (row2, { PP_Length, PP_VelAmp, PP_VelStart });
            l1 = "ENVELOPE + MIX"; l2 = "LENGTH + VELOCITY";
            break;
        case 2:
            padRow (row1, { PP_Tune, PP_PitchAmt, PP_PitchDecay, PP_PitchCurve, PP_RandPitch });
            l1 = "PAD PITCH (ADDS TO EACH LAYER'S TUNE)";
            break;
        case 3:
            padRow (row1, { PP_Cutoff, PP_Reso, PP_FltEnv, PP_FltDecay, PP_VelFilt });
            l1 = "FILTER (PER HIT)";
            break;
        default: {
            padRow (row1, { PP_SendRev, PP_SendDly });
            auto pct = [] (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; };
            row2.push_back (makeAtomicKnob ("REV DECAY", proc.revDecay, [] (float v) { return juce::String (FxProcessor::rt60FromKnob (v), 2) + " s"; }, -1));
            row2.push_back (makeAtomicKnob ("REV DAMP", proc.revDamp, pct, -1));
            auto* pr = &proc;
            auto dlyFmt = [pr] (float v) {
                int d = pr->dlySync.load();
                float sec = d >= 0 ? (float) TempoSync::divisionSeconds (d, pr->hostBpm.load()) : FxProcessor::sendDelaySeconds (v);
                return (d >= 0 ? juce::String (TempoSync::divisionName (d)) + "  " : juce::String()) + juce::String (juce::roundToInt (sec * 1000.f)) + " ms";
            };
            row2.push_back (makeAtomicKnob ("DLY TIME", proc.dlyTime, dlyFmt, -1));
            auto* timeK = row2.back().get();
            row2.push_back (makeAtomicKnob ("DLY FDBK", proc.dlyFb, pct, -1));
            // DLY SYNC: FREE, then the shared tempo divisions (host tempo, 120 BPM without one)
            static constexpr int steps = TempoSync::NumDivisions;   // static: usable in the lambdas below without a capture (MSVC)
            auto sk = std::make_unique<Knob> ("DLY SYNC", col);
            sk->val.setColour (juce::Label::textColourId, col.brighter (0.2f));
            sk->setManual ((float) (proc.dlySync.load() + 1) / steps, [pr, timeK] (float v) {
                pr->dlySync = juce::roundToInt (v * steps) - 1;
                timeK->s.setAlpha (pr->dlySync.load() >= 0 ? 0.45f : 1.f);
                timeK->val.setText (timeK->fmtr ((float) timeK->s.getValue()), juce::dontSendNotification);
            }, [] (float v) { int d = juce::roundToInt (v * steps) - 1; return d < 0 ? juce::String ("FREE") : juce::String (TempoSync::divisionName (d)); });
            sk->s.setRange (0.0, 1.0, 1.0 / steps);
            sk->s.setTooltip ("FREE: DLY TIME sets the delay. Otherwise the delay follows the host tempo.");
            timeK->s.setAlpha (proc.dlySync.load() >= 0 ? 0.45f : 1.f);
            row2.push_back (std::move (sk));
            l1 = "SENDS"; l2 = "SHARED RETURNS (ALL PADS)";
            break;
        }
    }
    row1Lab.setText (l1, juce::dontSendNotification);
    row2Lab.setText (l2, juce::dontSendNotification);
    row1Lab.setColour (juce::Label::textColourId, col);
    row2Lab.setColour (juce::Label::textColourId, col);
    for (auto& k : row1) samplePanel.addAndMakeVisible (*k);
    for (auto& k : row2) samplePanel.addAndMakeVisible (*k);
    bool flt = page == 3;
    fmodeLab.setVisible (flt); fmodeBox.setVisible (flt);
    fmodeAtt.reset();
    fmodeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, padFilterModeID (p), fmodeBox);
    resized();
}

void SamplerEditor::refreshHeader() {
    int p = sel();
    auto& pe = proc.pads[p];
    auto col = padColour (p);
    auto nm = proc.padDisplayName (p);
    nameLab.setText (nm, juce::dontSendNotification);
    outBox.setSelectedId (pe.output.load() + 1, juce::dontSendNotification);
    for (int i = 0; i < NumAuxOuts; ++i) {
        auto* bus = proc.getBus (false, i + 1);
        outBox.changeItemText (i + 2, "Out " + juce::String (i + 2) + (bus != nullptr && bus->isEnabled() ? "" : " (off)"));
    }
    outBox.setText (outBox.getItemText (outBox.getSelectedItemIndex()), juce::dontSendNotification); // shows "(off)" too
    chokeBox.setSelectedId (pe.choke.load() + 1, juce::dontSendNotification);
    voiceBox.setSelectedId (pe.voiceMode.load() + 1, juce::dontSendNotification);
    const int lm = pe.layerMode.load();
    stackB.setToggleState (lm == LM_Stack, juce::dontSendNotification);
    rrB.setToggleState (lm == LM_RoundRobin, juce::dontSendNotification);
    velB.setToggleState (lm == LM_Velocity, juce::dontSendNotification);
    for (auto* b : { &stackB, &rrB, &velB, &trigB }) b->setColour (juce::Slider::rotarySliderFillColourId, col);
    auto& L = pe.layers[selLayer()];
    revB.setToggleState (L.reverse.load(), juce::dontSendNotification);
    normB.setToggleState (L.normalize.load(), juce::dontSendNotification);
    bool has = L.hasSample();
    for (auto* b : { &revB, &normB, &clearB, &prevFileB, &nextFileB }) b->setEnabled (has || (b == &clearB && L.path.isNotEmpty()));
    for (auto& c : layerCells) c->repaint();
    if (macroPanel) macroPanel->refresh();
    wave.repaint();
    repaint (padNumRect);
}

void SamplerEditor::refreshFxRows() {
    if (partFxView) partFxView->refresh();
    if (globFxView) globFxView->refresh();
    bool pe = proc.pads[sel()].fx.enabled.load();
    partFxPower.setToggleState (pe, juce::dontSendNotification);
    partFxPower.setButtonText (pe ? "ON" : "OFF");
    partFxLock.setToggleState (proc.isLocked (sel(), SamplerProcessor::LockFx), juce::dontSendNotification);
    bool ge = proc.globalFx.enabled.load();
    globPower.setToggleState (ge, juce::dontSendNotification);
    globPower.setButtonText (ge ? "ON" : "OFF");
    limitB.setToggleState (proc.limiterOn.load(), juce::dontSendNotification);
}

// the selected pad's colour drives the shared accent: view tabs, toggles, output
// fader and limiter all follow the pad being edited
void SamplerEditor::applyPadAccent (int p) {
    const auto col = padColour (p);
    lnf.accent = col;
    lnf.setColour (juce::Slider::rotarySliderFillColourId, col);
    for (auto* b : { &viewSample, &viewPartFx, &viewGlobFx, &viewBrowser })
        b->setColour (juce::Slider::rotarySliderFillColourId, col);
    masterFader.setColour (juce::Slider::thumbColourId, col);
    masterFader.setColour (juce::Slider::trackColourId, col.withAlpha (0.5f));
    repaint();
}

void SamplerEditor::refreshAll() {
    applyPadAccent (sel());
    refreshHeader();
    rebuildPage();
    refreshFxRows();
    for (auto& pv : padViews) pv->repaint();
    if (browserPanel) { browserPanel->model.accent = padColour (sel()); browserPanel->refreshHint(); browserPanel->list.repaint(); }
    lastVer = proc.stateVersion.load(); lastSel = sel(); lastLayer = selLayer(); lastKit = proc.currentKit;
}

void SamplerEditor::selectPad (int p) {
    if (p != sel()) proc.selectedLayer = 0;
    proc.selectedPad = juce::jlimit (0, NumPads - 1, p);
    refreshAll();
}
void SamplerEditor::selectLayer (int l) {
    proc.selectedLayer = juce::jlimit (0, NumLayers - 1, l);
    refreshHeader();
    rebuildPage();
    lastLayer = selLayer();
}

void SamplerEditor::chooseFilesForLayer (int pad, int layer) {
    juce::File start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    for (auto& L : proc.pads[pad].layers)
        if (juce::File::isAbsolutePath (L.path)) { start = juce::File (L.path).getParentDirectory(); break; }
    chooser = std::make_unique<juce::FileChooser> (layer < 0 ? "Load samples into pad " + juce::String (pad + 1) + " (up to 4 = layers)"
                                                             : "Load sample into layer " + juce::String (layer + 1),
                                                   start, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
               | juce::FileBrowserComponent::canSelectMultipleItems;
    chooser->launchAsync (flags, [sp = juce::Component::SafePointer<SamplerEditor> (this), pad, layer] (const juce::FileChooser& fc) {
        if (sp == nullptr) return;
        auto res = fc.getResults();
        if (res.isEmpty()) return;
        if (layer < 0) sp->proc.loadPadFiles (pad, res);
        else {
            sp->proc.pushUndo();
            for (int i = 0; i < res.size() && layer + i < NumLayers; ++i) sp->proc.loadLayer (pad, layer + i, res[i], false);
        }
        sp->refreshAll();
    });
}

void SamplerEditor::loadFileFromBrowser (const juce::File& f, int pad, int layer) {
    if (! SamplerProcessor::isAudioFile (f)) return;
    pad = juce::jlimit (0, NumPads - 1, pad);
    bool ok;
    if (layer < 0) { proc.loadPadFiles (pad, { f }); ok = proc.numLoadedLayers (pad) > 0; layer = 0; }
    else ok = proc.loadLayer (pad, juce::jlimit (0, NumLayers - 1, layer), f);
    if (! ok) return;
    proc.stopPreview();
    proc.selectedPad = pad;
    proc.selectedLayer = layer;
    refreshAll();
    proc.auditionPad (pad);
}

void SamplerEditor::stepLayerFile (int dir) {
    auto& L = proc.pads[sel()].layers[selLayer()];
    if (! juce::File::isAbsolutePath (L.path)) return;
    juce::File cur (L.path);
    auto all = cur.getParentDirectory().findChildFiles (juce::File::findFiles, false);
    juce::Array<juce::File> files;
    for (auto& f : all) if (SamplerProcessor::isAudioFile (f)) files.add (f);
    if (files.isEmpty()) return;
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    int i = files.indexOf (cur);
    int n = files.size();
    for (int k = 1; k <= n; ++k) {
        int j = ((i < 0 ? 0 : i) + dir * k + n * 4) % n;
        if (proc.loadLayer (sel(), selLayer(), files[j])) { proc.auditionPad (sel()); break; }
    }
    refreshAll();
}

// ---------------- layout ----------------
void SamplerEditor::paint (juce::Graphics& g) {
    g.fillAll (kBg);
    if (footerRect.getWidth() > 0) { g.setColour (juce::Colour (0xff141419)); g.fillRect (footerRect); }
    g.setColour (padColour (sel()));
    g.setFont (juce::Font (18.f, juce::Font::bold));
    g.drawText (juce::String (sel() + 1), padNumRect, juce::Justification::centredLeft);
}

void SamplerEditor::resized() {
    if (getWidth() < 50 || getHeight() < 50 || padViews.size() < (size_t) NumPads || ! macroPanel) return;
    auto r = getLocalBounds();
    auto header = r.removeFromTop (52);
    prevB.setBounds (header.removeFromLeft (30).reduced (2));
    presetBtn.setBounds (header.removeFromLeft (230).reduced (2));
    nextB.setBounds (header.removeFromLeft (30).reduced (2));
    header.removeFromLeft (8);
    undoB.setBounds (header.removeFromLeft (62).reduced (2));
    redoB.setBounds (header.removeFromLeft (62).reduced (2));
    initB.setBounds (header.removeFromLeft (54).reduced (2));
    scopeBox.setBounds (header.removeFromLeft (80).reduced (2));
    randPartB.setBounds (header.removeFromLeft (110).reduced (2));
    mutPartB.setBounds (header.removeFromLeft (100).reduced (2));
    randKitB.setBounds (header.removeFromLeft (70).reduced (2));
    auto mhead = header.removeFromRight (120);
    masterLab.setBounds (mhead.removeFromLeft (60).reduced (2));
    masterDial.setBounds (mhead.removeFromLeft (46).reduced (2));
    voiceLab.setBounds (header.removeFromRight (110).reduced (4));
    kitLab.setBounds (header.reduced (4));
    kitLab.setVisible (getWidth() >= 1180);

    footerRect = r.removeFromBottom (64);
    auto orr = footerRect.reduced (8);
    outTitle.setBounds (orr.removeFromLeft (80).reduced (2));
    limitB.setBounds (orr.removeFromRight (90).reduced (14, 8));
    outMeter.setBounds (orr.removeFromRight (180).reduced (4, 12));
    masterFader.setBounds (orr.reduced (2));

    // pads: 4x4 grid, pad 1 bottom-left
    auto body = r.reduced (8, 6);
    const int gap = 6;
    float padH = (body.getHeight() - 3.f * gap) / 4.f, padW = padH * 0.834f;
    if (padW * 4 + 3 * gap > body.getWidth() * 0.46f) padW = (body.getWidth() * 0.46f - 3 * gap) / 4.f;
    int gridW = (int) (padW * 4 + 3 * gap);
    padArea = body.removeFromLeft (gridW);
    for (int i = 0; i < NumPads; ++i) {
        int row = 3 - i / 4, col = i % 4;
        padViews[(size_t) i]->setBounds ((int) (padArea.getX() + col * (padW + gap)), (int) (padArea.getY() + row * (padH + gap)),
                                         (int) padW, (int) padH);
    }
    body.removeFromLeft (14);

    // right panel
    auto rp = body;
    macroPanel->setBounds (rp.removeFromBottom (juce::jmin (148, rp.getHeight() / 4)));
    rp.removeFromBottom (8);
    auto tr = rp.removeFromTop (28);
    viewSample.setBounds (tr.removeFromLeft (112).reduced (1));
    tr.removeFromLeft (4);
    viewPartFx.setBounds (tr.removeFromLeft (112).reduced (1));
    tr.removeFromLeft (4);
    viewGlobFx.setBounds (tr.removeFromLeft (112).reduced (1));
    tr.removeFromLeft (4);
    viewBrowser.setBounds (tr.removeFromLeft (112).reduced (1));
    trigB.setBounds (tr.removeFromRight (84).reduced (1));
    rp.removeFromTop (8);
    auto hr = rp.removeFromTop (26);
    padNumRect = hr.removeFromLeft (26);
    nameLab.setBounds (hr.removeFromLeft (juce::jmin (190, hr.getWidth() / 4)).reduced (0, 1));
    hr.removeFromLeft (8);
    auto place = [&hr] (juce::Label& l, int lw, juce::ComboBox& b, int bw) {
        l.setBounds (hr.removeFromLeft (lw)); hr.removeFromLeft (4);
        b.setBounds (hr.removeFromLeft (bw).reduced (0, 1)); hr.removeFromLeft (8);
    };
    place (outLab, 28, outBox, 108);
    place (chokeLab, 42, chokeBox, 92);
    place (voiceModeLab, 40, voiceBox, 76);
    rp.removeFromTop (6);
    samplePanel.setBounds (rp);
    if (browserPanel) browserPanel->setBounds (rp);
    fxPanel.setBounds (rp);
    globalPanel.setBounds (rp);

    // sample panel internals
    auto sp = samplePanel.getLocalBounds();
    auto tabRow = sp.removeFromTop (24);
    const int tw[5] = { 60, 44, 50, 54, 62 };
    for (int t = 0; t < 5; ++t) { tabs[t].setBounds (tabRow.removeFromLeft (tw[t])); tabRow.removeFromLeft (4); }
    fmodeBox.setBounds (tabRow.removeFromRight (64));
    fmodeLab.setBounds (tabRow.removeFromRight (42));
    lockHint.setBounds (tabRow.reduced (4, 0));
    sp.removeFromTop (6);
    auto lr = sp.removeFromTop (26);
    layersLab.setBounds (lr.removeFromLeft (50));
    stackB.setBounds (lr.removeFromLeft (56));
    rrB.setBounds (lr.removeFromLeft (92));
    velB.setBounds (lr.removeFromLeft (70));
    lr.removeFromLeft (8);
    int cw = (lr.getWidth() - 3 * 4) / NumLayers;
    for (int l = 0; l < NumLayers; ++l) layerCells[(size_t) l]->setBounds (lr.getX() + l * (cw + 4), lr.getY(), cw, lr.getHeight());
    sp.removeFromTop (6);
    int rowH = juce::jlimit (80, 108, sp.getHeight() / 4);
    auto knobArea = sp.removeFromBottom (rowH * 2);
    sp.removeFromBottom (4);
    wave.setBounds (sp);
    {
        auto wb = wave.getLocalBounds().reduced (6, 4).removeFromTop (18);
        clearB.setBounds (wb.removeFromRight (48)); wb.removeFromRight (3);
        normB.setBounds (wb.removeFromRight (76)); wb.removeFromRight (3);
        revB.setBounds (wb.removeFromRight (66)); wb.removeFromRight (3);
        loadB.setBounds (wb.removeFromRight (60)); wb.removeFromRight (3);
        nextFileB.setBounds (wb.removeFromRight (20)); wb.removeFromRight (2);
        prevFileB.setBounds (wb.removeFromRight (20)); wb.removeFromRight (3);
        scopeB.setBounds (wb.removeFromRight (56));
        auto pa = wave.plotArea().toNearestInt();
        padScope.setBounds (pa.removeFromLeft (pa.getWidth() / 2).reduced (2));
        padSpectrum.setBounds (pa.reduced (2));
    }
    auto layoutRow = [] (std::vector<std::unique_ptr<Knob>>& row, juce::Label& lab, juce::Rectangle<int> area) {
        lab.setBounds (area.removeFromTop (14));
        int kw = area.getWidth() / 6;
        for (size_t i = 0; i < row.size(); ++i) row[i]->setBounds (area.getX() + (int) i * kw, area.getY(), kw, area.getHeight());
    };
    layoutRow (row1, row1Lab, knobArea.removeFromTop (rowH));
    layoutRow (row2, row2Lab, knobArea);
    row2Lab.setVisible (! row2.empty());

    // FX panels (same as the synth)
    auto fr = fxPanel.getLocalBounds().reduced (4);
    auto fxHead = fr.removeFromTop (20);
    partFxPower.setBounds (fxHead.removeFromRight (56).reduced (1));
    partFxLock.setBounds (fxHead.removeFromRight (56).reduced (1));
    fxHint.setBounds (fxHead.removeFromRight (140).reduced (1));
    fxTitle.setBounds (fxHead.reduced (1));
    if (partFxView) partFxView->setBounds (fr);
    auto gr = globalPanel.getLocalBounds().reduced (4);
    auto gHead = gr.removeFromTop (20);
    globPower.setBounds (gHead.removeFromRight (56).reduced (1));
    gloTitle.setBounds (gHead.reduced (1));
    if (globFxView) globFxView->setBounds (gr);

    if (browser) browser->setBounds (getLocalBounds());
}

void SamplerEditor::layoutMainView() {
    viewSample.setToggleState (mainView == 0, juce::dontSendNotification);
    viewPartFx.setToggleState (mainView == 1, juce::dontSendNotification);
    viewGlobFx.setToggleState (mainView == 2, juce::dontSendNotification);
    viewBrowser.setToggleState (mainView == 3, juce::dontSendNotification);
    if (browserPanel) { browserPanel->setVisible (mainView == 3); if (mainView == 3) browserPanel->refreshList(); }
    if (mainView != 3) proc.stopPreview();
    samplePanel.setVisible (mainView == 0);
    fxPanel.setVisible (mainView == 1);
    globalPanel.setVisible (mainView == 2);
}

void SamplerEditor::timerCallback() {
    int ver = proc.stateVersion.load();
    if (lastSel != sel() || lastLayer != selLayer() || ver != lastVer || lastKit != proc.currentKit)
        if (juce::ModalComponentManager::getInstance()->getNumModalComponents() == 0 && ! nameLab.isBeingEdited()) // never rebuild under an open menu / editor
            refreshAll();
    if (lastKitName != proc.kitName) {
        lastKitName = proc.kitName;
        auto style = presetGenreOf (proc.currentKit);
        kitLab.setText (style.isEmpty() ? proc.kitName : proc.kitName + "  |  " + style, juce::dontSendNotification);
        presetBtn.setButtonText (proc.kitName);
    }
    presetBtn.setButtonText (proc.isLoadingKit() ? juce::String ("LOADING...") : proc.kitName);
    undoB.setEnabled (! proc.undoStack.empty());
    redoB.setEnabled (! proc.redoStack.empty());
    voiceLab.setText ("VOICES " + juce::String (proc.activeVoices.load()) + " / " + juce::String (MaxVoices), juce::dontSendNotification);
    for (auto& pv : padViews) pv->tick();
    outMeter.repaint();
    if (mainView == 0) {
        wave.repaint();
        for (auto& c : layerCells) c->repaint();
    }
}

} // namespace mds

juce::AudioProcessorEditor* mds::SamplerProcessor::createEditor() { return new mds::SamplerEditor (*this); }
