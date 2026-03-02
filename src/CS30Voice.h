/*
  ==============================================================================
    CS30Voice.h
    Yamaha CS-30 full signal path — polyphonic voice.

    Per-voice signal flow:
      VCO 1 + VCO 2 → VCF 1 (IG00156) → }
                     → VCF 2 (IG00156) → } → VCA 1 (IG00151) mixer
      VCA 1 ──────────────────────────────→ VCA 2 (normal path)
      VCO1 × carrier ─────────────────────→ VCA 2 (ring mod path)
      VCA 2 → OUTPUT

    Modulation sources (passed from processor per-block):
      LFO value (multiple waveforms), Sequencer CV, EG1/EG2/EG3
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "CS30Envelope.h"
#include "CS30Filter.h"
#include "CS30LFO.h"
#include "CS30Oscillator.h"

//==============================================================================
class CS30Sound : public juce::SynthesiserSound {
 public:
  bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
  bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

//==============================================================================
// Parameters passed from the processor to each voice per-block
struct CS30VoiceParams {
  // Pitch
  float pitchTune = 0.0f;
  float pitchDetune = 0.0f;
  float pitchEGDepth = 0.0f;
  int pitchEGSel = 0;  // 0=EG1, 1=EG2, 2=EG3

  // VCO 1
  int vco1Feet = 2;  // index: 0=32', 1=16', 2=8', 3=4', 4=2'
  float vco1PW = 0.5f;
  float vco1ModVCO2 = 0.0f;
  float vco1PWM = 0.0f;
  CS30Oscillator::Waveform vco1Func = CS30Oscillator::Waveform::Saw;
  float vco1Depth = 5.0f;
  bool vco1Seq = false;  // false=KBD, true=SEQ
  int vco1EGSel = 1;     // 0=EG1, 1=EG2, 2=EG3 (default EG2 = amplitude)

  // VCO 2
  int vco2Feet = 2;
  float vco2PW = 0.5f;
  float vco2PWM = 0.0f;
  CS30Oscillator::Waveform vco2Func = CS30Oscillator::Waveform::Saw;
  float vco2Depth = 5.0f;
  bool vco2Seq = false;  // false=KBD, true=SEQ
  int vco2EGSel = 1;     // 0=EG1, 1=EG2, 2=EG3 (default EG2 = amplitude)

  // VCF 1
  float vcf1Level = 10.0f;
  float vcf1Cutoff = 5000.0f;
  float vcf1KBDFollow = 5.0f;
  float vcf1Reso = 0.0f;
  float vcf1ModDepth = 0.0f;
  float vcf1EGDepth = 5.0f;
  CS30LFO::Waveform vcf1ModFunc = CS30LFO::Waveform::Sine;
  int vcf1EGSel = 0;

  // VCF 2
  float vcf2Level = 10.0f;
  float vcf2Cutoff = 5000.0f;
  float vcf2KBDFollow = 5.0f;
  float vcf2Reso = 0.0f;
  float vcf2ModDepth = 0.0f;
  float vcf2EGDepth = 5.0f;
  CS30LFO::Waveform vcf2ModFunc = CS30LFO::Waveform::Sine;
  int vcf2EGSel = 0;

  // VCA 1
  CS30Filter::Type vca1FilterType = CS30Filter::Type::LowPass;
  float vca1VCF1 = 10.0f;
  float vca1VCF2 = 0.0f;
  float vca1ModDepth = 0.0f;
  float vca1VCO1 = 0.0f;
  CS30LFO::Waveform vca1ModFunc = CS30LFO::Waveform::Sine;

  // VCA 2
  float vca2NormMod = 0.0f;  // 0=Normal, 1=Ring mod
  float vca2ModDepth = 0.0f;
  int vca2RMOSrc = 0;  // 0=VCO1, 1=LFO
  CS30LFO::Waveform vca2ModFunc = CS30LFO::Waveform::Sine;

  // Trigger
  bool trigLFO = false;
  bool trigMulti = true;

  // LFO EG gating (controls LFO amplitude via selected EG)
  float lfoEGDepth = 0.0f;  // 0=no gating, 10=full EG control
  int lfoEGSel = 0;         // 0=EG1, 1=EG2, 2=EG3

  // EG 1, 2, 3 (ADSR each)
  CS30Envelope::Parameters eg1Params;
  CS30Envelope::Parameters eg2Params;
  CS30Envelope::Parameters eg3Params;

  // Sequencer CV (semitones ±24, routed to VCO pitch like real CS-30)
  float sequencerCV = 0.0f;

  // Portamento
  float portamento = 0.0f;  // 0–10 mapped to glide time

  // Pitch bend
  float pbRange = 2.0f;  // semitones

  // Output
  float outBalance = 0.5f;  // 0=VCA1, 1=VCA2
  float outVolume = 8.0f;
};

//==============================================================================
class CS30Voice : public juce::SynthesiserVoice {
 public:
  CS30Voice();

  bool canPlaySound(juce::SynthesiserSound* sound) override;

  void startNote(int midiNoteNumber, float velocity,
                 juce::SynthesiserSound* sound,
                 int currentPitchWheelPosition) override;
  void stopNote(float velocity, bool allowTailOff) override;
  void pitchWheelMoved(int newPitchWheelValue) override;
  void controllerMoved(int controllerNumber, int newControllerValue) override;

  void prepareToPlay(double sampleRate, int samplesPerBlock);

  void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample,
                       int numSamples) override;

  // Retrigger: change pitch without stopping the voice (mono mode)
  void retriggerNote(int midiNoteNumber, float velocity);

  // Set all parameters at once (called per-block from processBlock)
  void setParams(const CS30VoiceParams& p);

  // Set the current global LFO values (all waveforms, called per-block)
  void setLFOValues(float sine, float tri, float saw, float square, float sh);

  // Mark this voice as the sequencer auto-voice (mutes non-SEQ VCOs)
  void setSeqAutoVoice(bool v) { isSeqAutoVoice_ = v; }

 private:
  // ── DSP modules ──
  CS30Oscillator vco1_;
  CS30Oscillator vco2_;
  CS30Filter vcf1_;
  CS30Filter vcf2_;
  CS30Envelope eg1_;
  CS30Envelope eg2_;
  CS30Envelope eg3_;
  GermaniumDiodeClipper fmClipper_;

  // ── Voice state ──
  float noteFrequency_ = 440.0f;
  float targetFrequency_ = 440.0f;
  float currentFrequency_ = 440.0f;  // smoothed for portamento
  float portamentoRate_ = 1.0f;
  float velocity_ = 0.0f;
  float pitchWheelSemitones_ = 0.0f;
  bool noteHeld_ = false;     // true between startNote and stopNote
  bool lfoGateHigh_ = false;  // tracks LFO square wave state for edge detection
  bool isSeqAutoVoice_ =
      false;  // true when this voice is the sequencer auto-voice

  // ── Parameter cache ──
  CS30VoiceParams params_;

  // ── LFO cache (from processor) ──
  float lfoValues_[5] = {};  // indexed by CS30LFO::Waveform

  bool prepared_ = false;

  // ── Helpers ──
  static float feetToMultiplier(int feetIndex);
  float getEnvelopeValue(int egSel) const;
  float getLFOValue(CS30LFO::Waveform w) const;

  // Mutable EG outputs (updated per-sample in renderNextBlock)
  mutable float eg1Val_ = 0.0f;
  mutable float eg2Val_ = 0.0f;
  mutable float eg3Val_ = 0.0f;
};
