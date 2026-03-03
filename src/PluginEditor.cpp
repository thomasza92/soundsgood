/*
  ==============================================================================
    PluginEditor.cpp
    Soundsgood -- WPPM synthesiser editor.

    Layout (4 rows):
      Row 1 — WPPM (waveform viz + Split + Pulse1 + Link + Pulse2)
      Row 2 — Sub + Unison + Filter
      Row 3 — LFO [1|2] + Mod Matrix + Envelope [Amp|Mod]
      Row 4 — Analog + Quality + Output (Vol + Scope)

    Tab system: clickable tab buttons in section headers like Serum.
  ==============================================================================
*/

#include "PluginEditor.h"

#include <cmath>

//==============================================================================
static constexpr int kKnobW = 56;
static constexpr int kKnobH = 68;
static constexpr int kLabelH = 14;
static constexpr int kPad = 4;
static constexpr int kSectionPad = 8;
static constexpr int kHeaderH = 18;
static constexpr int kComboW = 72;
static constexpr int kComboH = 22;
static constexpr int kKnobStep = kKnobW + kPad;                   // 60
static constexpr int kComboStep = kComboW + kPad;                 // 76
static constexpr int kRowH = kHeaderH + kLabelH + kKnobH + kPad;  // 104

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kTwoPi = 6.28318530717958647692f;

// Catppuccin Mocha palette
static const juce::Colour kBgColour(0xff1e1e2e);
static const juce::Colour kSectionBg(0xff181825);
static const juce::Colour kSectionBorder(0xff45475a);
static const juce::Colour kAccentBlue(0xff89b4fa);
static const juce::Colour kAccentPurple(0xffcba6f7);
static const juce::Colour kTextColour(0xffcdd6f4);
static const juce::Colour kDarkBg(0xff11111b);
static const juce::Colour kTabActive(0xff313244);
static const juce::Colour kTabInactive(0xff1e1e2e);

