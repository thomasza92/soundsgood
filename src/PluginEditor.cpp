/*
  ==============================================================================
    PluginEditor.cpp
    Soundsgood — CS-30 full front panel emulation UI.
    Data-driven two-row layout with 75 controls across 16 sections.
  ==============================================================================
*/

#include "PluginEditor.h"

#include "PluginProcessor.h"

//==============================================================================
// WaveformDisplay implementation
//==============================================================================
void WaveformDisplay::updateFromProcessor(
    const SoundsgoodAudioProcessor& proc) {
  constexpr int kBufSize = SoundsgoodAudioProcessor::kScopeSize;
  const float* buf = proc.getScopeData();
  int wp = proc.getScopeWritePos();

  // Linearize ring buffer (oldest sample first)
  float linear[kBufSize];
  for (int i = 0; i < kBufSize; ++i) {
    linear[i] = buf[(wp + i) % kBufSize];
  }

  // Zero-crossing trigger for stable display
  int triggerPoint = 0;
  int searchEnd = kBufSize - kDisplaySamples;
  for (int i = 1; i < searchEnd; ++i) {
    if (linear[i - 1] <= 0.0f && linear[i] > 0.0f) {
      triggerPoint = i;
      break;
    }
  }

  for (int i = 0; i < kDisplaySamples; ++i) {
    displayBuffer_[i] = linear[triggerPoint + i];
  }

  repaint();
}

void WaveformDisplay::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background
  g.setColour(juce::Colour(0xff101020));
  g.fillRoundedRectangle(bounds, 4.0f);

  // Border
  g.setColour(juce::Colour(0xff555577));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.5f);

  // "SCOPE" label
  g.setColour(juce::Colour(0xff666688));
  g.setFont(juce::FontOptions(9.0f));
  g.drawText("SCOPE", bounds.getX() + 6, bounds.getY() + 2, 40, 12,
             juce::Justification::centredLeft);

  // Center zero line
  float centerY = bounds.getCentreY();
  g.setColour(juce::Colour(0xff333355));
  g.drawHorizontalLine(static_cast<int>(centerY), bounds.getX() + 4,
                       bounds.getRight() - 4);

  if (bounds.getWidth() < 8.0f || bounds.getHeight() < 8.0f) {
    return;
  }

  // Build waveform path
  constexpr float kInset = 4.0f;
  float w = bounds.getWidth() - kInset * 2;
  float h = bounds.getHeight() - kInset * 2;

  juce::Path path;
  for (int i = 0; i < kDisplaySamples; ++i) {
    float x = bounds.getX() + kInset +
              (static_cast<float>(i) / (kDisplaySamples - 1)) * w;
    float sample = juce::jlimit(-1.0f, 1.0f, displayBuffer_[i]);
    float y = centerY - sample * (h * 0.5f);

    if (i == 0) {
      path.startNewSubPath(x, y);
    } else {
      path.lineTo(x, y);
    }
  }

  // Glow
  g.setColour(juce::Colour(0x40e0a030));
  g.strokePath(path, juce::PathStrokeType(3.0f));

  // Main waveform line
  g.setColour(juce::Colour(0xffe0a030));
  g.strokePath(path, juce::PathStrokeType(1.5f));
}

//==============================================================================
// EGCurveDisplay implementation
//==============================================================================
void EGCurveDisplay::setParameters(float attack, float decay, float sustain,
                                   float release) {
  if (attack_ != attack || decay_ != decay || sustain_ != sustain ||
      release_ != release) {
    attack_ = attack;
    decay_ = decay;
    sustain_ = sustain;
    release_ = release;
    repaint();
  }
}

