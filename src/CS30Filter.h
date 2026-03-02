/*
  ==============================================================================
    CS30Filter.h
    Yamaha CS-30 VCF emulation.

    Models the IG00156 state-variable filter with simultaneous HP/BP/LP outputs
    (Pin 15/13/10). Includes NJM4558DN op-amp (IC3/IC4) input saturation stage,
    rank-dependent component scaling (Rank A/B/C), keyboard follow (Pin 2 KV),
    and resonance control (Pin 7 VQ).
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include <cmath>
#include <cstdint>

//==============================================================================
// NJM4558D op-amp soft-clipping model.
// The 4558 clips symmetrically at roughly ±13V supply rails, with a smooth
// saturation onset around ±10V. Modeled with tanh for the soft-knee shape.
namespace OpAmpSaturation {
inline float processSample(float input, float drive) {
  float scaled = input * (1.0f + drive * 4.0f);
  return std::tanh(scaled);
}
}  // namespace OpAmpSaturation

//==============================================================================
class CS30Filter {
 public:
  // IG00156 simultaneous outputs: Pin 15 (HP), Pin 13 (BP), Pin 10 (LP)
  enum class Type : std::uint8_t { LowPass = 0, BandPass, HighPass };

  // Component rank determines resistor values in the frequency/Q path.
  // Rank A: R1/R2=3.0kΩ, R3/R4=470Ω (baseline)
  // Rank B: R1/R2=2.4kΩ, R3/R4=430Ω (brighter, slightly more resonant)
  // Rank C: R1/R2=2.0kΩ, R3/R4=390Ω (brightest, most resonant)
  enum class Rank : std::uint8_t { A = 0, B, C };

  // All three simultaneous filter outputs from the IG00156
  struct Outputs {
    float hp = 0.0f;
    float bp = 0.0f;
    float lp = 0.0f;
  };

  void prepare(double sampleRate);
  void setCutoff(float freq);
  void setResonance(float q);
  void setType(Type t);
  void setRank(Rank r);
  void setDrive(float d);
  void setKeyboardFollow(float amount, float noteFreq);
  void reset();

  // Process and return the selected type output
  float processSample(float input);

  // Process and return all three outputs simultaneously (HP/BP/LP)
  Outputs processSampleAll(float input);

 private:
  double sampleRate_ = 44100.0;
  float cutoff_ = 1000.0f;
  float resonance_ = 0.0f;
  float drive_ = 0.0f;
  Type type_ = Type::LowPass;
  Rank rank_ = Rank::A;
  float keyFollowAmount_ = 0.0f;
  float keyFollowFreq_ = 440.0f;

  // Cytomic TPT SVF state (ic1eq, ic2eq)
  float ic1eq_ = 0.0f;
  float ic2eq_ = 0.0f;

  float getRankFreqScale() const;
  float getRankQScale() const;
  float getEffectiveCutoff() const;
};