//==============================================================================
SoundsgoodAudioProcessorEditor::SoundsgoodAudioProcessorEditor(
    SoundsgoodAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p) {
  // --- WPPM ---
  setupKnob(splitKnob, "d", "Split");
  setupKnob(ratio1Knob, "R1", "Ratio 1");
  setupKnob(depth1Knob, "I1", "Depth 1");
  setupKnob(phase1Knob, "P1", "Phase 1");
  setupKnob(ratio2Knob, "R2", "Ratio 2");
  setupKnob(depth2Knob, "I2", "Depth 2");
  setupKnob(phase2Knob, "P2", "Phase 2");

  linkAttach = std::make_unique<ButtonAttach>(processorRef.getAPVTS(), "link",
                                              linkButton);
  linkButton.setColour(juce::ToggleButton::textColourId, kTextColour);
  addAndMakeVisible(linkButton);

  // --- Sub ---
  setupCombo(subOctaveCombo, "subOctave", "Sub Oct",
             {"Off", "-1 Oct", "-2 Oct"});
  setupKnob(subMixKnob, "subMix", "Sub Mix");

  // --- Unison ---
  setupKnob(unisonVoicesKnob, "unisonVoices", "Voices");
  setupKnob(unisonDetuneKnob, "unisonDetune", "Detune");
  setupKnob(unisonSpreadKnob, "unisonSpread", "Spread");

  // --- Filter ---
  setupCombo(filterModeCombo, "filterMode", "Mode",
             {"Low Pass", "High Pass", "Band Pass"});
  setupKnob(filterCutoffKnob, "filterCutoff", "Cutoff");
  setupKnob(filterResonanceKnob, "filterResonance", "Reso");
  setupKnob(filterKeyTrackKnob, "filterKeyTrack", "Key Trk");
  setupKnob(filterEnvAmountKnob, "filterEnvAmount", "Env Amt");

  // --- LFO 1 ---
  setupCombo(lfoShapeCombo, "lfoShape", "Shape",
             {"Sine", "Triangle", "Saw", "Square"});
  setupKnob(lfoRateKnob, "lfoRate", "Rate");
  setupKnob(lfoDepthKnob, "lfoDepth", "Depth");
  setupCombo(lfoDestCombo, "lfoDest", "Dest",
             {"Split", "Filter", "Pitch", "Ratio"});

  // --- LFO 2 ---
  setupCombo(lfo2ShapeCombo, "lfo2Shape", "Shape",
             {"Sine", "Triangle", "Saw", "Square"});
  setupKnob(lfo2RateKnob, "lfo2Rate", "Rate");
  setupKnob(lfo2DepthKnob, "lfo2Depth", "Depth");
  setupCombo(lfo2DestCombo, "lfo2Dest", "Dest",
             {"Split", "Filter", "Pitch", "Ratio"});

  // LFO tab buttons
  auto setupTab = [this](juce::TextButton& btn, bool active) {
    btn.setColour(juce::TextButton::buttonColourId,
                  active ? kTabActive : kTabInactive);
    btn.setColour(juce::TextButton::textColourOffId, kTextColour);
    btn.setColour(juce::TextButton::buttonOnColourId, kTabActive);
    addAndMakeVisible(btn);
  };

  setupTab(lfoTab1, true);
  setupTab(lfoTab2, false);
  lfoTab1.onClick = [this] { switchLFOTab(0); };
  lfoTab2.onClick = [this] { switchLFOTab(1); };

  // --- Mod Matrix ---
  setupKnob(velToDKnob, "velToD", "Vel>D");
  setupKnob(velToDepthKnob, "velToDepth", "Vel>Dep");
  setupKnob(atToDKnob, "atToD", "AT>D");
  setupKnob(atToDepthKnob, "atToDepth", "AT>Dep");

  // --- Envelope 1 (Amp) ---
  setupKnob(attackKnob, "attack", "A");
  setupKnob(decayKnob, "decay", "D");
  setupKnob(sustainKnob, "sustain", "S");
  setupKnob(releaseKnob, "release", "R");

  // --- Envelope 2 (Mod) ---
  setupKnob(env2AttackKnob, "env2Attack", "A");
  setupKnob(env2DecayKnob, "env2Decay", "D");
  setupKnob(env2SustainKnob, "env2Sustain", "S");
  setupKnob(env2ReleaseKnob, "env2Release", "R");
  setupCombo(env2DestCombo, "env2Dest", "Dest",
             {"Split", "Filter", "Depth", "Ratio", "Pitch"});
  setupKnob(env2AmountKnob, "env2Amount", "Amt");

  // Envelope tab buttons
  setupTab(envTab1, true);
  setupTab(envTab2, false);
  envTab1.onClick = [this] { switchEnvTab(0); };
  envTab2.onClick = [this] { switchEnvTab(1); };

  // --- Analog ---
  setupKnob(analogDriveKnob, "analogDrive", "Drive");
  setupKnob(analogMixKnob, "analogMix", "Wet");

  // --- Quality ---
  setupCombo(qualityCombo, "oversampleMode", "Quality",
             {"Eco", "High (4x)", "Ultra (8x)"});

  // --- Output ---
  setupKnob(volumeKnob, "volume", "Vol");

  // Initialize tab visibility
  switchLFOTab(0);
  switchEnvTab(0);

  setSize(960, 460);
  startTimerHz(30);
}

