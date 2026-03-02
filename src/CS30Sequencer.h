/*
  ==============================================================================
    CS30Sequencer.h
    Yamaha CS-30 step sequencer emulation (header-only).

    8-step sequencer with clock speed, start/stop, hold, and per-step
    knob values. Outputs a CV value (0–1) from the current step.
    Runs globally (not per-voice) from the processor.
  ==============================================================================
*/

#pragma once

class CS30Sequencer {
 public:
  static constexpr int kMaxSteps = 8;

  void prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    reset();
  }

  void setClockSpeed(float hz) {
    phaseInc_ = static_cast<double>(hz) / sampleRate_;
  }

  void setRunning(bool run) { running_ = run; }
  void setHold(bool hold) { hold_ = hold; }

  void setNumSteps(int n) {
    if (n >= 1 && n <= kMaxSteps) {
      numSteps_ = n;
      if (currentStep_ >= numSteps_) {
        currentStep_ = 0;
      }
    }
  }

  void setStepValue(int step, float value) {
    if (step >= 0 && step < kMaxSteps) {
      stepValues_[step] = value;
    }
  }

  void reset() {
    phase_ = 0.0;
    currentStep_ = 0;
    currentValue_ = stepValues_[0];
  }

  // Advance by one sample. Call once per sample from the processor.
  void advance() {
    if (!running_ || hold_) {
      return;
    }

    phase_ += phaseInc_;
    if (phase_ >= 1.0) {
      phase_ -= 1.0;
      currentStep_ = (currentStep_ + 1) % numSteps_;
      currentValue_ = stepValues_[currentStep_];
    }
  }

  float getCurrentValue() const { return currentValue_; }
  int getCurrentStep() const { return currentStep_; }

 private:
  double sampleRate_ = 44100.0;
  double phase_ = 0.0;
  double phaseInc_ = 0.0;
  bool running_ = false;
  bool hold_ = false;
  int numSteps_ = 8;
  int currentStep_ = 0;
  float currentValue_ = 0.0f;
  float stepValues_[kMaxSteps] = {0.5f, 0.5f, 0.5f, 0.5f,
                                  0.5f, 0.5f, 0.5f, 0.5f};
};