void EGCurveDisplay::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat();

  // Background
  g.setColour(juce::Colour(0xff101820));
  g.fillRoundedRectangle(bounds, 4.0f);

  // Border
  g.setColour(juce::Colour(0xff446644));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.5f);

  constexpr float kInset = 6.0f;
  float w = bounds.getWidth() - kInset * 2;
  float h = bounds.getHeight() - kInset * 2;
  float baseX = bounds.getX() + kInset;
  float baseY = bounds.getY() + kInset;

  if (w < 4.0f || h < 4.0f) {
    return;
  }

  // Compute time segments: A, D, sustain hold (fixed), R
  // Normalize times so the total fits the display width
  float totalTime = attack_ + decay_ + 0.3f + release_;  // 0.3s sustain hold
  if (totalTime < 0.001f) {
    totalTime = 0.001f;
  }
  float aFrac = attack_ / totalTime;
  float dFrac = decay_ / totalTime;
  float sFrac = 0.3f / totalTime;
  // rFrac is the remainder

  // Key x positions
  float xA = baseX + aFrac * w;
  float xD = xA + dFrac * w;
  float xS = xD + sFrac * w;
  float xR = baseX + w;

  // Key y positions (top = 1.0, bottom = 0.0)
  float yBottom = baseY + h;
  float yTop = baseY;
  float ySus = baseY + (1.0f - sustain_) * h;

  // Build ADSR path with exponential curves
  juce::Path path;
  path.startNewSubPath(baseX, yBottom);

  // Attack: exponential charge curve (concave up) from 0 to 1
  constexpr int kSegments = 20;
  for (int i = 1; i <= kSegments; ++i) {
    float t = static_cast<float>(i) / kSegments;
    // RC charge curve: 1 - e^(-3t) (reaches ~95% at t=1)
    float level = 1.0f - std::exp(-3.0f * t);
    level /= (1.0f - std::exp(-3.0f));  // normalize to exactly 1.0 at t=1
    float x = baseX + t * (xA - baseX);
    float y = yBottom - level * h;
    path.lineTo(x, y);
  }

  // Decay: exponential fall from 1 to sustain
  for (int i = 1; i <= kSegments; ++i) {
    float t = static_cast<float>(i) / kSegments;
    float level = sustain_ + (1.0f - sustain_) * std::exp(-3.0f * t);
    float x = xA + t * (xD - xA);
    float y = baseY + (1.0f - level) * h;
    path.lineTo(x, y);
  }

  // Sustain hold (flat)
  path.lineTo(xS, ySus);

  // Release: exponential fall from sustain to 0
  for (int i = 1; i <= kSegments; ++i) {
    float t = static_cast<float>(i) / kSegments;
    float level = sustain_ * std::exp(-3.0f * t);
    float x = xS + t * (xR - xS);
    float y = baseY + (1.0f - level) * h;
    path.lineTo(x, y);
  }

  // Ensure we end at the bottom
  path.lineTo(xR, yBottom);

  // Filled area (subtle)
  juce::Path fillPath(path);
  fillPath.lineTo(baseX, yBottom);
  fillPath.closeSubPath();
  g.setColour(juce::Colour(0x1880e060));
  g.fillPath(fillPath);

  // Glow
  g.setColour(juce::Colour(0x4080e060));
  g.strokePath(path, juce::PathStrokeType(2.5f));

  // Main curve line
  g.setColour(juce::Colour(0xff80e060));
  g.strokePath(path, juce::PathStrokeType(1.5f));

  // Sustain level indicator (dashed horizontal)
  g.setColour(juce::Colour(0x5580e060));
  float dashLengths[] = {3.0f, 3.0f};
  juce::Path susLine;
  susLine.startNewSubPath(baseX, ySus);
  susLine.lineTo(baseX + w, ySus);
  g.strokePath(susLine, juce::PathStrokeType(0.75f), juce::AffineTransform());
}

// ── Layout constants ──
namespace Layout {
constexpr int kTitleH = 44;
constexpr int kSectionHeaderH = 22;
constexpr int kCellW = 50;
constexpr int kComboCellW = 70;  // wider spacing for combo/toggle row
constexpr int kCellH = 76;
constexpr int kKnobSz = 44;
constexpr int kComboW = 66;
constexpr int kComboH = 22;
constexpr int kToggleW = 66;
constexpr int kToggleH = 24;
constexpr int kLabelH = 14;
constexpr int kMarginX = 6;
constexpr int kTopRowY = kTitleH;
constexpr int kTopRowH = kSectionHeaderH + 3 * kCellH + 20;
constexpr int kBotRowY = kTopRowY + kTopRowH;
constexpr int kBotRowH = kSectionHeaderH + 2 * kCellH + 20;
constexpr int kWindowW = 1600;
constexpr int kWindowH = kBotRowY + kBotRowH;
}  // namespace Layout

