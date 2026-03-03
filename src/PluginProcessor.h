/*
  ==============================================================================
    PluginProcessor.h
    Soundsgood -- WPPM (Windowed Piecewise Phase Modulation) synthesiser.

    Parameters (44 total):
      WPPM: d, R1, I1, P1, R2, I2, P2, link
      Sub:  subOctave, subMix
      Unison: unisonVoices, unisonDetune, unisonSpread
      Filter: filterMode, filterCutoff, filterResonance, filterKeyTrack,
              filterEnvAmount
      LFO1: lfoShape, lfoRate, lfoDepth, lfoDest
      LFO2: lfo2Shape, lfo2Rate, lfo2Depth, lfo2Dest
      Mod: velToD, velToDepth, atToD, atToDepth
      Env1: attack, decay, sustain, release
      Env2: env2Attack, env2Decay, env2Sustain, env2Release, env2Dest,
            env2Amount
      Analog: analogDrive, analogMix
      Quality: oversampleMode
      Output: volume
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class SoundsgoodAudioProcessor : public juce::AudioProcessor {
 public:
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

  //==============================================================================
  // Scope buffer for waveform visualizer
  static constexpr int kScopeSize = 512;
  static constexpr int maxVoices = 8;

  // --- Accessors (editor reads these) ---
  juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
  const float* getScopeBuffer() const noexcept { return scopeBuffer_; }
  int getScopeWritePos() const noexcept {
    return scopeWritePos_.load(std::memory_order_relaxed);
  }

 private:
  juce::AudioProcessorValueTreeState apvts_;

  float scopeBuffer_[kScopeSize] = {};
  std::atomic<int> scopeWritePos_{0};
  //==============================================================================
  juce::Synthesiser synth;

  static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();

  // ===== Cached parameter pointers =====

  // WPPM oscillator (7 + link)
  std::atomic<float>* dParam = nullptr;
  std::atomic<float>* r1Param = nullptr;
  std::atomic<float>* i1Param = nullptr;
  std::atomic<float>* p1Param = nullptr;
  std::atomic<float>* r2Param = nullptr;
  std::atomic<float>* i2Param = nullptr;
  std::atomic<float>* p2Param = nullptr;
  std::atomic<float>* linkParam = nullptr;

  // Sub oscillator (2)
  std::atomic<float>* subOctaveParam = nullptr;
  std::atomic<float>* subMixParam = nullptr;

  // Unison (3)
  std::atomic<float>* unisonVoicesParam = nullptr;
  std::atomic<float>* unisonDetuneParam = nullptr;
  std::atomic<float>* unisonSpreadParam = nullptr;

  // Filter (5)
  std::atomic<float>* filterModeParam = nullptr;
  std::atomic<float>* filterCutoffParam = nullptr;
  std::atomic<float>* filterResonanceParam = nullptr;
  std::atomic<float>* filterKeyTrackParam = nullptr;
  std::atomic<float>* filterEnvAmountParam = nullptr;

  // LFO (4)
  std::atomic<float>* lfoShapeParam = nullptr;
  std::atomic<float>* lfoRateParam = nullptr;
  std::atomic<float>* lfoDepthParam = nullptr;
  std::atomic<float>* lfoDestParam = nullptr;

  // LFO 2 (4)
  std::atomic<float>* lfo2ShapeParam = nullptr;
  std::atomic<float>* lfo2RateParam = nullptr;
  std::atomic<float>* lfo2DepthParam = nullptr;
  std::atomic<float>* lfo2DestParam = nullptr;

  // Modulation routing (4)
  std::atomic<float>* velToDParam = nullptr;
  std::atomic<float>* velToDepthParam = nullptr;
  std::atomic<float>* atToDParam = nullptr;
  std::atomic<float>* atToDepthParam = nullptr;

  // Envelope ADSR (4)
  std::atomic<float>* attackParam = nullptr;
  std::atomic<float>* decayParam = nullptr;
  std::atomic<float>* sustainParam = nullptr;
  std::atomic<float>* releaseParam = nullptr;

  // Envelope 2 — mod (6)
  std::atomic<float>* env2AttackParam = nullptr;
  std::atomic<float>* env2DecayParam = nullptr;
  std::atomic<float>* env2SustainParam = nullptr;
  std::atomic<float>* env2ReleaseParam = nullptr;
  std::atomic<float>* env2DestParam = nullptr;
  std::atomic<float>* env2AmountParam = nullptr;

  // Analog stage (2)
  std::atomic<float>* analogDriveParam = nullptr;
  std::atomic<float>* analogMixParam = nullptr;

  // Quality (1)
  std::atomic<float>* oversampleModeParam = nullptr;

  // Output (1)
  std::atomic<float>* volumeParam = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsgoodAudioProcessor)
};