SoundsgoodAudioProcessorEditor::~SoundsgoodAudioProcessorEditor() {
  stopTimer();
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::setupKnob(LabelledKnob& k,
                                               const juce::String& paramId,
                                               const juce::String& text) {
  k.slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
  k.slider.setColour(juce::Slider::rotarySliderFillColourId, kAccentBlue);
  k.slider.setColour(juce::Slider::thumbColourId, kAccentPurple);
  addAndMakeVisible(k.slider);

  k.label.setText(text, juce::dontSendNotification);
  k.label.setJustificationType(juce::Justification::centred);
  k.label.setColour(juce::Label::textColourId, kTextColour);
  k.label.setFont(juce::FontOptions(11.0f));
  addAndMakeVisible(k.label);

  k.attachment = std::make_unique<SliderAttach>(processorRef.getAPVTS(),
                                                paramId, k.slider);
}

void SoundsgoodAudioProcessorEditor::setupCombo(
    LabelledCombo& c, const juce::String& paramId, const juce::String& text,
    const juce::StringArray& choices) {
  for (int i = 0; i < choices.size(); ++i) c.combo.addItem(choices[i], i + 1);

  c.combo.setColour(juce::ComboBox::backgroundColourId, kTabActive);
  c.combo.setColour(juce::ComboBox::textColourId, kTextColour);
  addAndMakeVisible(c.combo);

  c.label.setText(text, juce::dontSendNotification);
  c.label.setJustificationType(juce::Justification::centred);
  c.label.setColour(juce::Label::textColourId, kTextColour);
  c.label.setFont(juce::FontOptions(11.0f));
  addAndMakeVisible(c.label);

  c.attachment =
      std::make_unique<ComboAttach>(processorRef.getAPVTS(), paramId, c.combo);
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::switchLFOTab(int tab) {
  activeLFOTab_ = tab;

  bool show1 = (tab == 0);
  lfoShapeCombo.combo.setVisible(show1);
  lfoShapeCombo.label.setVisible(show1);
  lfoRateKnob.slider.setVisible(show1);
  lfoRateKnob.label.setVisible(show1);
  lfoDepthKnob.slider.setVisible(show1);
  lfoDepthKnob.label.setVisible(show1);
  lfoDestCombo.combo.setVisible(show1);
  lfoDestCombo.label.setVisible(show1);

  bool show2 = (tab == 1);
  lfo2ShapeCombo.combo.setVisible(show2);
  lfo2ShapeCombo.label.setVisible(show2);
  lfo2RateKnob.slider.setVisible(show2);
  lfo2RateKnob.label.setVisible(show2);
  lfo2DepthKnob.slider.setVisible(show2);
  lfo2DepthKnob.label.setVisible(show2);
  lfo2DestCombo.combo.setVisible(show2);
  lfo2DestCombo.label.setVisible(show2);

  lfoTab1.setColour(juce::TextButton::buttonColourId,
                    show1 ? kTabActive : kTabInactive);
  lfoTab2.setColour(juce::TextButton::buttonColourId,
                    show2 ? kTabActive : kTabInactive);

  repaint();
}

void SoundsgoodAudioProcessorEditor::switchEnvTab(int tab) {
  activeEnvTab_ = tab;

  bool show1 = (tab == 0);
  attackKnob.slider.setVisible(show1);
  attackKnob.label.setVisible(show1);
  decayKnob.slider.setVisible(show1);
  decayKnob.label.setVisible(show1);
  sustainKnob.slider.setVisible(show1);
  sustainKnob.label.setVisible(show1);
  releaseKnob.slider.setVisible(show1);
  releaseKnob.label.setVisible(show1);

  bool show2 = (tab == 1);
  env2AttackKnob.slider.setVisible(show2);
  env2AttackKnob.label.setVisible(show2);
  env2DecayKnob.slider.setVisible(show2);
  env2DecayKnob.label.setVisible(show2);
  env2SustainKnob.slider.setVisible(show2);
  env2SustainKnob.label.setVisible(show2);
  env2ReleaseKnob.slider.setVisible(show2);
  env2ReleaseKnob.label.setVisible(show2);
  env2DestCombo.combo.setVisible(show2);
  env2DestCombo.label.setVisible(show2);
  env2AmountKnob.slider.setVisible(show2);
  env2AmountKnob.label.setVisible(show2);

  envTab1.setColour(juce::TextButton::buttonColourId,
                    show1 ? kTabActive : kTabInactive);
  envTab2.setColour(juce::TextButton::buttonColourId,
                    show2 ? kTabActive : kTabInactive);

  repaint();
}

//==============================================================================
namespace {
void placeKnob(juce::Slider& slider, juce::Label& label, int x, int y) {
  label.setBounds(x, y - kLabelH, kKnobW, kLabelH);
  slider.setBounds(x, y, kKnobW, kKnobH);
}

void placeCombo(juce::ComboBox& combo, juce::Label& label, int x, int y) {
  label.setBounds(x, y - kLabelH, kComboW, kLabelH);
  combo.setBounds(x, y + 10, kComboW, kComboH);
}
}  // namespace

//==============================================================================
void SoundsgoodAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(kBgColour);

  auto drawSectionBg = [&](juce::Rectangle<int> area,
                           const juce::String& title) {
    g.setColour(kSectionBg);
    g.fillRoundedRectangle(area.toFloat(), 6.0f);
    g.setColour(kSectionBorder);
    g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);
    g.setColour(kAccentBlue);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(title, area.getX() + 6, area.getY() + 2, area.getWidth() - 12,
               kHeaderH, juce::Justification::centredLeft);
  };

  // Same helper but title text is shorter (tabs will sit in the header)
  auto drawSectionBgShort = [&](juce::Rectangle<int> area,
                                const juce::String& title) {
    g.setColour(kSectionBg);
    g.fillRoundedRectangle(area.toFloat(), 6.0f);
    g.setColour(kSectionBorder);
    g.drawRoundedRectangle(area.toFloat(), 6.0f, 1.0f);
    g.setColour(kAccentBlue);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText(title, area.getX() + 6, area.getY() + 2, 50, kHeaderH,
               juce::Justification::centredLeft);
  };

  int sectionH = kRowH + kPad;  // 108
  int totalW = getWidth();

  // ========== ROW 1: WPPM ==========
  int y1 = kPad;
  int wppmW = totalW - 2 * kPad;
  drawSectionBg({kPad, y1, wppmW, sectionH}, "Wave Pulse Phase Modulation");

  // ========== ROW 2: Sub + Unison + Filter ==========
  int y2 = y1 + sectionH + kPad;

  int subW = kComboStep + kKnobStep + kSectionPad + kPad;
  drawSectionBg({kPad, y2, subW, sectionH}, "Sub");

  int uniX = kPad + subW + kSectionPad;
  int uniW = 3 * kKnobStep + kSectionPad + kPad;
  drawSectionBg({uniX, y2, uniW, sectionH}, "Unison");

  int filtX = uniX + uniW + kSectionPad;
  int filtW = totalW - filtX - kPad;
  drawSectionBg({filtX, y2, filtW, sectionH}, "Filter");

  // ========== ROW 3: LFO [tabs] + Mod Matrix + Envelope [tabs] ==========
  int y3 = y2 + sectionH + kPad;

  int lfoW = kComboStep + 2 * kKnobStep + kComboStep + kSectionPad + kPad;
  drawSectionBgShort({kPad, y3, lfoW, sectionH}, "LFO");

  int modX = kPad + lfoW + kSectionPad;
  int modW = 4 * kKnobStep + kSectionPad + kPad;
  drawSectionBg({modX, y3, modW, sectionH}, "Mod Matrix");

  int envX = modX + modW + kSectionPad;
  int envW = totalW - envX - kPad;
  drawSectionBgShort({envX, y3, envW, sectionH}, "Env");

  // ========== ROW 4: Analog + Quality + Output ==========
  int y4 = y3 + sectionH + kPad;

  int anaW = 2 * kKnobStep + kSectionPad + kPad;
  drawSectionBg({kPad, y4, anaW, sectionH}, "Analog");

  int qualX = kPad + anaW + kSectionPad;
  int qualW = kComboStep + kSectionPad + kPad;
  drawSectionBg({qualX, y4, qualW, sectionH}, "Quality");

  int outX = qualX + qualW + kSectionPad;
  int outW = totalW - outX - kPad;
  drawSectionBg({outX, y4, outW, sectionH}, "Output");

  // ========== Visualizers ==========
  if (!wppmVizBounds_.isEmpty()) {
    g.setColour(kDarkBg);
    g.fillRoundedRectangle(wppmVizBounds_.toFloat(), 4.0f);
    drawWPPMWaveform(g, wppmVizBounds_.reduced(3));
  }

  if (!envelopeCurveBounds_.isEmpty()) {
    g.setColour(kDarkBg);
    g.fillRoundedRectangle(envelopeCurveBounds_.toFloat(), 4.0f);

    auto& apvts = processorRef.getAPVTS();
    if (activeEnvTab_ == 0) {
      drawEnvelopeCurve(g, envelopeCurveBounds_.reduced(3),
                        *apvts.getRawParameterValue("attack"),
                        *apvts.getRawParameterValue("decay"),
                        *apvts.getRawParameterValue("sustain"),
                        *apvts.getRawParameterValue("release"));
    } else {
      drawEnvelopeCurve(g, envelopeCurveBounds_.reduced(3),
                        *apvts.getRawParameterValue("env2Attack"),
                        *apvts.getRawParameterValue("env2Decay"),
                        *apvts.getRawParameterValue("env2Sustain"),
                        *apvts.getRawParameterValue("env2Release"));
    }
  }

  if (!scopeBounds_.isEmpty()) {
    g.setColour(kDarkBg);
    g.fillRoundedRectangle(scopeBounds_.toFloat(), 4.0f);
    drawScope(g, scopeBounds_.reduced(3));
  }
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::resized() {
  int sectionH = kRowH + kPad;
  int totalW = getWidth();

  // ====== ROW 1: WPPM ======
  int y = kPad + kHeaderH + kLabelH;
  int x = kPad + kPad;

  // Waveform viz
  wppmVizBounds_ = {x, kPad + kHeaderH, 96, kKnobH + kLabelH};
  x += 96 + kSectionPad;

  // Split (d)
  placeKnob(splitKnob.slider, splitKnob.label, x, y);
  x += kKnobStep;

  // Pulse 1
  placeKnob(ratio1Knob.slider, ratio1Knob.label, x, y);
  x += kKnobStep;
  placeKnob(depth1Knob.slider, depth1Knob.label, x, y);
  x += kKnobStep;
  placeKnob(phase1Knob.slider, phase1Knob.label, x, y);
  x += kKnobStep;

  // Link button
  linkButton.setBounds(x, y + kKnobH / 3, 62, 22);
  x += 62 + kPad;

  // Pulse 2
  placeKnob(ratio2Knob.slider, ratio2Knob.label, x, y);
  x += kKnobStep;
  placeKnob(depth2Knob.slider, depth2Knob.label, x, y);
  x += kKnobStep;
  placeKnob(phase2Knob.slider, phase2Knob.label, x, y);

  // ====== ROW 2: Sub + Unison + Filter ======
  y = kPad + sectionH + kPad + kHeaderH + kLabelH;
  x = kPad + kPad;

  // Sub
  placeCombo(subOctaveCombo.combo, subOctaveCombo.label, x, y);
  x += kComboStep;
  placeKnob(subMixKnob.slider, subMixKnob.label, x, y);

  // Unison
  int subW = kComboStep + kKnobStep + kSectionPad + kPad;
  int uniX = kPad + subW + kSectionPad;
  x = uniX + kPad;
  placeKnob(unisonVoicesKnob.slider, unisonVoicesKnob.label, x, y);
  x += kKnobStep;
  placeKnob(unisonDetuneKnob.slider, unisonDetuneKnob.label, x, y);
  x += kKnobStep;
  placeKnob(unisonSpreadKnob.slider, unisonSpreadKnob.label, x, y);

  // Filter
  int uniW = 3 * kKnobStep + kSectionPad + kPad;
  int filtX = uniX + uniW + kSectionPad;
  x = filtX + kPad;
  placeCombo(filterModeCombo.combo, filterModeCombo.label, x, y);
  x += kComboStep;
  placeKnob(filterCutoffKnob.slider, filterCutoffKnob.label, x, y);
  x += kKnobStep;
  placeKnob(filterResonanceKnob.slider, filterResonanceKnob.label, x, y);
  x += kKnobStep;
  placeKnob(filterKeyTrackKnob.slider, filterKeyTrackKnob.label, x, y);
  x += kKnobStep;
  placeKnob(filterEnvAmountKnob.slider, filterEnvAmountKnob.label, x, y);

  // ====== ROW 3: LFO + Mod Matrix + Envelope ======
  y = kPad + 2 * (sectionH + kPad) + kHeaderH + kLabelH;
  x = kPad + kPad;

  // LFO tab buttons (in header)
  int lfoHeaderY = kPad + 2 * (sectionH + kPad) + 1;
  lfoTab1.setBounds(kPad + 32, lfoHeaderY, 24, kHeaderH - 2);
  lfoTab2.setBounds(kPad + 58, lfoHeaderY, 24, kHeaderH - 2);

  // LFO1 controls (same position as LFO2)
  auto placeLFO = [&](LabelledCombo& shape, LabelledKnob& rate,
                      LabelledKnob& depth, LabelledCombo& dest) {
    int lx = kPad + kPad;
    placeCombo(shape.combo, shape.label, lx, y);
    lx += kComboStep;
    placeKnob(rate.slider, rate.label, lx, y);
    lx += kKnobStep;
    placeKnob(depth.slider, depth.label, lx, y);
    lx += kKnobStep;
    placeCombo(dest.combo, dest.label, lx, y);
  };
  placeLFO(lfoShapeCombo, lfoRateKnob, lfoDepthKnob, lfoDestCombo);
  placeLFO(lfo2ShapeCombo, lfo2RateKnob, lfo2DepthKnob, lfo2DestCombo);

  // Mod Matrix
  int lfoW = kComboStep + 2 * kKnobStep + kComboStep + kSectionPad + kPad;
  int modX = kPad + lfoW + kSectionPad;
  x = modX + kPad;
  placeKnob(velToDKnob.slider, velToDKnob.label, x, y);
  x += kKnobStep;
  placeKnob(velToDepthKnob.slider, velToDepthKnob.label, x, y);
  x += kKnobStep;
  placeKnob(atToDKnob.slider, atToDKnob.label, x, y);
  x += kKnobStep;
  placeKnob(atToDepthKnob.slider, atToDepthKnob.label, x, y);

  // Envelope section
  int modW = 4 * kKnobStep + kSectionPad + kPad;
  int envX = modX + modW + kSectionPad;

  // Envelope tab buttons (in header)
  int envHeaderY = kPad + 2 * (sectionH + kPad) + 1;
  envTab1.setBounds(envX + 32, envHeaderY, 34, kHeaderH - 2);
  envTab2.setBounds(envX + 68, envHeaderY, 34, kHeaderH - 2);

  // Envelope controls (both tabs share same positions for ADSR knobs)
  x = envX + kPad;
  auto placeADSR = [&](LabelledKnob& a, LabelledKnob& d, LabelledKnob& s,
                       LabelledKnob& r) {
    int ex = envX + kPad;
    placeKnob(a.slider, a.label, ex, y);
    ex += kKnobStep;
    placeKnob(d.slider, d.label, ex, y);
    ex += kKnobStep;
    placeKnob(s.slider, s.label, ex, y);
    ex += kKnobStep;
    placeKnob(r.slider, r.label, ex, y);
  };
  placeADSR(attackKnob, decayKnob, sustainKnob, releaseKnob);
  placeADSR(env2AttackKnob, env2DecayKnob, env2SustainKnob, env2ReleaseKnob);

  x = envX + kPad + 4 * kKnobStep;

  // ENV2 extra controls (dest combo + amount knob) — placed after ADSR
  placeCombo(env2DestCombo.combo, env2DestCombo.label, x, y);
  x += kComboStep;
  placeKnob(env2AmountKnob.slider, env2AmountKnob.label, x, y);
  x += kKnobStep;

  // Envelope curve visualizer
  int envVizTop = kPad + 2 * (sectionH + kPad) + kHeaderH;
  envelopeCurveBounds_ = {x, envVizTop, totalW - x - kPad - kSectionPad,
                          kKnobH + kLabelH};

  // ====== ROW 4: Analog + Quality + Output ======
  y = kPad + 3 * (sectionH + kPad) + kHeaderH + kLabelH;
  x = kPad + kPad;

  // Analog
  placeKnob(analogDriveKnob.slider, analogDriveKnob.label, x, y);
  x += kKnobStep;
  placeKnob(analogMixKnob.slider, analogMixKnob.label, x, y);

  // Quality
  int anaW = 2 * kKnobStep + kSectionPad + kPad;
  int qualX = kPad + anaW + kSectionPad;
  x = qualX + kPad;
  placeCombo(qualityCombo.combo, qualityCombo.label, x, y);

  // Output
  int qualW = kComboStep + kSectionPad + kPad;
  int outX = qualX + qualW + kSectionPad;
  x = outX + kPad;
  placeKnob(volumeKnob.slider, volumeKnob.label, x, y);
  x += kKnobStep + kPad;

  // Scope
  int scopeTop = kPad + 3 * (sectionH + kPad) + kHeaderH;
  int scopeH = kKnobH + kLabelH;
  scopeBounds_ = {x, scopeTop, totalW - x - kPad - kSectionPad, scopeH};
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::timerCallback() {
  // Scope
  int wp = processorRef.getScopeWritePos();
  for (int i = 0; i < SoundsgoodAudioProcessor::kScopeSize; ++i) {
    int idx = (wp + i) % SoundsgoodAudioProcessor::kScopeSize;
    scopeData_[static_cast<size_t>(i)] = processorRef.getScopeBuffer()[idx];
  }

  updateWPPMWaveform();

  // Dim P2 knobs when linked
  bool linked = *processorRef.getAPVTS().getRawParameterValue("link") >= 0.5f;
  float alpha = linked ? 0.35f : 1.0f;
  for (auto* k : {&ratio2Knob, &depth2Knob, &phase2Knob}) {
    k->slider.setEnabled(!linked);
    k->slider.setAlpha(alpha);
    k->label.setAlpha(alpha);
  }

  repaint();
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::updateWPPMWaveform() {
  auto& apvts = processorRef.getAPVTS();
  float d = *apvts.getRawParameterValue("d");
  float R1 = *apvts.getRawParameterValue("R1");
  float I1 = *apvts.getRawParameterValue("I1");
  float P1 = *apvts.getRawParameterValue("P1");
  float R2 = *apvts.getRawParameterValue("R2");
  float I2 = *apvts.getRawParameterValue("I2");
  float P2 = *apvts.getRawParameterValue("P2");

  bool linked = *apvts.getRawParameterValue("link") >= 0.5f;
  if (linked) {
    R2 = R1;
    I2 = I1;
    P2 = P1;
  }

  d = std::clamp(d, 0.01f, 0.99f);

  auto dWindow = [](float dv) {
    return std::sin(kPi * std::clamp(dv, 0.0f, 1.0f));
  };

  for (int i = 0; i < kWPPMVizSize; ++i) {
    float phi = static_cast<float>(i) / static_cast<float>(kWPPMVizSize);
    float F;
    if (phi < d) {
      float t1 = phi / d;
      float w1 = std::sin(kPi * t1);
      float g1 = w1 * I1 * std::sin(kTwoPi * (R1 * t1 + P1));
      g1 *= dWindow(d);
      F = phi / (2.0f * d) + g1;
    } else {
      float oneMinusD = 1.0f - d;
      float t2 = (phi - d) / oneMinusD;
      float w2 = std::sin(kPi * t2);
      float g2 = w2 * I2 * std::sin(kTwoPi * (R2 * t2 + P2));
      g2 *= dWindow(oneMinusD);
      F = 0.5f + (phi - d) / (2.0f * oneMinusD) + g2;
    }
    wppmWaveform_[static_cast<size_t>(i)] = -std::cos(kTwoPi * F);
  }
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::drawWPPMWaveform(
    juce::Graphics& g, juce::Rectangle<int> area) {
  g.setColour(juce::Colour(0xfff9e2af));  // yellow

  juce::Path path;
  float w = static_cast<float>(area.getWidth());
  float h = static_cast<float>(area.getHeight());
  float midY = static_cast<float>(area.getY()) + h * 0.5f;

  for (int i = 0; i < kWPPMVizSize; ++i) {
    float xP = static_cast<float>(area.getX()) +
               (static_cast<float>(i) / static_cast<float>(kWPPMVizSize)) * w;
    float yP = midY - wppmWaveform_[static_cast<size_t>(i)] * (h * 0.45f);
    if (i == 0) {
      path.startNewSubPath(xP, yP);
    } else {
      path.lineTo(xP, yP);
    }
  }
  g.strokePath(path, juce::PathStrokeType(1.5f));

  // Split indicator
  float d = *processorRef.getAPVTS().getRawParameterValue("d");
  float splitX = static_cast<float>(area.getX()) + d * w;
  g.setColour(juce::Colour(0x60f38ba8));
  g.drawVerticalLine(static_cast<int>(splitX), static_cast<float>(area.getY()),
                     static_cast<float>(area.getBottom()));
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::drawEnvelopeCurve(
    juce::Graphics& g, juce::Rectangle<int> area, float a, float d, float s,
    float r) {
  float sustainTime = 0.8f;
  float totalFrac = a + d + sustainTime + r;
  if (totalFrac < 0.001f) totalFrac = 1.0f;

  float aF = a / totalFrac;
  float dF = d / totalFrac;
  float sF = sustainTime / totalFrac;
  float rF = r / totalFrac;

  float w = static_cast<float>(area.getWidth());
  float h = static_cast<float>(area.getHeight());
  float x0 = static_cast<float>(area.getX());
  float bottom = static_cast<float>(area.getY()) + h;

  g.setColour(kAccentBlue);

  juce::Path envPath;
  float cx = x0;
  envPath.startNewSubPath(cx, bottom);

  // Attack
  float aEnd = cx + aF * w;
  int aSteps = std::max(2, static_cast<int>(aF * w));
  for (int i = 1; i <= aSteps; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(aSteps);
    float env = 1.0f - std::exp(-3.0f * t);
    env = std::min(env / (1.0f - std::exp(-3.0f)), 1.0f);
    envPath.lineTo(cx + t * (aEnd - cx), bottom - env * h);
  }
  cx = aEnd;

  // Decay
  float dEnd = cx + dF * w;
  int dSteps = std::max(2, static_cast<int>(dF * w));
  for (int i = 1; i <= dSteps; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(dSteps);
    float env = 1.0f - (1.0f - s) * (1.0f - std::exp(-3.0f * t)) /
                           (1.0f - std::exp(-3.0f));
    envPath.lineTo(cx + t * (dEnd - cx), bottom - env * h);
  }
  cx = dEnd;

  // Sustain
  float sEnd = cx + sF * w;
  envPath.lineTo(sEnd, bottom - s * h);
  cx = sEnd;

  // Release
  float rEnd = cx + rF * w;
  int rSteps = std::max(2, static_cast<int>(rF * w));
  for (int i = 1; i <= rSteps; ++i) {
    float t = static_cast<float>(i) / static_cast<float>(rSteps);
    float env =
        s * (1.0f - (1.0f - std::exp(-3.0f * t)) / (1.0f - std::exp(-3.0f)));
    envPath.lineTo(cx + t * (rEnd - cx), bottom - env * h);
  }

  g.strokePath(envPath, juce::PathStrokeType(1.5f));

  // Fill
  juce::Path fillPath(envPath);
  fillPath.lineTo(x0 + w, bottom);
  fillPath.lineTo(x0, bottom);
  fillPath.closeSubPath();
  g.setColour(juce::Colour(0x2089b4fa));
  g.fillPath(fillPath);
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::drawScope(juce::Graphics& g,
                                               juce::Rectangle<int> area) {
  g.setColour(juce::Colour(0xffa6e3a1));

  juce::Path path;
  float w = static_cast<float>(area.getWidth());
  float h = static_cast<float>(area.getHeight());
  float midY = static_cast<float>(area.getY()) + h * 0.5f;

  for (int i = 0; i < SoundsgoodAudioProcessor::kScopeSize; ++i) {
    float xP = static_cast<float>(area.getX()) +
               (static_cast<float>(i) /
                static_cast<float>(SoundsgoodAudioProcessor::kScopeSize)) *
                   w;
    float yP =
        midY - juce::jlimit(-1.0f, 1.0f, scopeData_[static_cast<size_t>(i)]) *
                   (h * 0.45f);
    if (i == 0) {
      path.startNewSubPath(xP, yP);
    } else {
      path.lineTo(xP, yP);
    }
  }
  g.strokePath(path, juce::PathStrokeType(1.5f));
}
