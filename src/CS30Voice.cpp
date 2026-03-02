/*
  ==============================================================================
    CS30Voice.cpp
    Yamaha CS-30 full signal path voice implementation.

    Signal flow per sample:
      VCO 2 → FM clipper → VCO 1 freq mod
      VCO 1 + VCO 2 → mix → VCF 1 (all outputs) + VCF 2 (all outputs)
      VCA 1: VCF1(HP/BP/LP sel) + VCF2(LP) + VCO1(direct) + LFO tremolo
      VCA 2: Normal/Ring-mod blend + LFO tremolo + EG2 amplitude
      Output: stereo pan (balance) × volume × velocity
  ==============================================================================
*/

#include "CS30Voice.h"

#include <cmath>

CS30Voice::CS30Voice() = default;

bool CS30Voice::canPlaySound(juce::SynthesiserSound* sound) {
  return dynamic_cast<CS30Sound*>(sound) != nullptr;
}

void CS30Voice::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
  vco1_.prepare(sampleRate);
  vco2_.prepare(sampleRate);
  vcf1_.prepare(sampleRate);
  vcf2_.prepare(sampleRate);
  eg1_.prepare(sampleRate);
  eg2_.prepare(sampleRate);
  eg3_.prepare(sampleRate);
  fmClipper_.prepare(sampleRate);
  prepared_ = true;
}

//==============================================================================
// Helpers

