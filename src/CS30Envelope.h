/*
  ==============================================================================
    CS30Envelope.h
    Yamaha CS-30 Envelope Generator emulation (header-only).

    Three identical EGs (EG1 = IG00152, EG2/EG3 = IG00159).
    Front-panel controls are standard ADSR per envelope.
    Exponential RC charge/discharge curves through the 2SC458/2SA561
    transistor switching network on the SEQ board.
  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

class CS30Envelope {
 public:
  struct Parameters {
    float attackTime = 0.01f;   // seconds
    float decayTime = 0.3f;     // seconds
    float sustainLevel = 0.7f;  // 0–1
    float releaseTime = 0.5f;   // seconds
  };

  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    reset();
  }

  void setParameters(const Parameters& p) {
    params_ = p;
    calculateRates();
  }

  void noteOn() {
    stage_ = Stage::Attack;
    // Don't reset currentLevel_ — allows soft retrigger from current
    // amplitude (prevents clicks during LFO retriggering and is more
    // analog-authentic for multi-trigger mode).
  }

  void noteOff() {
    if (stage_ != Stage::Idle) {
      stage_ = Stage::Release;
    }
  }

  void reset() {
    stage_ = Stage::Idle;
    currentLevel_ = 0.0f;
  }

  bool isActive() const { return stage_ != Stage::Idle; }

  float processSample() {
    switch (stage_) {
      case Stage::Idle:
        return 0.0f;

      case Stage::Attack: {
        // Exponential charge with 1.3× overshoot (analog RC behavior)
        float target = 1.3f;
        currentLevel_ += attackRate_ * (target - currentLevel_);
        if (currentLevel_ >= 0.999f) {
          currentLevel_ = 1.0f;
          stage_ = Stage::Decay;
        }
        break;
      }

      case Stage::Decay: {
        // Hybrid: exponential convergence + minimum linear step
        // Ensures audible decay even with long time constants
        float delta = params_.sustainLevel - currentLevel_;
        float expStep = decayRate_ * delta;
        float linStep =
            -decayLinRate_;  // always negative (approaching sustain from above)
        // Use whichever covers more ground (both are negative when above
        // sustain)
        if (currentLevel_ > params_.sustainLevel) {
          currentLevel_ += std::min(
              expStep, linStep);  // both negative; min = larger magnitude
          currentLevel_ = std::max(currentLevel_, params_.sustainLevel);
        } else {
          currentLevel_ = params_.sustainLevel;
        }
        if (std::abs(currentLevel_ - params_.sustainLevel) < 0.0005f) {
          currentLevel_ = params_.sustainLevel;
          stage_ = Stage::Sustain;
        }
        break;
      }

      case Stage::Sustain:
        currentLevel_ = params_.sustainLevel;
        break;

      case Stage::Release: {
        // Hybrid: exponential convergence + minimum linear step
        float expStep = releaseRate_ * (0.0f - currentLevel_);
        float linStep = -releaseLinRate_;  // always negative
        currentLevel_ += std::min(expStep, linStep);
        if (currentLevel_ <= 0.0005f) {
          currentLevel_ = 0.0f;
          stage_ = Stage::Idle;
        }
        break;
      }
    }

    return currentLevel_;
  }

 private:
  enum class Stage : std::uint8_t { Idle, Attack, Decay, Sustain, Release };

  Stage stage_ = Stage::Idle;
  float currentLevel_ = 0.0f;
  Parameters params_;
  double sampleRate_ = 44100.0;

  float attackRate_ = 0.0f;
  float decayRate_ = 0.0f;
  float releaseRate_ = 0.0f;
  float decayLinRate_ = 0.0f;    // linear fallback: 1/(time*sr)
  float releaseLinRate_ = 0.0f;  // linear fallback

  void calculateRates() {
    auto timeToRate = [this](float timeSec) -> float {
      if (timeSec < 0.001f) {
        return 1.0f;
      }
      return 1.0f -
             std::exp(-1.0f / (timeSec * static_cast<float>(sampleRate_)));
    };

    auto timeToLinRate = [this](float timeSec) -> float {
      if (timeSec < 0.001f) {
        return 1.0f;
      }
      return 1.0f / (timeSec * static_cast<float>(sampleRate_));
    };

    attackRate_ = timeToRate(params_.attackTime);
    decayRate_ = timeToRate(params_.decayTime);
    releaseRate_ = timeToRate(params_.releaseTime);
    decayLinRate_ = timeToLinRate(params_.decayTime);
    releaseLinRate_ = timeToLinRate(params_.releaseTime);
  }
};
