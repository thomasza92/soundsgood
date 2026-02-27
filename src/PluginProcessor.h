/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 */
class SoundsgoodAudioProcessor : public juce::AudioProcessor {
 public:
  //==============================================================================
  SoundsgoodAudioProcessor();
  ~SoundsgoodAudioProcessor() override;

  //==============================================================================
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  //==============================================================================
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  //==============================================================================
  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  //==============================================================================
  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String& newName) override;

  //==============================================================================
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // Oscillator waveform types
  enum class Waveform : uint8 { sine, square, saw };

  // Public accessors for editor
  Waveform getWaveform() const { return currentWaveform; }
  void setWaveform(Waveform waveform) { currentWaveform = waveform; }
  double getFrequency() const { return currentFrequency; }
  void setFrequency(double freq) { currentFrequency = freq; }

 private:
  //==============================================================================
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsgoodAudioProcessor)

  // Oscillator state
  Waveform currentWaveform{Waveform::sine};
  double currentFrequency{440.0};

  // JUCE DSP oscillator
  juce::dsp::Oscillator<float> oscillator;

  // Frequency parameter for automation
  std::unique_ptr<juce::AudioParameterFloat> frequencyParam;

  // Helper function to convert enum to JUCE oscillator waveform function
  std::function<float(float)> getOscillatorWaveformFunction(Waveform waveform);

  // MIDI note handling
  // Map of active MIDI notes: note number -> frequency
  std::unordered_map<int, double> activeNotes;

  // Helper to convert MIDI note number to frequency
  static double midiNoteToFrequency(int note);
};
