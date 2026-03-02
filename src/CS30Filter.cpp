/*
  ==============================================================================
    CS30Filter.cpp
    Yamaha CS-30 VCF (IG00156) emulation implementation.

    Uses the Cytomic/Zavalishin Topology-Preserving Transform state-variable
    filter. Rank-dependent resistor values scale the cutoff frequency and
    resonance behavior, matching the IG00156 IC manufacturing variations.
  ==============================================================================
*/

#include "CS30Filter.h"

#include <cmath>

void CS30Filter::prepare(double sampleRate) {
  sampleRate_ = sampleRate;
  reset();
}

void CS30Filter::setCutoff(float freq) {
  cutoff_ = juce::jlimit(20.0f, 20000.0f, freq);
}

void CS30Filter::setResonance(float q) {
  resonance_ = juce::jlimit(0.0f, 1.0f, q);
}

void CS30Filter::setType(Type t) { type_ = t; }
void CS30Filter::setRank(Rank r) { rank_ = r; }

void CS30Filter::setDrive(float d) { drive_ = juce::jlimit(0.0f, 1.0f, d); }

void CS30Filter::setKeyboardFollow(float amount, float noteFreq) {
  keyFollowAmount_ = amount;
  keyFollowFreq_ = noteFreq;
}

void CS30Filter::reset() {
  ic1eq_ = 0.0f;
  ic2eq_ = 0.0f;
}

float CS30Filter::getRankFreqScale() const {
  // Cutoff frequency scales inversely with R1/R2 resistance.
  // f_c ∝ 1/(R * C), so lower R → higher f_c for same CV.
  switch (rank_) {
    case Rank::A:
      return 1.0f;  // 3.0kΩ baseline
    case Rank::B:
      return 3000.0f / 2400.0f;  // ≈1.25× brighter
    case Rank::C:
      return 3000.0f / 2000.0f;  // ≈1.50× brightest
  }
  return 1.0f;
}

float CS30Filter::getRankQScale() const {
  // Resonance feedback scales inversely with R3/R4 resistance.
  // Lower R3/R4 → more resonant at the same setting.
  switch (rank_) {
    case Rank::A:
      return 1.0f;  // 470Ω baseline
    case Rank::B:
      return 470.0f / 430.0f;  // ≈1.09× sharper
    case Rank::C:
      return 470.0f / 390.0f;  // ≈1.21× sharpest
  }
  return 1.0f;
}

float CS30Filter::getEffectiveCutoff() const {
  // Pin 2 (KV): keyboard follow modulates cutoff.
  // CS-30 KV range is 0.25V–4.0V across the keyboard.
  float keyMod = keyFollowAmount_ * std::log2(keyFollowFreq_ / 440.0f) * 12.0f;
  float modifiedCutoff = cutoff_ * std::exp2(keyMod / 12.0f);

  // Apply rank-dependent frequency scaling (R1/R2)
  modifiedCutoff *= getRankFreqScale();

  return juce::jlimit(20.0f, static_cast<float>(sampleRate_ * 0.49),
                      modifiedCutoff);
}

float CS30Filter::processSample(float input) {
  auto out = processSampleAll(input);
  switch (type_) {
    case Type::LowPass:
      return out.lp;
    case Type::BandPass:
      return out.bp;
    case Type::HighPass:
      return out.hp;
  }
  return out.lp;
}

CS30Filter::Outputs CS30Filter::processSampleAll(float input) {
  // NJM4558DN (IC3/IC4) input buffer saturation
  input = OpAmpSaturation::processSample(input, drive_);

  float fc = getEffectiveCutoff();

  // Cytomic TPT SVF coefficient
  float g = std::tan(juce::MathConstants<float>::pi * fc /
                     static_cast<float>(sampleRate_));

  // Resonance with rank-dependent Q scaling (R3/R4)
  float effectiveRes = resonance_ * getRankQScale();
  effectiveRes = juce::jlimit(0.0f, 0.99f, effectiveRes);

  // Damping: k=2 at no resonance, k→0 at self-oscillation
  float k = 2.0f * (1.0f - effectiveRes);

  // Cytomic SVF tick (Andy Simper's form)
  float a1 = 1.0f / (1.0f + g * (g + k));
  float a2 = g * a1;
  float a3 = g * a2;

  float v3 = input - ic2eq_;
  float v1 = a1 * ic1eq_ + a2 * v3;
  float v2 = ic2eq_ + a2 * ic1eq_ + a3 * v3;

  // Update state
  ic1eq_ = 2.0f * v1 - ic1eq_;
  ic2eq_ = 2.0f * v2 - ic2eq_;

  // IG00156 simultaneous outputs
  Outputs out;
  out.lp = v2;
  out.bp = v1;
  out.hp = input - k * v1 - v2;
  return out;
}
