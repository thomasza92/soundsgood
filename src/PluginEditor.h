/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

//==============================================================================
/**
 */
class SoundsgoodAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Slider::Listener {
 public:
  explicit SoundsgoodAudioProcessorEditor(SoundsgoodAudioProcessor&);
  ~SoundsgoodAudioProcessorEditor() override;

  //==============================================================================
  void paint(juce::Graphics& /*g*/) override;
  void resized() override;

  // Slider listener callback
  void sliderValueChanged(juce::Slider* sliderThatHasChanged) override;

 private:
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  SoundsgoodAudioProcessor& audioProcessor;

  // Waveform selector combo box
  std::unique_ptr<juce::ComboBox> waveformComboBox;

  // Frequency slider
  std::unique_ptr<juce::Slider> frequencySlider;

  // Labels for UI elements
  std::unique_ptr<juce::Label> waveformLabel;
  std::unique_ptr<juce::Label> frequencyLabel;

  // Helper to get waveform name string
  static juce::String getWaveformName(
      SoundsgoodAudioProcessor::Waveform waveform);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsgoodAudioProcessorEditor)
};
