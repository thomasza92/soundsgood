/*
  ==============================================================================
    WPPMVoice.h
    WPPM synthesiser voice with:
      - WPPM oscillator (sub-osc, stereo unison)
      - Oversampling (Eco / High 4x / Ultra 8x)
      - State-variable filter (LP/HP/BP)
      - LFO routable to d / filter / pitch / R1+R2
      - Modulation matrix: velocity & aftertouch → d / I1+I2
      - ADSR envelope
      - Per-voice WDF diode clipper
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "AnalogStage.h"
#include "Oversampler.h"
#include "WPPMLFO.h"
#include "WPPMOscillator.h"

//==============================================================================
class WPPMSound : public juce::SynthesiserSound {
 public:
  bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
  bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

//==============================================================================
enum class FilterMode : uint8_t { LowPass = 0, HighPass, BandPass };

//==============================================================================
struct WPPMVoiceParams {
  // WPPM oscillator (7)
  float d = 0.5f;
  float R1 = 1.0f;
  float I1 = 0.0f;
  float P1 = 0.0f;
  float R2 = 1.0f;
  float I2 = 0.0f;
  float P2 = 0.0f;

  // Sub oscillator
  int subOctave = 0;
  float subMix = 0.0f;

  // Unison
  int unisonVoices = 1;
  float unisonDetune = 0.0f;
  float unisonSpread = 0.0f;  // stereo spread [0, 1]

  // Filter
  FilterMode filterMode = FilterMode::LowPass;
  float filterCutoff = 8000.0f;
  float filterResonance = 0.0f;
  float filterKeyTrack = 0.0f;
  float filterEnvAmount = 0.0f;

  // LFO
  WPPMLFO::Shape lfoShape = WPPMLFO::Shape::Sine;
  float lfoRate = 1.0f;
  float lfoDepth = 0.0f;
  int lfoDest = 0;  // 0=d, 1=filter, 2=pitch, 3=ratio

  // LFO 2
  WPPMLFO::Shape lfo2Shape = WPPMLFO::Shape::Sine;
  float lfo2Rate = 1.0f;
  float lfo2Depth = 0.0f;
  int lfo2Dest = 0;

  // Modulation routing
  float velToD = 0.0f;      // velocity → d amount [-1, 1]
  float velToDepth = 0.0f;  // velocity → I1+I2 amount [-1, 1]
  float atToD = 0.0f;       // aftertouch → d amount [-1, 1]
  float atToDepth = 0.0f;   // aftertouch → I1+I2 amount [-1, 1]

  // Envelope (ADSR) — amp
  float attack = 0.01f;
  float decay = 0.3f;
  float sustain = 0.7f;
  float release = 0.5f;

  // Envelope 2 — modulation
  float env2Attack = 0.01f;
  float env2Decay = 0.3f;
  float env2Sustain = 0.0f;
  float env2Release = 0.5f;
  int env2Dest = 0;         // 0=d, 1=filter, 2=depth, 3=ratio, 4=pitch
  float env2Amount = 0.0f;  // [-1, 1]

  // Analog stage
  float analogDrive = 0.0f;
  float analogMix = 0.0f;

  // Quality
  int oversampleMode = 0;  // 0=Eco, 1=High(4x), 2=Ultra(8x)

  // Output
  float volume = 0.8f;
};

//==============================================================================
class WPPMVoice : public juce::SynthesiserVoice {
 public:
  WPPMVoice();

  bool canPlaySound(juce::SynthesiserSound* sound) override;

  void startNote(int midiNoteNumber, float velocity,
                 juce::SynthesiserSound* sound,
                 int currentPitchWheelPosition) override;

  void stopNote(float velocity, bool allowTailOff) override;

  void pitchWheelMoved(int newPitchWheelValue) override;
  void controllerMoved(int controllerNumber, int newControllerValue) override;
  void aftertouchChanged(int newAftertouchValue) override;

  void prepareToPlay(double sampleRate, int samplesPerBlock);

  void setParams(const WPPMVoiceParams& p);

  void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample,
                       int numSamples) override;

 private:
  struct SVFilter {
    void reset() { ic1eq_ = ic2eq_ = lp_ = bp_ = hp_ = 0.0f; }

    void process(float input, float cutoffHz, float resonance,
                 double sampleRate) {
      float maxCut = static_cast<float>(sampleRate) * 0.49f;
      cutoffHz = std::clamp(cutoffHz, 20.0f, maxCut);

      float g =
          std::tan(3.14159265f * cutoffHz / static_cast<float>(sampleRate));
      float k = 2.0f - 2.0f * std::clamp(resonance, 0.0f, 1.0f);
      float a1 = 1.0f / (1.0f + g * (g + k));
      float a2 = g * a1;
      float a3 = g * a2;

      float v3 = input - ic2eq_;
      float v1 = a1 * ic1eq_ + a2 * v3;
      float v2 = ic2eq_ + a2 * ic1eq_ + a3 * v3;

      ic1eq_ = 2.0f * v1 - ic1eq_;
      ic2eq_ = 2.0f * v2 - ic2eq_;

      lp_ = v2;
      bp_ = v1;
      hp_ = input - k * v1 - v2;
    }

    float getLp() const noexcept { return lp_; }
    float getBp() const noexcept { return bp_; }
    float getHp() const noexcept { return hp_; }

   private:
    float lp_ = 0.0f, bp_ = 0.0f, hp_ = 0.0f;
    float ic1eq_ = 0.0f, ic2eq_ = 0.0f;
  };

  WPPMOscillator osc_;
  WPPMLFO lfo_;
  WPPMLFO lfo2_;
  SVFilter filterL_, filterR_;  // stereo filters
  DiodeClipperStage analogStageL_, analogStageR_;
  Oversampler oversamplerL_, oversamplerR_;
  juce::ADSR adsr_;
  juce::ADSR::Parameters adsrParams_;
  juce::ADSR env2_;
  juce::ADSR::Parameters env2Params_;

  WPPMVoiceParams params_;
  float velocity_ = 0.0f;
  float aftertouch_ = 0.0f;
  float noteFrequency_ = 440.0f;
  int noteNumber_ = 60;
  float pitchWheelSemitones_ = 0.0f;
  double currentSampleRate_ = 44100.0;
  int currentOversampleMode_ = -1;  // track for re-prepare

  bool prepared_ = false;
};
