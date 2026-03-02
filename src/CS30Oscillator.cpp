/*
  ==============================================================================
    CS30Oscillator.cpp
    Yamaha CS-30 VCO emulation implementation.
  ==============================================================================
*/

#include "CS30Oscillator.h"

#include <cmath>

//==============================================================================
// GermaniumDiodeClipper
//==============================================================================

GermaniumDiodeClipper::GermaniumDiodeClipper() = default;

void GermaniumDiodeClipper::prepare(double sampleRate) {
  C1.prepare(static_cast<float>(sampleRate));
}

void GermaniumDiodeClipper::reset() { C1.reset(); }

float GermaniumDiodeClipper::processSample(float input) {
  Vs.setVoltage(input);
  dp.incident(P1.reflected());
  P1.incident(dp.reflected());
  return chowdsp::wdft::voltage<float>(C1);
}

//==============================================================================
// CS30Oscillator
//==============================================================================

void CS30Oscillator::prepare(double sampleRate) {
  sampleRate_ = sampleRate;
  phase_ = 0.0;
  driftCounter_ = 0;
  driftSmoothed_ = 0.0f;
  driftTarget_ = 0.0f;
}

void CS30Oscillator::setFrequency(float freq) {
  phaseInc_ = static_cast<double>(freq) / sampleRate_;
}

void CS30Oscillator::setWaveform(Waveform w) { waveform_ = w; }

void CS30Oscillator::setPulseWidth(float pw) {
  pulseWidth_ = juce::jlimit(0.05f, 0.95f, pw);
}

void CS30Oscillator::reset() {
  phase_ = 0.0;
  driftSmoothed_ = 0.0f;
  driftTarget_ = 0.0f;
  driftCounter_ = 0;
}

float CS30Oscillator::polyBlep(double t, double dt) {
  if (t < dt) {
    auto tn = t / dt;
    return static_cast<float>(tn + tn - tn * tn - 1.0);
  }
  if (t > 1.0 - dt) {
    auto tn = (t - 1.0) / dt;
    return static_cast<float>(tn * tn + tn + tn + 1.0);
  }
  return 0.0f;
}

float CS30Oscillator::generateSaw() {
  // 2SK30A JFET thermal drift: random pitch micro-variations
  if (--driftCounter_ <= 0) {
    driftTarget_ = (driftRng_.nextFloat() - 0.5f) * 0.001f;  // ±0.05%
    driftCounter_ = static_cast<int>(sampleRate_ * 0.05);    // update at 20 Hz
  }
  driftSmoothed_ += (driftTarget_ - driftSmoothed_) * 0.001f;

  double effectiveInc = phaseInc_ * (1.0 + static_cast<double>(driftSmoothed_));

  phase_ += effectiveInc;
  if (phase_ >= 1.0) {
    phase_ -= 1.0;
  }

  // Naive saw with PolyBLEP correction at the discontinuity
  auto raw = static_cast<float>(2.0 * phase_ - 1.0);
  raw -= polyBlep(phase_, effectiveInc);
  return raw;
}

float CS30Oscillator::sawToSquare(float /*saw*/) const {
  // IG00158 WSC: comparator derives square/pulse from saw phase ramp.
  // Pulse width set by external CV to pin 9 threshold.
  float value = (phase_ < static_cast<double>(pulseWidth_)) ? 1.0f : -1.0f;

  // PolyBLEP at rising edge (phase=0)
  value += polyBlep(phase_, phaseInc_);

  // PolyBLEP at falling edge (phase=pw)
  double pwPhase = phase_ - static_cast<double>(pulseWidth_);
  if (pwPhase < 0.0) {
    pwPhase += 1.0;
  }
  value -= polyBlep(pwPhase, phaseInc_);

  return value;
}

float CS30Oscillator::generateTriangle() {
  // Phase-based triangle: linear ramp up then down.
  // 0→0.5: rises from -1 to +1; 0.5→1.0: falls from +1 to -1.
  float p = static_cast<float>(phase_);
  float tri;
  if (p < 0.5f) {
    tri = 4.0f * p - 1.0f;  // -1 → +1
  } else {
    tri = 3.0f - 4.0f * p;  // +1 → -1
  }
  return tri;
}

float CS30Oscillator::processSample() {
  float saw = generateSaw();

  switch (waveform_) {
    case Waveform::Saw:
      return saw;
    case Waveform::Square:
      return sawToSquare(saw);
    case Waveform::Triangle:
      return generateTriangle();
    case Waveform::Sine:
      return std::sin(static_cast<float>(phase_) *
                      juce::MathConstants<float>::twoPi);
    case Waveform::Noise:
      return noiseRng_.nextFloat() * 2.0f - 1.0f;
  }

  return saw;
}