//==============================================================================
SoundsgoodAudioProcessorEditor::SoundsgoodAudioProcessorEditor(
    SoundsgoodAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  // ── Define 16 sections ──
  // Top row: 9 sections
  sections_ = {
      // sec 0-8: Top row (panelRow=0)
      {"PITCH", juce::Colour(0xff2e4a6a), 0, 0, 100},
      {"VCO 1", juce::Colour(0xff1e3a5f), 0, 100, 210},
      {"VCO 2", juce::Colour(0xff1e3a5f), 0, 310, 210},
      {"VCF 1", juce::Colour(0xff5f3a1e), 0, 520, 160},
      {"VCF 2", juce::Colour(0xff5f3a1e), 0, 680, 160},
      {"VCA 1", juce::Colour(0xff1e5f3a), 0, 840, 160},
      {"VCA 2", juce::Colour(0xff1e5f3a), 0, 1000, 150},
      {"TRIGGER", juce::Colour(0xff5f1e3a), 0, 1150, 150},
      {"SEQUENCER", juce::Colour(0xff4a2e6a), 0, 1300, 300},
      // sec 9-15: Bottom row (panelRow=1)
      {"EG 1", juce::Colour(0xff3a5f1e), 1, 0, 250},
      {"EG 2", juce::Colour(0xff3a5f1e), 1, 250, 250},
      {"EG 3", juce::Colour(0xff3a5f1e), 1, 500, 250},
      {"LFO", juce::Colour(0xff6a4a2e), 1, 750, 230},
      {"PORTAMENTO", juce::Colour(0xff3a3a5f), 1, 980, 140},
      {"PITCH BEND", juce::Colour(0xff3a3a5f), 1, 1120, 200},
      {"OUTPUT", juce::Colour(0xff5f4a1e), 1, 1320, 280},
  };

  // ── Create all controls ──
  // Layout rule: knobs in top rows, combos/toggles in bottom row of section.
  // Top-row sections: rows 0-1 = knobs, row 2 = combos/toggles
  // Bottom-row sections: row 0 = knobs, row 1 = combos/toggles

  // sec 0: PITCH (4)
  addKnob("pitchTune", "TUNE", 0, 0, 0);
  addKnob("pitchDetune", "DETUNE", 0, 1, 0);
  addKnob("pitchEGDepth", "EG DEPTH", 0, 0, 1);
  addCombo("pitchEGSel", "EG SEL", 0, 0, 2);

  // sec 1: VCO 1 (8)
  addKnob("vco1PW", "PW", 1, 0, 0);
  addKnob("vco1Depth", "DEPTH", 1, 1, 0);
  addKnob("vco1ModVCO2", "FM VCO2", 1, 2, 0);
  addKnob("vco1PWM", "PWM", 1, 0, 1);
  addToggle("vco1Src", "SEQ", 1, 1, 1);
  addCombo("vco1Feet", "FEET", 1, 0, 2);
  addCombo("vco1Func", "FUNC", 1, 1, 2);
  addCombo("vco1EGSel", "EG", 1, 2, 2);

  // sec 2: VCO 2 (7)
  addKnob("vco2PW", "PW", 2, 0, 0);
  addKnob("vco2Depth", "DEPTH", 2, 1, 0);
  addKnob("vco2PWM", "PWM", 2, 2, 0);
  addToggle("vco2Src", "SEQ", 2, 0, 1);
  addCombo("vco2Feet", "FEET", 2, 0, 2);
  addCombo("vco2Func", "FUNC", 2, 1, 2);
  addCombo("vco2EGSel", "EG", 2, 2, 2);

  // sec 3: VCF 1 (8)
  addKnob("vcf1Level", "LEVEL", 3, 0, 0);
  addKnob("vcf1Cutoff", "CUTOFF", 3, 1, 0);
  addKnob("vcf1KBDFollow", "KBD FLW", 3, 2, 0);
  addKnob("vcf1Reso", "RESO", 3, 0, 1);
  addKnob("vcf1ModDepth", "MOD DPT", 3, 1, 1);
  addKnob("vcf1EGDepth", "EG DPT", 3, 2, 1);
  addCombo("vcf1ModFunc", "MOD FN", 3, 0, 2);
  addCombo("vcf1EGSel", "EG SEL", 3, 1, 2);

  // sec 4: VCF 2 (8)
  addKnob("vcf2Level", "LEVEL", 4, 0, 0);
  addKnob("vcf2Cutoff", "CUTOFF", 4, 1, 0);
  addKnob("vcf2KBDFollow", "KBD FLW", 4, 2, 0);
  addKnob("vcf2Reso", "RESO", 4, 0, 1);
  addKnob("vcf2ModDepth", "MOD DPT", 4, 1, 1);
  addKnob("vcf2EGDepth", "EG DPT", 4, 2, 1);
  addCombo("vcf2ModFunc", "MOD FN", 4, 0, 2);
  addCombo("vcf2EGSel", "EG SEL", 4, 1, 2);

  // sec 5: VCA 1 (6)
  addKnob("vca1VCF1", "VCF1", 5, 0, 0);
  addKnob("vca1VCF2", "VCF2", 5, 1, 0);
  addKnob("vca1ModDepth", "MOD DPT", 5, 2, 0);
  addKnob("vca1VCO1", "VCO1 DIR", 5, 0, 1);
  addCombo("vca1FilterType", "FLT TYPE", 5, 0, 2);
  addCombo("vca1ModFunc", "MOD FN", 5, 1, 2);

  // sec 6: VCA 2 (4)
  addKnob("vca2NormMod", "NORM/MOD", 6, 0, 0);
  addKnob("vca2ModDepth", "MOD DPT", 6, 1, 0);
  addCombo("vca2RMOSrc", "RMO SRC", 6, 0, 2);
  addCombo("vca2ModFunc", "MOD FN", 6, 1, 2);

  // sec 7: TRIGGER (2)
  addToggle("trigMode", "LFO", 7, 0, 0);
  addToggle("trigType", "MULTI", 7, 1, 0);

  // sec 8: SEQUENCER (11)
  addKnob("seqClock", "CLOCK", 8, 0, 0);
  addToggle("seqRunning", "RUN", 8, 1, 0);
  addToggle("seqHold", "HOLD", 8, 2, 0);
  addCombo("seqSteps", "STEPS", 8, 3, 0);
  addKnob("seqKnob1", "1", 8, 0, 1);
  stepKnobComps_[0] = sliders_.getLast();
  addKnob("seqKnob2", "2", 8, 1, 1);
  stepKnobComps_[1] = sliders_.getLast();
  addKnob("seqKnob3", "3", 8, 2, 1);
  stepKnobComps_[2] = sliders_.getLast();
  addKnob("seqKnob4", "4", 8, 3, 1);
  stepKnobComps_[3] = sliders_.getLast();
  addKnob("seqKnob5", "5", 8, 0, 2);
  stepKnobComps_[4] = sliders_.getLast();
  addKnob("seqKnob6", "6", 8, 1, 2);
  stepKnobComps_[5] = sliders_.getLast();
  addKnob("seqKnob7", "7", 8, 2, 2);
  stepKnobComps_[6] = sliders_.getLast();
  addKnob("seqKnob8", "8", 8, 3, 2);
  stepKnobComps_[7] = sliders_.getLast();

  // sec 9: EG 1 (4)
  addKnob("eg1Atk", "ATTACK", 9, 0, 0);
  addKnob("eg1Dec", "DECAY", 9, 1, 0);
  addKnob("eg1Sus", "SUSTAIN", 9, 2, 0);
  addKnob("eg1Rel", "RELEASE", 9, 3, 0);

  // sec 10: EG 2 (4)
  addKnob("eg2Atk", "ATTACK", 10, 0, 0);
  addKnob("eg2Dec", "DECAY", 10, 1, 0);
  addKnob("eg2Sus", "SUSTAIN", 10, 2, 0);
  addKnob("eg2Rel", "RELEASE", 10, 3, 0);

  // sec 11: EG 3 (4)
  addKnob("eg3Atk", "ATTACK", 11, 0, 0);
  addKnob("eg3Dec", "DECAY", 11, 1, 0);
  addKnob("eg3Sus", "SUSTAIN", 11, 2, 0);
  addKnob("eg3Rel", "RELEASE", 11, 3, 0);

  // sec 12: LFO (3)
  addKnob("lfoEGDepth", "EG DPT", 12, 0, 0);
  addKnob("lfoSpeed", "SPEED", 12, 1, 0);
  addCombo("lfoEGSel", "EG SEL", 12, 0, 1);

  // sec 13: PORTAMENTO (1)
  addKnob("portamento", "GLIDE", 13, 0, 0);

  // sec 14: PITCH BEND (2)
  addKnob("pbLimiter", "LIMITER", 14, 0, 0);
  addKnob("pbRange", "RANGE", 14, 1, 0);

  // sec 15: OUTPUT (3)
  addKnob("outBalance", "BALANCE", 15, 0, 0);
  addKnob("outVolume", "VOLUME", 15, 1, 0);
  addToggle("outMono", "MONO", 15, 2, 0);

  addAndMakeVisible(scopeDisplay_);

  // EG curve displays
  addAndMakeVisible(egCurve1_);
  addAndMakeVisible(egCurve2_);
  addAndMakeVisible(egCurve3_);

  setSize(Layout::kWindowW, Layout::kWindowH);

  // Start timer for sequencer LED animation (~30 fps)
  startTimerHz(30);
}

