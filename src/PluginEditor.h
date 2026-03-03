/*
  ==============================================================================
    PluginEditor.h
    Soundsgood -- WPPM synthesiser editor.

    Layout (4 rows):
      Row 1 — WPPM waveform viz + Split/Pulse1/Link/Pulse2
      Row 2 — Sub + Unison + Filter
      Row 3 — LFO [1|2] tabs + Mod Matrix + Envelope [Amp|Mod] tabs
      Row 4 — Analog + Quality + Output (Vol + Scope)

    Tab system inspired by Serum: clickable tab buttons in section
    headers switch between LFO1/LFO2 and Env1(Amp)/Env2(Mod).
  ==============================================================================
*/

#pragma once

#include <array>

#include "PluginProcessor.h"

//==============================================================================
class SoundsgoodAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer {
 public:
  explicit SoundsgoodAudioProcessorEditor(SoundsgoodAudioProcessor&);
  ~SoundsgoodAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;
  void drawScope(juce::Graphics& g, juce::Rectangle<int> area);
  void drawWPPMWaveform(juce::Graphics& g, juce::Rectangle<int> area);
  void drawEnvelopeCurve(juce::Graphics& g, juce::Rectangle<int> area, float a,
                         float d, float s, float r);
  void updateWPPMWaveform();
  void switchLFOTab(int tab);
  void switchEnvTab(int tab);

  // Helpers
  using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
  using ComboAttach = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

  struct LabelledKnob {
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<SliderAttach> attachment;
  };

  struct LabelledCombo {
    juce::ComboBox combo;
    juce::Label label;
    std::unique_ptr<ComboAttach> attachment;
  };

  void setupKnob(LabelledKnob& k, const juce::String& paramId,
                 const juce::String& text);
  void setupCombo(LabelledCombo& c, const juce::String& paramId,
                  const juce::String& text, const juce::StringArray& choices);

  SoundsgoodAudioProcessor& processorRef;

  // --- WPPM knobs ---
  LabelledKnob splitKnob;
  LabelledKnob ratio1Knob, depth1Knob, phase1Knob;
  LabelledKnob ratio2Knob, depth2Knob, phase2Knob;
  juce::ToggleButton linkButton{"Link P1=P2"};
  std::unique_ptr<ButtonAttach> linkAttach;

  // --- Sub oscillator ---
  LabelledCombo subOctaveCombo;
  LabelledKnob subMixKnob;

  // --- Unison ---
  LabelledKnob unisonVoicesKnob, unisonDetuneKnob, unisonSpreadKnob;

  // --- Filter ---
  LabelledCombo filterModeCombo;
  LabelledKnob filterCutoffKnob, filterResonanceKnob;
  LabelledKnob filterKeyTrackKnob, filterEnvAmountKnob;

  // --- LFO 1 ---
  LabelledCombo lfoShapeCombo, lfoDestCombo;
  LabelledKnob lfoRateKnob, lfoDepthKnob;

  // --- LFO 2 ---
  LabelledCombo lfo2ShapeCombo, lfo2DestCombo;
  LabelledKnob lfo2RateKnob, lfo2DepthKnob;

  // LFO tab buttons
  juce::TextButton lfoTab1{"1"}, lfoTab2{"2"};
  int activeLFOTab_ = 0;

  // --- Modulation matrix ---
  LabelledKnob velToDKnob, velToDepthKnob;
  LabelledKnob atToDKnob, atToDepthKnob;

  // --- Envelope 1 (Amp) ---
  LabelledKnob attackKnob, decayKnob, sustainKnob, releaseKnob;

  // --- Envelope 2 (Mod) ---
  LabelledKnob env2AttackKnob, env2DecayKnob, env2SustainKnob, env2ReleaseKnob;
  LabelledCombo env2DestCombo;
  LabelledKnob env2AmountKnob;

  // Envelope tab buttons
  juce::TextButton envTab1{"Amp"}, envTab2{"Mod"};
  int activeEnvTab_ = 0;
  juce::Rectangle<int> envelopeCurveBounds_;

  // --- Analog stage ---
  LabelledKnob analogDriveKnob, analogMixKnob;

  // --- Quality ---
  LabelledCombo qualityCombo;

  // --- Output ---
  LabelledKnob volumeKnob;

  // WPPM waveform preview
  static constexpr int kWPPMVizSize = 256;
  std::array<float, kWPPMVizSize> wppmWaveform_{};
  juce::Rectangle<int> wppmVizBounds_;

  // Output scope
  std::array<float, SoundsgoodAudioProcessor::kScopeSize> scopeData_{};
  juce::Rectangle<int> scopeBounds_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsgoodAudioProcessorEditor)
};
