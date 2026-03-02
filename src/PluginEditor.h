/*
  ==============================================================================
    PluginEditor.h
    Soundsgood — CS-30 full front panel emulation UI.
    Two-row layout matching the physical CS-30 panel order.
    Top: PITCH, VCO1, VCO2, VCF1, VCF2, VCA1, VCA2, TRIGGER, SEQUENCER
    Bottom: EG1, EG2, EG3, LFO, PORTAMENTO, PITCH BEND, OUTPUT
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

//==============================================================================
// Real-time waveform scope display (placed in the OUTPUT section)
class WaveformDisplay : public juce::Component {
 public:
  void updateFromProcessor(const SoundsgoodAudioProcessor& proc);
  void paint(juce::Graphics& g) override;

 private:
  static constexpr int kDisplaySamples = 256;
  float displayBuffer_[kDisplaySamples] = {};
};

//==============================================================================
// ADSR curve display for EG sections (reads knob values, draws computed shape)
class EGCurveDisplay : public juce::Component {
 public:
  void setParameters(float attack, float decay, float sustain, float release);
  void paint(juce::Graphics& g) override;

 private:
  float attack_ = 0.01f;
  float decay_ = 0.3f;
  float sustain_ = 0.7f;
  float release_ = 0.5f;
};

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

  SoundsgoodAudioProcessor& audioProcessor;

  // ── Attachment aliases ──
  using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
  using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

  // ── Control type tag ──
  enum class CtrlType : std::uint8_t { Knob, Combo, Toggle };

  // ── Per-control layout info ──
  struct CtrlInfo {
    juce::Component* comp;
    juce::String label;
    CtrlType type;
    int section;
    int col;
    int row;
  };
  std::vector<CtrlInfo> controls_;

  // ── Section layout info ──
  struct SectionInfo {
    juce::String name;
    juce::Colour colour;
    int panelRow;  // 0=top, 1=bottom
    int x;
    int width;
  };
  std::vector<SectionInfo> sections_;

  // ── Owned components and attachments ──
  juce::OwnedArray<juce::Slider> sliders_;
  juce::OwnedArray<juce::ComboBox> combos_;
  juce::OwnedArray<juce::ToggleButton> toggles_;
  juce::OwnedArray<SliderAtt> sliderAtts_;
  juce::OwnedArray<ComboAtt> comboAtts_;
  juce::OwnedArray<ButtonAtt> buttonAtts_;

  // ── Helpers ──
  void addKnob(const juce::String& paramId, const juce::String& label,
               int section, int col, int row);
  void addCombo(const juce::String& paramId, const juce::String& label,
                int section, int col, int row);
  void addToggle(const juce::String& paramId, const juce::String& label,
                 int section, int col, int row);

  // ── Sequencer step LED tracking ──
  // Pointers to the 8 step knob components (for LED positioning)
  juce::Component* stepKnobComps_[8] = {};

  // ── Waveform visualizer ──
  WaveformDisplay scopeDisplay_;

  // ── EG curve displays ──
  EGCurveDisplay egCurve1_;
  EGCurveDisplay egCurve2_;
  EGCurveDisplay egCurve3_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsgoodAudioProcessorEditor)
};
