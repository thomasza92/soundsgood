/*
  ==============================================================================
    CS30LFO.h
    Yamaha CS-30 LFO emulation (header-only).

    Single LFO with multiple simultaneous waveform outputs.
    Each modulation point in the synth selects its own waveform via MOD
  FUNCTION. LFO amplitude can be modulated by a selected EG via lfoEGDepth.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include <cmath>
#include <cstdint>

class CS30LFO {
 public:
  enum class Waveform : std::uint8_t {
    Sine = 0,
    Triangle,
    Saw,
    Square,
    SampleHold
  };

  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    reset();
  }

  void setSpeed(float hz) { phaseInc_ = static_cast<double>(hz) / sampleRate_; }

  void reset() {
    phase_ = 0.0;
    shValue_ = 0.0f;
    prevPhaseWrap_ = false;
  }

  // Advance the LFO by one sample. Call once per sample from the processor.
  void advance() {
    phase_ += phaseInc_;
    bool wrapped = false;
    if (phase_ >= 1.0) {
      phase_ -= 1.0;
      wrapped = true;
    }

    // Sample & Hold: latch a new random value on each LFO cycle reset
    if (wrapped && !prevPhaseWrap_) {
      shValue_ = shRng_.nextFloat() * 2.0f - 1.0f;
    }
    prevPhaseWrap_ = wrapped;
  }

  // Get the current LFO output for a specific waveform shape.
  // Returns bipolar -1..+1.
  float getOutput(Waveform w) const {
    auto p = static_cast<float>(phase_);

    switch (w) {
      case Waveform::Sine:
        return std::sin(p * juce::MathConstants<float>::twoPi);

      case Waveform::Triangle:
        if (p < 0.25f) {
          return p * 4.0f;
        }
        if (p < 0.75f) {
          return 2.0f - p * 4.0f;
        }
        return p * 4.0f - 4.0f;

      case Waveform::Saw:
        return 2.0f * p - 1.0f;

      case Waveform::Square:
        return (p < 0.5f) ? 1.0f : -1.0f;

      case Waveform::SampleHold:
        return shValue_;
    }
    return 0.0f;
  }

 private:
  double sampleRate_ = 44100.0;
  double phase_ = 0.0;
  double phaseInc_ = 0.0;
  float shValue_ = 0.0f;
  bool prevPhaseWrap_ = false;
  juce::Random shRng_;
};