float CS30Voice::feetToMultiplier(int feetIndex) {
  // 0=32' (×0.25), 1=16' (×0.5), 2=8' (×1), 3=4' (×2), 4=2' (×4)
  constexpr float kMul[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
  if (feetIndex >= 0 && feetIndex < 5) {
    return kMul[feetIndex];
  }
  return 1.0f;
}

float CS30Voice::getEnvelopeValue(int egSel) const {
  switch (egSel) {
    case 0:
      return eg1Val_;
    case 1:
      return eg2Val_;
    case 2:
      return eg3Val_;
    default:
      return eg1Val_;
  }
}

float CS30Voice::getLFOValue(CS30LFO::Waveform w) const {
  auto idx = static_cast<int>(w);
  float raw = (idx >= 0 && idx < 5) ? lfoValues_[idx] : 0.0f;

  // EG gating: lfoEGDepth controls how much the selected EG
  // modulates the LFO amplitude (0 = full LFO, 10 = fully EG-gated).
  float depth = params_.lfoEGDepth / 10.0f;
  if (depth > 0.001f) {
    float egVal = getEnvelopeValue(params_.lfoEGSel);
    raw *= (1.0f - depth) + depth * egVal;
  }
  return raw;
}

//==============================================================================
void CS30Voice::startNote(int midiNoteNumber, float velocity,
                          juce::SynthesiserSound* /*sound*/,
                          int currentPitchWheelPosition) {
  velocity_ = velocity;
  targetFrequency_ =
      static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
  noteFrequency_ = targetFrequency_;

  float pitchBend =
      (static_cast<float>(currentPitchWheelPosition) - 8192.0f) / 8192.0f;
  pitchWheelSemitones_ = pitchBend * params_.pbRange;

  // Portamento: snap immediately when off
  if (params_.portamento < 0.01f) {
    currentFrequency_ = targetFrequency_;
  }

  // Reset oscillators and filters on new note
  vco1_.reset();
  vco2_.reset();
  vcf1_.reset();
  vcf2_.reset();
  fmClipper_.reset();

  // Trigger envelopes based on trigger type
  if (params_.trigMulti) {
    eg1_.noteOn();
    eg2_.noteOn();
    eg3_.noteOn();
  } else {
    // Single/legato: only trigger if not already active
    if (!eg1_.isActive()) {
      eg1_.noteOn();
    }
    if (!eg2_.isActive()) {
      eg2_.noteOn();
    }
    if (!eg3_.isActive()) {
      eg3_.noteOn();
    }
  }

  noteHeld_ = true;
  lfoGateHigh_ = false;  // reset LFO gate tracking for new note
}

void CS30Voice::retriggerNote(int midiNoteNumber, float velocity) {
  velocity_ = velocity;
  targetFrequency_ =
      static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
  noteFrequency_ = targetFrequency_;

  // Portamento: snap immediately when off
  if (params_.portamento < 0.01f) {
    currentFrequency_ = targetFrequency_;
  }

  // DO NOT reset oscillators, filters, or clipper — smooth transition.

  // Retrigger envelopes (soft retrigger — noteOn doesn't reset level)
  if (params_.trigMulti) {
    eg1_.noteOn();
    eg2_.noteOn();
    eg3_.noteOn();
  }
  // In single/legato mode, envelopes keep running without retrigger.

  noteHeld_ = true;
  lfoGateHigh_ = false;
}

void CS30Voice::stopNote(float /*velocity*/, bool allowTailOff) {
  noteHeld_ = false;

  if (allowTailOff) {
    eg1_.noteOff();
    eg2_.noteOff();
    eg3_.noteOff();
  } else {
    eg1_.reset();
    eg2_.reset();
    eg3_.reset();
    clearCurrentNote();
  }
}

void CS30Voice::pitchWheelMoved(int newPitchWheelValue) {
  float pitchBend =
      (static_cast<float>(newPitchWheelValue) - 8192.0f) / 8192.0f;
  pitchWheelSemitones_ = pitchBend * params_.pbRange;
}

void CS30Voice::controllerMoved(int /*controllerNumber*/,
                                int /*newControllerValue*/) {
  // Reserved for MIDI CC handling
}

//==============================================================================
void CS30Voice::setParams(const CS30VoiceParams& p) {
  params_ = p;
  eg1_.setParameters(p.eg1Params);
  eg2_.setParameters(p.eg2Params);
  eg3_.setParameters(p.eg3Params);

  // VCO waveforms
  vco1_.setWaveform(p.vco1Func);
  vco2_.setWaveform(p.vco2Func);

  // VCF resonance and drive (constant per block)
  vcf1_.setResonance(p.vcf1Reso / 10.0f);
  vcf2_.setResonance(p.vcf2Reso / 10.0f);
  vcf1_.setDrive(0.5f);
  vcf2_.setDrive(0.5f);

  // Portamento rate (exponential smoothing coefficient)
  float sr = static_cast<float>(getSampleRate());
  if (sr < 1.0f) {
    sr = 44100.0f;
  }
  if (p.portamento < 0.01f) {
    portamentoRate_ = 1.0f;  // instant
  } else {
    float glideTime = p.portamento * 0.3f;  // 0–10 → 0–3 seconds
    portamentoRate_ = 1.0f - std::exp(-1.0f / (glideTime * sr));
  }
}

void CS30Voice::setLFOValues(float sine, float tri, float saw, float square,
                             float sh) {
  lfoValues_[0] = sine;
  lfoValues_[1] = tri;
  lfoValues_[2] = saw;
  lfoValues_[3] = square;
  lfoValues_[4] = sh;
}

//==============================================================================
void CS30Voice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples) {
  // LFO trigger mode: use LFO square wave as a repeating gate.
  // Rising edge → retrigger envelopes, falling edge → release envelopes.
  // Only active while note key is held.
  if (params_.trigLFO && noteHeld_) {
    bool gateNow =
        (lfoValues_[static_cast<int>(CS30LFO::Waveform::Square)] > 0.0f);
    if (gateNow && !lfoGateHigh_) {
      // Rising edge: retrigger envelopes (soft — level continues from current)
      eg1_.noteOn();
      eg2_.noteOn();
      eg3_.noteOn();
    } else if (!gateNow && lfoGateHigh_) {
      // Falling edge: release envelopes (they will decay/release)
      eg1_.noteOff();
      eg2_.noteOff();
      eg3_.noteOff();
    }
    lfoGateHigh_ = gateNow;
  }

  // If amplitude envelope (EG2) is done, voice is complete
  if (!eg2_.isActive()) {
    if (!eg1_.isActive() && !eg3_.isActive()) {
      clearCurrentNote();
    }
    return;
  }

  for (int i = 0; i < numSamples; ++i) {
    // ── Portamento frequency smoothing ──
    currentFrequency_ +=
        portamentoRate_ * (targetFrequency_ - currentFrequency_);

    // ── Envelope generators (all three) ──
    eg1Val_ = eg1_.processSample();
    eg2Val_ = eg2_.processSample();
    eg3Val_ = eg3_.processSample();

    // ── Pitch modulation from selected EG + pitch bend + sequencer CV ──
    float pitchEGVal = getEnvelopeValue(params_.pitchEGSel);
    float pitchMod =
        (params_.pitchEGDepth / 10.0f) * pitchEGVal * 4.0f;  // ±4 oct
    float pitchBendMod = pitchWheelSemitones_ / 12.0f;

    // Sequencer CV in semitones (±24), convert to octaves
    float seqPitchMod = params_.sequencerCV / 12.0f;

    // Base frequency for SEQ mode (C4 = 261.63 Hz, fixed pitch)
    constexpr float kSeqBaseFreq = 261.63f;

    // ── VCO 2 (generate first — FM source for VCO 1) ──
    float vco2Feet = feetToMultiplier(params_.vco2Feet);
    float vco2Base = params_.vco2Seq ? kSeqBaseFreq : currentFrequency_;
    float vco2SeqMod = params_.vco2Seq ? seqPitchMod : 0.0f;
    float vco2Freq =
        vco2Base * vco2Feet *
        std::exp2(params_.pitchTune / 12.0f + params_.pitchDetune / 24.0f +
                  pitchMod + pitchBendMod + vco2SeqMod);

    float lfoTriangle = getLFOValue(CS30LFO::Waveform::Triangle);
    float pw2 = juce::jlimit(
        0.05f, 0.95f, params_.vco2PW + lfoTriangle * (params_.vco2PWM / 10.0f));

    vco2_.setFrequency(vco2Freq);
    vco2_.setPulseWidth(pw2);
    float osc2 = vco2_.processSample();

    // ── VCO 1 (with FM from VCO 2 through 1N34A germanium clipper) ──
    float vco1Feet = feetToMultiplier(params_.vco1Feet);
    float fmMod = fmClipper_.processSample(osc2);
    float fmAmount = params_.vco1ModVCO2 / 10.0f;

    float vco1Base = params_.vco1Seq ? kSeqBaseFreq : currentFrequency_;
    float vco1SeqMod = params_.vco1Seq ? seqPitchMod : 0.0f;
    float vco1Freq =
        vco1Base * vco1Feet *
        std::exp2(params_.pitchTune / 12.0f - params_.pitchDetune / 24.0f +
                  pitchMod + pitchBendMod + vco1SeqMod +
                  fmMod * fmAmount * 2.0f);

    float pw1 = juce::jlimit(
        0.05f, 0.95f, params_.vco1PW + lfoTriangle * (params_.vco1PWM / 10.0f));

    vco1_.setFrequency(vco1Freq);
    vco1_.setPulseWidth(pw1);
    float osc1 = vco1_.processSample();

    // ── VCO Mix (feeding both VCFs in parallel) ──
    // Seq auto-voice: mute VCOs not in SEQ mode (no KBD drone).
    // Keyboard voice: mute VCOs in SEQ mode (no seq bleed into KBD).
    float vco1Gain = params_.vco1Depth / 10.0f;
    float vco2Gain = params_.vco2Depth / 10.0f;

    // Per-VCO amplitude envelope (each VCO has its own selectable EG)
    vco1Gain *= getEnvelopeValue(params_.vco1EGSel);
    vco2Gain *= getEnvelopeValue(params_.vco2EGSel);

    if (isSeqAutoVoice_) {
      if (!params_.vco1Seq) {
        vco1Gain = 0.0f;
      }
      if (!params_.vco2Seq) {
        vco2Gain = 0.0f;
      }
    } else {
      // Keyboard-triggered voice: silence any VCO set to SEQ
      if (params_.vco1Seq) {
        vco1Gain = 0.0f;
      }
      if (params_.vco2Seq) {
        vco2Gain = 0.0f;
      }
    }
    float vcoMix = osc1 * vco1Gain + osc2 * vco2Gain;

    // ── VCF 1: IG00156 state-variable filter ──
    float vcf1EGVal = getEnvelopeValue(params_.vcf1EGSel);
    float vcf1LFO = getLFOValue(params_.vcf1ModFunc);
    float vcf1CutMod =
        params_.vcf1Cutoff *
        std::exp2(vcf1EGVal * (params_.vcf1EGDepth / 10.0f) * 4.0f +
                  vcf1LFO * (params_.vcf1ModDepth / 10.0f) * 2.0f);
    vcf1_.setCutoff(vcf1CutMod);
    vcf1_.setKeyboardFollow(params_.vcf1KBDFollow / 10.0f, noteFrequency_);

    float vcf1In = vcoMix * (params_.vcf1Level / 10.0f);
    CS30Filter::Outputs vcf1Out = vcf1_.processSampleAll(vcf1In);

    // ── VCF 2: IG00156 state-variable filter ──
    float vcf2EGVal = getEnvelopeValue(params_.vcf2EGSel);
    float vcf2LFO = getLFOValue(params_.vcf2ModFunc);
    float vcf2CutMod =
        params_.vcf2Cutoff *
        std::exp2(vcf2EGVal * (params_.vcf2EGDepth / 10.0f) * 4.0f +
                  vcf2LFO * (params_.vcf2ModDepth / 10.0f) * 2.0f);
    vcf2_.setCutoff(vcf2CutMod);
    vcf2_.setKeyboardFollow(params_.vcf2KBDFollow / 10.0f, noteFrequency_);

    float vcf2In = vcoMix * (params_.vcf2Level / 10.0f);
    CS30Filter::Outputs vcf2Out = vcf2_.processSampleAll(vcf2In);

    // ── VCA 1: Signal mixer (IG00151) ──
    // Select HP/BP/LP tap from VCF 1
    float vcf1Sel = vcf1Out.lp;
    switch (params_.vca1FilterType) {
      case CS30Filter::Type::HighPass:
        vcf1Sel = vcf1Out.hp;
        break;
      case CS30Filter::Type::BandPass:
        vcf1Sel = vcf1Out.bp;
        break;
      case CS30Filter::Type::LowPass:
        vcf1Sel = vcf1Out.lp;
        break;
    }

    float vca1Audio = vcf1Sel * (params_.vca1VCF1 / 10.0f) +
                      vcf2Out.lp * (params_.vca1VCF2 / 10.0f) +
                      osc1 * (params_.vca1VCO1 / 10.0f);

    // VCA 1 LFO amplitude modulation (tremolo)
    float vca1LFO = getLFOValue(params_.vca1ModFunc);
    vca1Audio *= 1.0f + vca1LFO * (params_.vca1ModDepth / 10.0f);

    // ── VCA 2: Normal / Ring-Mod blend + amplitude envelope (IG00151) ──
    float ringCarrier =
        (params_.vca2RMOSrc == 0) ? osc1 : getLFOValue(CS30LFO::Waveform::Sine);
    float ringMod = vca1Audio * ringCarrier;

    float vca2Audio = vca1Audio * (1.0f - params_.vca2NormMod) +
                      ringMod * params_.vca2NormMod;

    // VCA 2 LFO amplitude modulation
    float vca2LFO = getLFOValue(params_.vca2ModFunc);
    vca2Audio *= 1.0f + vca2LFO * (params_.vca2ModDepth / 10.0f);

    // Per-VCO EG amplitude is already applied at the mix stage,
    // so no additional hardwired EG2 multiplication here.

    // ── Output: balance (L/R pan) × volume × velocity ──
    float gain = (params_.outVolume / 10.0f) * velocity_;
    float outSample = vca2Audio * gain;

    int numCh = outputBuffer.getNumChannels();
    if (numCh == 1) {
      outputBuffer.addSample(0, startSample + i, outSample);
    } else if (numCh >= 2) {
      // Constant-power stereo pan from balance (0=left, 0.5=center, 1=right)
      float angle = params_.outBalance * juce::MathConstants<float>::halfPi;
      outputBuffer.addSample(0, startSample + i, outSample * std::cos(angle));
      outputBuffer.addSample(1, startSample + i, outSample * std::sin(angle));
    }
  }

  // Release complete — free the voice
  if (!eg2_.isActive()) {
    clearCurrentNote();
  }
}
