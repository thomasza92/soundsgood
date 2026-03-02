/*
  ==============================================================================
    CS30Oscillator.h
    Yamaha CS-30 VCO emulation.

    Models the IG00153 (VCO III) oscillator core and IG00158 (WSC) wave shape
    converter. Includes a WDF germanium diode clipper (1N34A) for the FM
    modulation path, buffered through an NJM4558D op-amp stage (IC4).
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <chowdsp_wdf/chowdsp_wdf.h>

#include <cstdint>

//==============================================================================
// Models the 1N34A germanium diode pair (D1, D2) in the FM modulation path.
// Circuit: Vs (4.7kΩ source) → C (47nF) ∥ DiodePair
// Germanium characteristics: Is=200nA, n=1.3, Vf≈0.3V (softer clip than Si).
class GermaniumDiodeClipper {
 public:
  GermaniumDiodeClipper();

  void prepare(double sampleRate);
  void reset();
  float processSample(float input);

 private:
  using Resistor = chowdsp::wdft::ResistiveVoltageSourceT<float>;
  using Capacitor = chowdsp::wdft::CapacitorT<float>;
  using Parallel = chowdsp::wdft::WDFParallelT<float, Capacitor, Resistor>;
  using Diodes = chowdsp::wdft::DiodePairT<float, Parallel>;

  // Members in strict construction order (Parallel references both above it)
  Resistor Vs{4700.0f};
  Capacitor C1{47.0e-9f};
  Parallel P1{C1, Vs};
  // 1N34A germanium: Is=200nA, Vt_eff = n*kT/q = 1.3*25.85mV ≈ 33.6mV
  Diodes dp{P1, 200.0e-9f, 33.605e-3f};
};

//==============================================================================
// Models the CS-30 VCO: IG00153 oscillator core + IG00158 Wave Shape Converter.
// Waveforms derived from a PolyBLEP saw (Pin 9 of WSC), matching the CS-30's
// signal flow: Saw → Pulse (Pin 7), Triangle (Pin 2), Double Triangle (Pin 14).
// Includes 2SK30A JFET thermal drift simulation for analog instability.
class CS30Oscillator {
 public:
  enum class Waveform : std::uint8_t { Saw = 0, Square, Triangle, Sine, Noise };

  void prepare(double sampleRate);
  void setFrequency(float freq);
  void setWaveform(Waveform w);
  void setPulseWidth(float pw);
  void reset();

  float processSample();

 private:
  double sampleRate_ = 44100.0;
  double phase_ = 0.0;
  double phaseInc_ = 0.0;
  float pulseWidth_ = 0.5f;
  Waveform waveform_ = Waveform::Saw;

  // 2SK30A JFET thermal drift simulation
  juce::Random driftRng_;
  float driftTarget_ = 0.0f;
  float driftSmoothed_ = 0.0f;
  int driftCounter_ = 0;

  // PolyBLEP anti-aliasing residual
  static float polyBlep(double t, double dt);

  // Core saw generator with drift
  float generateSaw();

  // IG00158 WSC waveform derivations
  float sawToSquare(float saw) const;
  float generateTriangle();

  // White noise via PRNG
  juce::Random noiseRng_;
};
