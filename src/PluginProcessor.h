/*
  ==============================================================================
    PluginProcessor.h
    Soundsgood — Yamaha CS-30 full emulation.
    Polyphonic synthesiser with WDF analog circuit modeling.
    75 parameters covering every CS-30 front panel control.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "CS30LFO.h"
#include "CS30Sequencer.h"
#include "CS30Voice.h"  // IWYU pragma: keep (used by .cpp for voice types)

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
  juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

  // Expose current sequencer step for LED display in editor
  int getCurrentSequencerStep() const { return sequencer_.getCurrentStep(); }
  bool isSequencerRunning() const { return seqRunningParam->load() >= 0.5f; }
  int getSequencerNumSteps() const {
    return static_cast<int>(seqStepsParam->load()) + 1;
  }

  // Scope buffer for waveform visualizer
  static constexpr int kScopeSize = 512;
  const float* getScopeData() const { return scopeBuffer_; }
  int getScopeWritePos() const {
    return scopeWritePos_.load(std::memory_order_relaxed);
  }

  static constexpr int maxVoices = 8;

 private:
  //==============================================================================
  juce::Synthesiser synth;
  CS30LFO lfo_;
  CS30Sequencer sequencer_;

  // Sequencer auto-voice: plays a base note when sequencer is running
  bool seqNoteActive_ = false;

  // Mono mode: track the single user voice and current note
  CS30Voice* monoVoice_ = nullptr;
  int monoCurrentNote_ = -1;
  static constexpr int kSeqMidiChannel = 16;
  static constexpr int kSeqMidiNote = 60;  // C4

  static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();

  juce::AudioProcessorValueTreeState apvts;

  // ── Cached parameter pointers: 75 total ──

  // PITCH (4)
  std::atomic<float>* pitchTuneParam = nullptr;
  std::atomic<float>* pitchDetuneParam = nullptr;
  std::atomic<float>* pitchEGDepthParam = nullptr;
  std::atomic<float>* pitchEGSelParam = nullptr;

  // VCO 1 (8)
  std::atomic<float>* vco1FeetParam = nullptr;
  std::atomic<float>* vco1PWParam = nullptr;
  std::atomic<float>* vco1ModVCO2Param = nullptr;
  std::atomic<float>* vco1PWMParam = nullptr;
  std::atomic<float>* vco1FuncParam = nullptr;
  std::atomic<float>* vco1DepthParam = nullptr;
  std::atomic<float>* vco1SrcParam = nullptr;
  std::atomic<float>* vco1EGSelParam = nullptr;

  // VCO 2 (7)
  std::atomic<float>* vco2FeetParam = nullptr;
  std::atomic<float>* vco2PWParam = nullptr;
  std::atomic<float>* vco2PWMParam = nullptr;
  std::atomic<float>* vco2FuncParam = nullptr;
  std::atomic<float>* vco2DepthParam = nullptr;
  std::atomic<float>* vco2SrcParam = nullptr;
  std::atomic<float>* vco2EGSelParam = nullptr;

  // VCF 1 (8)
  std::atomic<float>* vcf1LevelParam = nullptr;
  std::atomic<float>* vcf1CutoffParam = nullptr;
  std::atomic<float>* vcf1KBDFollowParam = nullptr;
  std::atomic<float>* vcf1ResoParam = nullptr;
  std::atomic<float>* vcf1ModDepthParam = nullptr;
  std::atomic<float>* vcf1EGDepthParam = nullptr;
  std::atomic<float>* vcf1ModFuncParam = nullptr;
  std::atomic<float>* vcf1EGSelParam = nullptr;

  // VCF 2 (8)
  std::atomic<float>* vcf2LevelParam = nullptr;
  std::atomic<float>* vcf2CutoffParam = nullptr;
  std::atomic<float>* vcf2KBDFollowParam = nullptr;
  std::atomic<float>* vcf2ResoParam = nullptr;
  std::atomic<float>* vcf2ModDepthParam = nullptr;
  std::atomic<float>* vcf2EGDepthParam = nullptr;
  std::atomic<float>* vcf2ModFuncParam = nullptr;
  std::atomic<float>* vcf2EGSelParam = nullptr;

  // VCA 1 (6)
  std::atomic<float>* vca1FilterTypeParam = nullptr;
  std::atomic<float>* vca1VCF1Param = nullptr;
  std::atomic<float>* vca1VCF2Param = nullptr;
  std::atomic<float>* vca1ModDepthParam = nullptr;
  std::atomic<float>* vca1VCO1Param = nullptr;
  std::atomic<float>* vca1ModFuncParam = nullptr;

  // VCA 2 (4)
  std::atomic<float>* vca2NormModParam = nullptr;
  std::atomic<float>* vca2ModDepthParam = nullptr;
  std::atomic<float>* vca2RMOSrcParam = nullptr;
  std::atomic<float>* vca2ModFuncParam = nullptr;

  // TRIGGER (2)
  std::atomic<float>* trigModeParam = nullptr;
  std::atomic<float>* trigTypeParam = nullptr;

  // SEQUENCER (11)
  std::atomic<float>* seqClockParam = nullptr;
  std::atomic<float>* seqRunningParam = nullptr;
  std::atomic<float>* seqHoldParam = nullptr;
  std::atomic<float>* seqStepsParam = nullptr;
  std::atomic<float>* seqKnobParams[8] = {};

  // EG 1 (4)
  std::atomic<float>* eg1AtkParam = nullptr;
  std::atomic<float>* eg1DecParam = nullptr;
  std::atomic<float>* eg1SusParam = nullptr;
  std::atomic<float>* eg1RelParam = nullptr;

  // EG 2 (4)
  std::atomic<float>* eg2AtkParam = nullptr;
  std::atomic<float>* eg2DecParam = nullptr;
  std::atomic<float>* eg2SusParam = nullptr;
  std::atomic<float>* eg2RelParam = nullptr;

  // EG 3 (4)
  std::atomic<float>* eg3AtkParam = nullptr;
  std::atomic<float>* eg3DecParam = nullptr;
  std::atomic<float>* eg3SusParam = nullptr;
  std::atomic<float>* eg3RelParam = nullptr;

  // LFO (3)
  std::atomic<float>* lfoEGSelParam = nullptr;
  std::atomic<float>* lfoEGDepthParam = nullptr;
  std::atomic<float>* lfoSpeedParam = nullptr;

  // PORTAMENTO (1)
  std::atomic<float>* portamentoParam = nullptr;

  // PITCH BEND (2)
  std::atomic<float>* pbLimiterParam = nullptr;
  std::atomic<float>* pbRangeParam = nullptr;

  // OUTPUT (3)
  std::atomic<float>* outBalanceParam = nullptr;
  std::atomic<float>* outVolumeParam = nullptr;
  std::atomic<float>* outMonoParam = nullptr;

  // Scope ring buffer (audio thread writes, GUI thread reads)
  float scopeBuffer_[kScopeSize] = {};
  std::atomic<int> scopeWritePos_{0};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundsgoodAudioProcessor)
};