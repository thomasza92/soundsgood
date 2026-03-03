/*
  ==============================================================================
    AnalogStage.h
    WDF-based analog processing stages using chowdsp_wdf.

    1. DiodeClipperStage - soft saturation via anti-parallel diode pair
    2. RCLowpassStage    - gentle analog RC lowpass for high-frequency taming
  ==============================================================================
*/

#pragma once

#include <chowdsp_wdf/chowdsp_wdf.h>

#include <algorithm>
#include <cmath>

//==============================================================================
// Anti-parallel diode clipper: Vs(R) → [C ∥ DiodePair]
// Models a classic guitar-pedal-style soft-clip circuit.
// Drive parameter scales the input voltage before the WDF tree.
class DiodeClipperStage {
 public:
  DiodeClipperStage() = default;

  void prepare(double sampleRate) {
    C1_.prepare(static_cast<float>(sampleRate));
    C1_.reset();
    sampleRate_ = sampleRate;
    dryEnv_ = 0.0f;
    wetEnv_ = 0.0f;
  }

  void reset() {
    C1_.reset();
    dryEnv_ = 0.0f;
    wetEnv_ = 0.0f;
  }

  // drive: [0, 1] maps to ~1x–10x voltage gain into the clipper
  // mix:   [0, 1] dry/wet blend
  void setParams(float drive, float mix) {
    drive_ = 1.0f + drive * 9.0f;  // 1x .. 10x
    mix_ = std::clamp(mix, 0.0f, 1.0f);
  }

  float processSample(float x) {
    float driven = x * drive_;

    Vs_.setVoltage(driven);

    // WDF process: reflect up → root scatters → incident back down
    dp_.incident(S1_.reflected());
    S1_.incident(dp_.reflected());

    float wet = C1_.voltage();

    // Envelope-based makeup gain: track dry & wet levels with a leaky
    // integrator, then scale wet to match dry so the mix stays volume-neutral.
    constexpr float kEnvCoeff = 0.0005f;  // ~2 ms @ 44.1 kHz
    dryEnv_ += kEnvCoeff * (std::abs(x) - dryEnv_);
    wetEnv_ += kEnvCoeff * (std::abs(wet) - wetEnv_);

    if (wetEnv_ > 1.0e-6f) {
      wet *= (dryEnv_ / wetEnv_);
    }

    return x * (1.0f - mix_) + wet * mix_;
  }

 private:
  double sampleRate_ = 44100.0;
  float drive_ = 1.0f;
  float mix_ = 0.5f;
  float dryEnv_ = 0.0f;
  float wetEnv_ = 0.0f;

  // Circuit elements (runtime-polymorphic API)
  // Canonical diode clipper: Vs+R in series with C, DiodePair as root.
  chowdsp::wdf::ResistiveVoltageSource<float> Vs_{4700.0f};  // 4.7 kΩ series R
  chowdsp::wdf::Capacitor<float> C1_{47.0e-9f};              // 47 nF cap

  // Topology: Vs in series with C1 (canonical diode clipper)
  chowdsp::wdf::WDFSeries<float> S1_{&Vs_, &C1_};

  // Root: anti-parallel diode pair (Is = 2.52e-9, Vt = 25.85e-3)
  chowdsp::wdf::DiodePair<float> dp_{&S1_, 2.52e-9f, 25.85e-3f};
};

//==============================================================================
// Simple 1-pole RC lowpass for gentle high-end rolloff / warmth.
// Uses WDF capacitor + resistive voltage source.
class RCLowpassStage {
 public:
  RCLowpassStage() = default;

  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    updateComponents();
    C1_.prepare(static_cast<float>(sampleRate));
    C1_.reset();
  }

  void reset() { C1_.reset(); }

  // cutoffHz: lowpass cutoff in Hz
  void setCutoff(float cutoffHz) {
    cutoffHz_ = std::clamp(cutoffHz, 20.0f, 20000.0f);
    updateComponents();
  }

  float processSample(float x) {
    Vs_.setVoltage(x);

    dp_.incident(P1_.reflected());
    P1_.incident(dp_.reflected());

    return C1_.voltage();
  }

 private:
  void updateComponents() {
    // R = 1 / (2*pi*f*C)
    float R = 1.0f / (2.0f * 3.14159265f * cutoffHz_ * kCapacitance);
    Vs_.setResistanceValue(R);
  }

  static constexpr float kCapacitance = 10.0e-9f;  // 10 nF

  double sampleRate_ = 44100.0;
  float cutoffHz_ = 8000.0f;

  chowdsp::wdf::ResistiveVoltageSource<float> Vs_{1000.0f};
  chowdsp::wdf::Capacitor<float> C1_{10.0e-9f};  // 10 nF

  chowdsp::wdf::WDFParallel<float> P1_{&Vs_, &C1_};
  chowdsp::wdf::DiodePair<float> dp_{&P1_, 2.52e-9f, 25.85e-3f};
};