SoundsgoodAudioProcessorEditor::~SoundsgoodAudioProcessorEditor() {
  stopTimer();
  // Detach attachments before components are destroyed
  sliderAtts_.clear();
  comboAtts_.clear();
  buttonAtts_.clear();
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::addKnob(const juce::String& paramId,
                                             const juce::String& label,
                                             int section, int col, int row) {
  auto* knob = new juce::Slider();
  knob->setSliderStyle(juce::Slider::RotaryVerticalDrag);
  knob->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
  knob->setColour(juce::Slider::rotarySliderFillColourId,
                  juce::Colour(0xffe0a030));
  knob->setColour(juce::Slider::thumbColourId, juce::Colour(0xffd0d0d0));
  addAndMakeVisible(knob);

  auto* att = new SliderAtt(audioProcessor.getAPVTS(), paramId, *knob);

  sliders_.add(knob);
  sliderAtts_.add(att);
  controls_.push_back({knob, label, CtrlType::Knob, section, col, row});
}

void SoundsgoodAudioProcessorEditor::addCombo(const juce::String& paramId,
                                              const juce::String& label,
                                              int section, int col, int row) {
  auto* combo = new juce::ComboBox();

  // Populate from AudioParameterChoice
  auto* param = dynamic_cast<juce::AudioParameterChoice*>(
      audioProcessor.getAPVTS().getParameter(paramId));
  if (param != nullptr) {
    for (int i = 0; i < param->choices.size(); ++i) {
      combo->addItem(param->choices[i], i + 1);
    }
  }

  combo->setColour(juce::ComboBox::backgroundColourId,
                   juce::Colour(0xff2a2a3e));
  combo->setColour(juce::ComboBox::textColourId, juce::Colour(0xffd0d0d0));
  combo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff444466));
  addAndMakeVisible(combo);

  auto* att = new ComboAtt(audioProcessor.getAPVTS(), paramId, *combo);

  combos_.add(combo);
  comboAtts_.add(att);
  controls_.push_back({combo, label, CtrlType::Combo, section, col, row});
}

