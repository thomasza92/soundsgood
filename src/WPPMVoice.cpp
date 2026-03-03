/*
  ==============================================================================
    WPPMVoice.cpp
    WPPM synthesiser voice implementation.

    Signal chain (per voice):
      LFO → modulation matrix → Osc (stereo, oversampled) → SVF Filter
        → ADSR env → Diode clipper → Volume → Output
  ==============================================================================
*/

#include "WPPMVoice.h"

#include <cmath>

//==============================================================================
WPPMVoice::WPPMVoice() = default;

bool WPPMVoice::canPlaySound(juce::SynthesiserSound* sound) {
  return dynamic_cast<WPPMSound*>(sound) != nullptr;
}

//==============================================================================
void WPPMVoice::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
  currentSampleRate_ = sampleRate;

  osc_.prepare(sampleRate);
  lfo_.prepare(sampleRate);
  lfo2_.prepare(sampleRate);
  filterL_.reset();
  filterR_.reset();
  analogStageL_.prepare(sampleRate);
  analogStageR_.prepare(sampleRate);
  oversamplerL_.prepare(sampleRate, params_.oversampleMode);
  oversamplerR_.prepare(sampleRate, params_.oversampleMode);
  currentOversampleMode_ = params_.oversampleMode;

  adsr_.setSampleRate(sampleRate);
  env2_.setSampleRate(sampleRate);

  prepared_ = true;
}

//==============================================================================
void WPPMVoice::startNote(int midiNoteNumber, float velocity,
                          juce::SynthesiserSound* /*sound*/,
                          int currentPitchWheelPosition) {
  noteNumber_ = midiNoteNumber;
  velocity_ = velocity;
  aftertouch_ = 0.0f;
  noteFrequency_ =
      static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

  // Pitch wheel: ±2 semitones
  float pw =
      (static_cast<float>(currentPitchWheelPosition) - 8192.0f) / 8192.0f;
  pitchWheelSemitones_ = pw * 2.0f;

  osc_.reset();
  osc_.setFrequency(noteFrequency_ * std::exp2(pitchWheelSemitones_ / 12.0f));

  lfo_.reset();
  lfo2_.reset();
  filterL_.reset();
  filterR_.reset();
  analogStageL_.reset();
  analogStageR_.reset();
  oversamplerL_.reset();
  oversamplerR_.reset();

  // Re-prepare oversampler if mode changed
  if (currentOversampleMode_ != params_.oversampleMode) {
    oversamplerL_.prepare(currentSampleRate_, params_.oversampleMode);
    oversamplerR_.prepare(currentSampleRate_, params_.oversampleMode);
    currentOversampleMode_ = params_.oversampleMode;
  }

  adsr_.setParameters(adsrParams_);
  adsr_.noteOn();

  env2_.setParameters(env2Params_);
  env2_.noteOn();
}

//==============================================================================
void WPPMVoice::stopNote(float /*velocity*/, bool allowTailOff) {
  if (allowTailOff) {
    adsr_.noteOff();
    env2_.noteOff();
  } else {
    adsr_.reset();
    env2_.reset();
    clearCurrentNote();
  }
}

