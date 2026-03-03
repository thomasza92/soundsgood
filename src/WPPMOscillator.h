/*
  ==============================================================================
    WPPMOscillator.h
    Windowed Piecewise Phase Modulation (WPPM) oscillator.

    Core synthesis engine implementing the WPPM algorithm:
      - Linear phase ramp split into two segments at breakpoint d
      - Each segment drives a windowed PM pulse g1, g2
      - Pulses are added to a piecewise phase distortion function F(phi)
      - Final output: -cos(2*pi*F(phi))

    Parameters (7 WPPM + sub osc + unison):
      d  : segment duration ratio [0.01, 0.99]
      R1 : pulse 1 frequency ratio
      I1 : pulse 1 modulation index (depth)
      P1 : pulse 1 phase offset
      R2 : pulse 2 frequency ratio
      I2 : pulse 2 modulation index (depth)
      P2 : pulse 2 phase offset
      subOctave  : 0 = off, 1 = -1 oct, 2 = -2 oct
      subMix     : sub oscillator mix level [0, 1]
      unisonVoices : number of unison layers (1 = off)
      unisonDetune : max detune in cents per layer
      unisonSpread : stereo spread [0, 1]
  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

class WPPMOscillator {
 public:
  static constexpr int kMaxUnison = 7;

  struct Params {
    float d = 0.5f;   // segment duration ratio [0.01, 0.99]
    float R1 = 1.0f;  // pulse 1 ratio
    float I1 = 0.0f;  // pulse 1 index (depth)
    float P1 = 0.0f;  // pulse 1 phase offset
    float R2 = 1.0f;  // pulse 2 ratio
    float I2 = 0.0f;  // pulse 2 index (depth)
    float P2 = 0.0f;  // pulse 2 phase offset

    int subOctave = 0;    // 0 = off, 1 = -1 oct, 2 = -2 oct
    float subMix = 0.0f;  // sub oscillator level [0, 1]

    int unisonVoices = 1;       // 1..kMaxUnison
    float unisonDetune = 0.0f;  // max detune in cents
    float unisonSpread = 0.0f;  // stereo spread [0, 1]
  };

  // Stereo output from processSample
  struct StereoSample {
    float left = 0.0f;
    float right = 0.0f;
  };

  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& p : phases_) {
      p = 0.0;
    }
    subPhase_ = 0.0;
  }

  // Change sample rate without resetting oscillator phase (for oversampling)
  void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

  void setFrequency(float hz) { baseHz_ = hz; }

  void setParams(const Params& p) {
    params_ = p;
    params_.d = std::clamp(params_.d, 0.01f, 0.99f);
    params_.unisonVoices = std::clamp(params_.unisonVoices, 1, kMaxUnison);
  }

  void reset() {
    for (auto& p : phases_) {
      p = 0.0;
    }
    subPhase_ = 0.0;
  }

  // Process a single sample. Returns stereo output.
  StereoSample processSample() {
    int nVoices = params_.unisonVoices;
    float sumL = 0.0f;
    float sumR = 0.0f;

    for (int v = 0; v < nVoices; ++v) {
      // Compute detuned frequency for this unison layer
      float detuneHz = 0.0f;
      if (nVoices > 1) {
        float t = static_cast<float>(v) / static_cast<float>(nVoices - 1);
        float centOffset = params_.unisonDetune * (2.0f * t - 1.0f);
        detuneHz = baseHz_ * (std::exp2(centOffset / 1200.0f) - 1.0f);
      }

      float freq = baseHz_ + detuneHz;
      double phaseInc = static_cast<double>(freq) / sampleRate_;

      // Per-voice d offset for stereo richness: ±0.02 spread across layers
      float dOffset = 0.0f;
      if (nVoices > 1) {
        float t = static_cast<float>(v) / static_cast<float>(nVoices - 1);
        dOffset = (2.0f * t - 1.0f) * 0.02f;
      }

      float voiceD = std::clamp(params_.d + dOffset, 0.01f, 0.99f);

      float sample = processSinglePhase(phases_[v], phaseInc, voiceD);

      // Stereo pan for this layer
      float pan = 0.5f;  // center
      if (nVoices > 1 && params_.unisonSpread > 0.0f) {
        float t = static_cast<float>(v) / static_cast<float>(nVoices - 1);
        pan = 0.5f + (t - 0.5f) * params_.unisonSpread;
      }

      // Constant-power pan
      float panL = std::cos(pan * kHalfPi);
      float panR = std::sin(pan * kHalfPi);
      sumL += sample * panL;
      sumR += sample * panR;
    }

    // Average unison layers
    float invN = 1.0f / static_cast<float>(nVoices);
    sumL *= invN;
    sumR *= invN;

    // Sub oscillator (mono, mixed equally)
    float subOut = 0.0f;
    if (params_.subOctave > 0 && params_.subMix > 0.0f) {
      float subFreq = baseHz_;
      for (int i = 0; i < params_.subOctave; ++i) {
        subFreq *= 0.5f;
      }
      double subInc = static_cast<double>(subFreq) / sampleRate_;

      subOut = std::sin(kTwoPi * static_cast<float>(subPhase_));
      subPhase_ += subInc;
      if (subPhase_ >= 1.0) {
        subPhase_ -= 1.0;
      }
    }

    // Mix main and sub
    float mainMix = 1.0f - params_.subMix;
    return {sumL * mainMix + subOut * params_.subMix,
            sumR * mainMix + subOut * params_.subMix};
  }

  // Mono convenience (sums L+R, for backward compatibility)
  float processSampleMono() {
    auto s = processSample();
    return (s.left + s.right) * 0.5f;
  }

 private:
  static constexpr float kTwoPi = 6.283185307179586f;
  static constexpr float kPi = 3.141592653589793f;
  static constexpr float kHalfPi = 1.5707963267948966f;

  static float window(float t) { return std::sin(kPi * t); }

  static float dWindow(float dVal) {
    return std::sin(kPi * std::clamp(dVal, 0.0f, 1.0f));
  }

  float processSinglePhase(double& phase, double phaseInc, float d) {
    float phi = static_cast<float>(phase);
    float F;

    if (phi < d) {
      float t1 = phi / d;
      float w1 = window(t1);
      float g1 =
          w1 * params_.I1 * std::sin(kTwoPi * (params_.R1 * t1 + params_.P1));
      g1 *= dWindow(d);
      F = phi / (2.0f * d) + g1;
    } else {
      float oneMinusD = 1.0f - d;
      float t2 = (phi - d) / oneMinusD;
      float w2 = window(t2);
      float g2 =
          w2 * params_.I2 * std::sin(kTwoPi * (params_.R2 * t2 + params_.P2));
      g2 *= dWindow(oneMinusD);
      F = 0.5f + (phi - d) / (2.0f * oneMinusD) + g2;
    }

    float out = -std::cos(kTwoPi * F);

    phase += phaseInc;
    if (phase >= 1.0) {
      phase -= 1.0;
    }

    return out;
  }

  double sampleRate_ = 44100.0;
  float baseHz_ = 440.0f;
  std::array<double, kMaxUnison> phases_{};
  double subPhase_ = 0.0;
  Params params_;
};