void SoundsgoodAudioProcessorEditor::addToggle(const juce::String& paramId,
                                               const juce::String& label,
                                               int section, int col, int row) {
  auto* btn = new juce::ToggleButton(label);
  btn->setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd0d0d0));
  btn->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffe0a030));
  addAndMakeVisible(btn);

  auto* att = new ButtonAtt(audioProcessor.getAPVTS(), paramId, *btn);

  toggles_.add(btn);
  buttonAtts_.add(att);
  controls_.push_back({btn, label, CtrlType::Toggle, section, col, row});
}

//==============================================================================
void SoundsgoodAudioProcessorEditor::paint(juce::Graphics& g) {
  // ── Background ──
  g.fillAll(juce::Colour(0xff1a1a2e));

  // ── Title bar ──
  g.setColour(juce::Colour(0xff12121f));
  g.fillRect(0, 0, getWidth(), Layout::kTitleH);
  g.setColour(juce::Colour(0xffe0a030));
  g.setFont(juce::FontOptions(22.0f));
  g.drawText("SOUNDSGOOD", 16, 8, 200, 28, juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff888899));
  g.setFont(juce::FontOptions(12.0f));
  g.drawText("Yamaha CS-30 Emulation", 190, 14, 220, 16,
             juce::Justification::centredLeft);

  // ── Section panels ──
  for (size_t i = 0; i < sections_.size(); ++i) {
    auto& sec = sections_[i];
    int baseY = (sec.panelRow == 0) ? Layout::kTopRowY : Layout::kBotRowY;
    int h = (sec.panelRow == 0) ? Layout::kTopRowH : Layout::kBotRowH;

    // Header
    g.setColour(sec.colour);
    g.fillRect(sec.x, baseY, sec.width, Layout::kSectionHeaderH);

    // Header text
    g.setColour(juce::Colour(0xffd0d0d0));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(sec.name, sec.x + 6, baseY + 2, sec.width - 12,
               Layout::kSectionHeaderH - 4, juce::Justification::centredLeft);

    // Body tint
    g.setColour(sec.colour.withAlpha(0.12f));
    g.fillRect(sec.x, baseY + Layout::kSectionHeaderH, sec.width,
               h - Layout::kSectionHeaderH);

    // Vertical divider
    if (i > 0 && sec.panelRow == sections_[i - 1].panelRow) {
      g.setColour(juce::Colour(0xff333355));
      g.drawVerticalLine(sec.x, static_cast<float>(baseY),
                         static_cast<float>(baseY + h));
    }
  }

  // ── Control labels ──
  g.setColour(juce::Colour(0xff999aaa));
  g.setFont(juce::FontOptions(9.0f));
  for (auto& ctrl : controls_) {
    if (ctrl.type == CtrlType::Toggle) {
      continue;  // toggles draw their own label
    }
    auto b = ctrl.comp->getBounds();
    g.drawText(ctrl.label, b.getX() - 4, b.getY() - Layout::kLabelH,
               b.getWidth() + 8, Layout::kLabelH, juce::Justification::centred);
  }
  // ── Sequencer step LEDs ──
  {
    bool running = audioProcessor.isSequencerRunning();
    int activeStep = audioProcessor.getCurrentSequencerStep();
    int numSteps = audioProcessor.getSequencerNumSteps();
    constexpr float kLedRadius = 4.0f;

    for (int s = 0; s < 8; ++s) {
      if (stepKnobComps_[s] == nullptr) {
        continue;
      }
      auto kb = stepKnobComps_[s]->getBounds();
      float cx = static_cast<float>(kb.getCentreX());
      float cy = static_cast<float>(kb.getY()) - Layout::kLabelH + 2.0f;

      bool isActive = running && (s == activeStep) && (s < numSteps);
      bool isInRange = s < numSteps;

      if (isActive) {
        // Bright glowing LED
        g.setColour(juce::Colour(0xffff4422));
        g.fillEllipse(cx - kLedRadius - 1, cy - kLedRadius - 1,
                      (kLedRadius + 1) * 2, (kLedRadius + 1) * 2);
        g.setColour(juce::Colour(0xffffaa66));
        g.fillEllipse(cx - kLedRadius + 1, cy - kLedRadius + 1,
                      (kLedRadius - 1) * 2, (kLedRadius - 1) * 2);
      } else if (isInRange) {
        // Dim LED (step is in range but not active)
        g.setColour(juce::Colour(0xff442211));
        g.fillEllipse(cx - kLedRadius, cy - kLedRadius, kLedRadius * 2,
                      kLedRadius * 2);
      } else {
        // Off LED (step is out of range)
        g.setColour(juce::Colour(0xff221111));
        g.fillEllipse(cx - kLedRadius, cy - kLedRadius, kLedRadius * 2,
                      kLedRadius * 2);
      }

      // LED bezel
      g.setColour(juce::Colour(0xff333344));
      g.drawEllipse(cx - kLedRadius, cy - kLedRadius, kLedRadius * 2,
                    kLedRadius * 2, 0.75f);
    }
  }
}

