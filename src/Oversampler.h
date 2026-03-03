/*
  ==============================================================================
    Oversampler.h
    Simple 2x/4x/8x oversampling with FIR anti-alias filtering.

    Modes:
      0 = Eco   — no oversampling (paper's d-windowing only)
      1 = High  — 4x oversampling
      2 = Ultra — 8x oversampling
  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

class Oversampler {
 public:
  // Quality modes
  static constexpr int kEco = 0;
  static constexpr int kHigh = 1;
  static constexpr int kUltra = 2;

  void prepare(double baseSampleRate, int mode) {
    mode_ = std::clamp(mode, 0, 2);
    baseSampleRate_ = baseSampleRate;
    factor_ = (mode_ == kEco) ? 1 : (mode_ == kHigh ? 4 : 8);
    effectiveSampleRate_ = baseSampleRate * static_cast<double>(factor_);

    // Build half-band FIR kernel for downsampling
    // Using a windowed-sinc lowpass at Nyquist/factor
    buildKernel();
    std::fill(filterState_.begin(), filterState_.end(), 0.0f);
  }

  int getFactor() const noexcept { return factor_; }
  double getEffectiveSampleRate() const noexcept {
    return effectiveSampleRate_;
  }

  void reset() { std::fill(filterState_.begin(), filterState_.end(), 0.0f); }

  // Downsample: given `factor_` oversampled values in `in`, return one output
  // sample. For Eco mode (factor=1), just returns in[0].
  float downsample(const float* in, int count) {
    if (factor_ <= 1) {
      return in[0];
    }

    // FIR polyphase decimation
    // Simple approach: filter then decimate
    // Feed all oversampled samples through the FIR, take last output
    float result = 0.0f;
    for (int i = 0; i < count; ++i) {
      // Shift state
      for (int j = kFilterLen - 1; j > 0; --j) {
        filterState_[static_cast<size_t>(j)] =
            filterState_[static_cast<size_t>(j - 1)];
      }
      filterState_[0] = in[i];

      // Only compute on the last sample (decimation)
      if (i == count - 1) {
        for (int j = 0; j < kFilterLen; ++j) {
          result += filterState_[static_cast<size_t>(j)] *
                    kernel_[static_cast<size_t>(j)];
        }
      }
    }

    return result;
  }

 private:
  static constexpr int kFilterLen = 31;  // FIR taps (odd for symmetric)

  void buildKernel() {
    if (factor_ <= 1) {
      return;
    }

    // Windowed-sinc lowpass, cutoff = 1/(2*factor) of oversampled rate
    // i.e. passband = base Nyquist
    float fc = 1.0f / static_cast<float>(2 * factor_);
    int M = kFilterLen - 1;
    float sum = 0.0f;

    for (int n = 0; n < kFilterLen; ++n) {
      float x = static_cast<float>(n) - static_cast<float>(M) / 2.0f;

      // sinc
      float sinc;
      if (std::abs(x) < 1.0e-6f) {
        sinc = 2.0f * fc;
      } else {
        sinc = std::sin(2.0f * kPi * fc * x) / (kPi * x);
      }

      // Blackman window
      float w = 0.42f -
                0.5f * std::cos(2.0f * kPi * static_cast<float>(n) /
                                static_cast<float>(M)) +
                0.08f * std::cos(4.0f * kPi * static_cast<float>(n) /
                                 static_cast<float>(M));

      kernel_[static_cast<size_t>(n)] = sinc * w;
      sum += kernel_[static_cast<size_t>(n)];
    }

    // Normalize for unity gain at DC
    for (int n = 0; n < kFilterLen; ++n) {
      kernel_[static_cast<size_t>(n)] /= sum;
    }

    // Scale by factor so decimation doesn't lose energy
    for (int n = 0; n < kFilterLen; ++n) {
      kernel_[static_cast<size_t>(n)] *= static_cast<float>(factor_);
    }
  }

  static constexpr float kPi = 3.14159265358979323846f;

  int mode_ = kEco;
  int factor_ = 1;
  double baseSampleRate_ = 44100.0;
  double effectiveSampleRate_ = 44100.0;

  std::array<float, kFilterLen> kernel_{};
  std::array<float, kFilterLen> filterState_{};
};