//==============================================================================
void WPPMVoice::pitchWheelMoved(int newPitchWheelValue) {
  float pw = (static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f;
  pitchWheelSemitones_ = pw * 2.0f;
}

void WPPMVoice::controllerMoved(int /*controllerNumber*/,
                                int /*newControllerValue*/) {
  // Could map CC1 (mod wheel) etc. here in the future
}

void WPPMVoice::aftertouchChanged(int newAftertouchValue) {
  aftertouch_ = static_cast<float>(newAftertouchValue) / 127.0f;
}

//==============================================================================
void WPPMVoice::setParams(const WPPMVoiceParams& p) {
  params_ = p;

  // ADSR
  adsrParams_.attack = p.attack;
  adsrParams_.decay = p.decay;
  adsrParams_.sustain = p.sustain;
  adsrParams_.release = p.release;
  adsr_.setParameters(adsrParams_);

  // LFO
  lfo_.setShape(p.lfoShape);
  lfo_.setRate(p.lfoRate);
  lfo_.setDepth(p.lfoDepth);

  // LFO2
  lfo2_.setShape(p.lfo2Shape);
  lfo2_.setRate(p.lfo2Rate);
  lfo2_.setDepth(p.lfo2Depth);

  // Envelope 2 (mod envelope)
  env2Params_.attack = p.env2Attack;
  env2Params_.decay = p.env2Decay;
  env2Params_.sustain = p.env2Sustain;
  env2Params_.release = p.env2Release;
  env2_.setParameters(env2Params_);

  // Analog stage
  analogStageL_.setParams(p.analogDrive, p.analogMix);
  analogStageR_.setParams(p.analogDrive, p.analogMix);

  // Re-prepare oversampler if mode changed during playback
  if (prepared_ && currentOversampleMode_ != p.oversampleMode) {
    oversamplerL_.prepare(currentSampleRate_, p.oversampleMode);
    oversamplerR_.prepare(currentSampleRate_, p.oversampleMode);
    currentOversampleMode_ = p.oversampleMode;
  }
}

//==============================================================================
void WPPMVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples) {
  if (!isVoiceActive()) return;

  int osFactor = oversamplerL_.getFactor();
  double osRate = oversamplerL_.getEffectiveSampleRate();

  for (int sample = 0; sample < numSamples; ++sample) {
    // --- LFO (runs at base rate) ---
    float lfoVal = lfo_.processSample();
    float lfo2Val = lfo2_.processSample();

    // --- ENV2 (mod envelope) ---
    float env2Val = env2_.getNextSample();

    // --- Modulation matrix ---
    // Start from base param values
    float modD = params_.d;
    float modI1 = params_.I1;
    float modI2 = params_.I2;
    float modR1 = params_.R1;
    float modR2 = params_.R2;
    float modCutoff = params_.filterCutoff;
    float freqMod = noteFrequency_ * std::exp2(pitchWheelSemitones_ / 12.0f);

    // Velocity modulation
    modD += velocity_ * params_.velToD * 0.4f;       // ±0.4 at max
    modI1 += velocity_ * params_.velToDepth * 5.0f;  // ±5 at max vel
    modI2 += velocity_ * params_.velToDepth * 5.0f;

    // Aftertouch modulation
    modD += aftertouch_ * params_.atToD * 0.4f;
    modI1 += aftertouch_ * params_.atToDepth * 5.0f;
    modI2 += aftertouch_ * params_.atToDepth * 5.0f;

    // LFO routing
    switch (params_.lfoDest) {
      case 0:  // d
        modD += lfoVal * 0.3f;
        break;
      case 1:                                   // Filter
        modCutoff *= std::exp2(lfoVal * 2.0f);  // ±2 octaves
        break;
      case 2:                                  // Pitch
        freqMod *= std::exp2(lfoVal / 12.0f);  // ±1 semitone
        break;
      case 3:  // Ratio (R1 + R2)
        modR1 += lfoVal * 2.0f;
        modR2 += lfoVal * 2.0f;
        break;
      default:
        break;
    }

    // LFO2 routing (same destinations)
    switch (params_.lfo2Dest) {
      case 0:
        modD += lfo2Val * 0.3f;
        break;
      case 1:
        modCutoff *= std::exp2(lfo2Val * 2.0f);
        break;
      case 2:
        freqMod *= std::exp2(lfo2Val / 12.0f);
        break;
      case 3:
        modR1 += lfo2Val * 2.0f;
        modR2 += lfo2Val * 2.0f;
        break;
      default:
        break;
    }

    // ENV2 routing (mod envelope)
    if (std::abs(params_.env2Amount) > 0.001f) {
      float envMod = env2Val * params_.env2Amount;
      switch (params_.env2Dest) {
        case 0:  // d
          modD += envMod * 0.4f;
          break;
        case 1:                                   // Filter
          modCutoff *= std::exp2(envMod * 4.0f);  // ±4 octaves
          break;
        case 2:  // Depth (I1+I2)
          modI1 += envMod * 5.0f;
          modI2 += envMod * 5.0f;
          break;
        case 3:  // Ratio (R1+R2)
          modR1 += envMod * 4.0f;
          modR2 += envMod * 4.0f;
          break;
        case 4:                          // Pitch
          freqMod *= std::exp2(envMod);  // ±1 octave
          break;
        default:
          break;
      }
    }

    // Clamp modulated values
    modD = std::clamp(modD, 0.01f, 0.99f);
    modI1 = std::max(modI1, 0.0f);
    modI2 = std::max(modI2, 0.0f);
    modR1 = std::max(modR1, 0.0f);
    modR2 = std::max(modR2, 0.0f);
    modCutoff = std::clamp(modCutoff, 20.0f, 20000.0f);

    // --- Build oscillator params ---
    WPPMOscillator::Params oscP;
    oscP.d = modD;
    oscP.R1 = modR1;
    oscP.I1 = modI1;
    oscP.P1 = params_.P1;
    oscP.R2 = modR2;
    oscP.I2 = modI2;
    oscP.P2 = params_.P2;
    oscP.subOctave = params_.subOctave;
    oscP.subMix = params_.subMix;
    oscP.unisonVoices = params_.unisonVoices;
    oscP.unisonDetune = params_.unisonDetune;
    oscP.unisonSpread = params_.unisonSpread;

    osc_.setParams(oscP);
    osc_.setFrequency(freqMod);

    // --- Oversampled oscillator rendering ---
    float rawL = 0.0f;
    float rawR = 0.0f;

    if (osFactor <= 1) {
      // Eco: no oversampling, just run at base rate
      auto stereo = osc_.processSample();
      rawL = stereo.left;
      rawR = stereo.right;
    } else {
      // Oversample: generate osFactor samples at higher rate, then downsample
      osc_.setSampleRate(osRate);
      osc_.setFrequency(freqMod);

      float osBufferL[8];  // max factor = 8
      float osBufferR[8];

      for (int os = 0; os < osFactor; ++os) {
        auto stereo = osc_.processSample();
        osBufferL[os] = stereo.left;
        osBufferR[os] = stereo.right;
      }

      // Restore base rate for next iteration
      osc_.setSampleRate(currentSampleRate_);

      rawL = oversamplerL_.downsample(osBufferL, osFactor);
      rawR = oversamplerR_.downsample(osBufferR, osFactor);
    }

    // --- Filter (stereo, at base rate) ---
    // Key-tracking: shift cutoff relative to note
    float keyTrackOctaves = (static_cast<float>(noteNumber_) - 60.0f) / 12.0f;
    float finalCutoff =
        modCutoff * std::exp2(keyTrackOctaves * params_.filterKeyTrack);

    // Envelope → filter
    float envVal = adsr_.getNextSample();
    finalCutoff *= std::exp2(envVal * params_.filterEnvAmount / 12.0f);
    finalCutoff = std::clamp(finalCutoff, 20.0f, 20000.0f);

    filterL_.process(rawL, finalCutoff, params_.filterResonance,
                     currentSampleRate_);
    filterR_.process(rawR, finalCutoff, params_.filterResonance,
                     currentSampleRate_);

    float filtL = 0.0f;
    float filtR = 0.0f;
    switch (params_.filterMode) {
      case FilterMode::LowPass:
        filtL = filterL_.getLp();
        filtR = filterR_.getLp();
        break;
      case FilterMode::HighPass:
        filtL = filterL_.getHp();
        filtR = filterR_.getHp();
        break;
      case FilterMode::BandPass:
        filtL = filterL_.getBp();
        filtR = filterR_.getBp();
        break;
    }

    // --- Envelope ---
    float outL = filtL * envVal;
    float outR = filtR * envVal;

    // --- Analog stage (stereo) ---
    if (params_.analogMix > 0.001f) {
      outL = analogStageL_.processSample(outL);
      outR = analogStageR_.processSample(outR);
    }

    // --- Volume ---
    outL *= params_.volume;
    outR *= params_.volume;

    // --- Write to output ---
    if (outputBuffer.getNumChannels() >= 2) {
      outputBuffer.addSample(0, startSample + sample, outL);
      outputBuffer.addSample(1, startSample + sample, outR);
    } else {
      outputBuffer.addSample(0, startSample + sample, (outL + outR) * 0.5f);
    }

    // Release tail done?
    if (!adsr_.isActive()) {
      clearCurrentNote();
      break;
    }
  }
}