void SoundsgoodAudioProcessorEditor::timerCallback() {
  // Update waveform scope every frame
  scopeDisplay_.updateFromProcessor(audioProcessor);

  // Update EG curve displays from current knob values
  auto& apvts = audioProcessor.getAPVTS();
  egCurve1_.setParameters(*apvts.getRawParameterValue("eg1Atk"),
                          *apvts.getRawParameterValue("eg1Dec"),
                          *apvts.getRawParameterValue("eg1Sus"),
                          *apvts.getRawParameterValue("eg1Rel"));
  egCurve2_.setParameters(*apvts.getRawParameterValue("eg2Atk"),
                          *apvts.getRawParameterValue("eg2Dec"),
                          *apvts.getRawParameterValue("eg2Sus"),
                          *apvts.getRawParameterValue("eg2Rel"));
  egCurve3_.setParameters(*apvts.getRawParameterValue("eg3Atk"),
                          *apvts.getRawParameterValue("eg3Dec"),
                          *apvts.getRawParameterValue("eg3Sus"),
                          *apvts.getRawParameterValue("eg3Rel"));

  // Repaint only the sequencer region for LED updates
  if (audioProcessor.isSequencerRunning()) {
    auto& seqSec = sections_[8];
    int baseY = Layout::kTopRowY;
    repaint(seqSec.x, baseY, seqSec.width, Layout::kTopRowH);
  }
}

