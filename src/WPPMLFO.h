/*
  ==============================================================================
    WPPMLFO.h
    Low Frequency Oscillator with multiple waveforms.
    Destinations: d (duration), filter cutoff, pitch.
  ==============================================================================
*/

#pragma once

#include <cmath>

class WPPMLFO {
 public:
  enum class Shape : uint8_t { Sine = 0, Triangle, Saw, Square, NumShapes };

  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    phase_ = 0.0;
  }

  void reset() { phase_ = 0.0; }

  void setRate(float hz) {
    rate_ = hz;
    phaseInc_ = static_cast<double>(hz) / sampleRate_;
  }

  void setDepth(float depth) { depth_ = depth; }
  void setShape(Shape s) { shape_ = s; }

  // Returns bipolar value [-1, +1] * depth
  float processSample() {
    float raw = 0.0f;

    switch (shape_) {
      case Shape::Sine:
        raw = std::sin(kTwoPi * static_cast<float>(phase_));
        break;

      case Shape::Triangle: {
        float p = static_cast<float>(phase_);
        raw = (p < 0.5f) ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
        break;
      }

      case Shape::Saw:
        raw = 2.0f * static_cast<float>(phase_) - 1.0f;
        break;

      case Shape::Square:
        raw = (phase_ < 0.5) ? 1.0f : -1.0f;
        break;

      default:
        break;
    }

    // Advance phase
    phase_ += phaseInc_;
    if (phase_ >= 1.0) phase_ -= 1.0;

    return raw * depth_;
  }

  // Get current value without advancing (for display)
  float getCurrentValue() const {
    float raw = 0.0f;
    switch (shape_) {
      case Shape::Sine:
        raw = std::sin(kTwoPi * static_cast<float>(phase_));
        break;
      case Shape::Triangle: {
        float p = static_cast<float>(phase_);
        raw = (p < 0.5f) ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
        break;
      }
      case Shape::Saw:
        raw = 2.0f * static_cast<float>(phase_) - 1.0f;
        break;
      case Shape::Square:
        raw = (phase_ < 0.5) ? 1.0f : -1.0f;
        break;
      default:
        break;
    }
    return raw * depth_;
  }

 private:
  static constexpr float kTwoPi = 6.283185307179586f;

  double sampleRate_ = 44100.0;
  double phase_ = 0.0;
  double phaseInc_ = 0.0;

  float rate_ = 1.0f;
  float depth_ = 0.0f;
  Shape shape_ = Shape::Sine;
};