void SoundsgoodAudioProcessorEditor::resized() {
  for (auto& ctrl : controls_) {
    auto& sec = sections_[static_cast<size_t>(ctrl.section)];
    int baseY = (sec.panelRow == 0) ? Layout::kTopRowY : Layout::kBotRowY;
    int ctrlTopY = baseY + Layout::kSectionHeaderH + Layout::kLabelH + 4;

    int y = ctrlTopY + ctrl.row * Layout::kCellH;

    switch (ctrl.type) {
      case CtrlType::Knob: {
        int x = sec.x + Layout::kMarginX + ctrl.col * Layout::kCellW;
        int knobX = x + (Layout::kCellW - Layout::kKnobSz) / 2;
        ctrl.comp->setBounds(knobX, y, Layout::kKnobSz, Layout::kKnobSz + 14);
        break;
      }
      case CtrlType::Combo: {
        int x = sec.x + Layout::kMarginX + ctrl.col * Layout::kComboCellW;
        int comboY = y + (Layout::kKnobSz - Layout::kComboH) / 2;
        ctrl.comp->setBounds(x, comboY, Layout::kComboW, Layout::kComboH);
        break;
      }
      case CtrlType::Toggle: {
        int x = sec.x + Layout::kMarginX + ctrl.col * Layout::kComboCellW;
        int toggleY = y + (Layout::kKnobSz - Layout::kToggleH) / 2;
        ctrl.comp->setBounds(x, toggleY, Layout::kToggleW, Layout::kToggleH);
        break;
      }
    }
  }

  // Position waveform scope in OUTPUT section (row 1 area)
  {
    auto& outSec = sections_[15];
    int baseY = Layout::kBotRowY;
    int ctrlTopY = baseY + Layout::kSectionHeaderH + Layout::kLabelH + 4;
    int scopeY = ctrlTopY + Layout::kCellH + 4;
    int scopeX = outSec.x + Layout::kMarginX;
    int scopeW = outSec.width - Layout::kMarginX * 2;
    int scopeH = Layout::kCellH - 8;
    scopeDisplay_.setBounds(scopeX, scopeY, scopeW, scopeH);
  }

  // Position EG curve displays in row 1 of each EG section
  for (int egIdx = 0; egIdx < 3; ++egIdx) {
    auto& sec = sections_[static_cast<size_t>(9 + egIdx)];
    int baseY = Layout::kBotRowY;
    int ctrlTopY = baseY + Layout::kSectionHeaderH + Layout::kLabelH + 4;
    int curveY = ctrlTopY + Layout::kCellH + 4;
    int curveX = sec.x + Layout::kMarginX;
    int curveW = sec.width - Layout::kMarginX * 2;
    int curveH = Layout::kCellH - 8;
    EGCurveDisplay* curves[] = {&egCurve1_, &egCurve2_, &egCurve3_};
    curves[egIdx]->setBounds(curveX, curveY, curveW, curveH);
  }
}
