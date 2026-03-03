/*
  ==============================================================================
    PluginProcessor.cpp
    Soundsgood -- WPPM synthesiser processor.
    27 parameters covering WPPM osc, sub, unison, filter, LFO, ADSR, analog
    stage, and output volume.
  ==============================================================================
*/

#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "WPPMVoice.h"

//==============================================================================
SoundsgoodAudioProcessor::SoundsgoodAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
#else
    :
#endif
      apvts_(*this, nullptr, "Parameters", createParameterLayout()) {
  // --- Cache all parameter pointers ---
  dParam = apvts_.getRawParameterValue("d");
  r1Param = apvts_.getRawParameterValue("R1");
  i1Param = apvts_.getRawParameterValue("I1");
  p1Param = apvts_.getRawParameterValue("P1");
  r2Param = apvts_.getRawParameterValue("R2");
  i2Param = apvts_.getRawParameterValue("I2");
  p2Param = apvts_.getRawParameterValue("P2");
  linkParam = apvts_.getRawParameterValue("link");

  subOctaveParam = apvts_.getRawParameterValue("subOctave");
  subMixParam = apvts_.getRawParameterValue("subMix");

  unisonVoicesParam = apvts_.getRawParameterValue("unisonVoices");
  unisonDetuneParam = apvts_.getRawParameterValue("unisonDetune");
  unisonSpreadParam = apvts_.getRawParameterValue("unisonSpread");

  filterModeParam = apvts_.getRawParameterValue("filterMode");
  filterCutoffParam = apvts_.getRawParameterValue("filterCutoff");
  filterResonanceParam = apvts_.getRawParameterValue("filterResonance");
  filterKeyTrackParam = apvts_.getRawParameterValue("filterKeyTrack");
  filterEnvAmountParam = apvts_.getRawParameterValue("filterEnvAmount");

  lfoShapeParam = apvts_.getRawParameterValue("lfoShape");
  lfoRateParam = apvts_.getRawParameterValue("lfoRate");
  lfoDepthParam = apvts_.getRawParameterValue("lfoDepth");
  lfoDestParam = apvts_.getRawParameterValue("lfoDest");

  lfo2ShapeParam = apvts_.getRawParameterValue("lfo2Shape");
  lfo2RateParam = apvts_.getRawParameterValue("lfo2Rate");
  lfo2DepthParam = apvts_.getRawParameterValue("lfo2Depth");
  lfo2DestParam = apvts_.getRawParameterValue("lfo2Dest");

  velToDParam = apvts_.getRawParameterValue("velToD");
  velToDepthParam = apvts_.getRawParameterValue("velToDepth");
  atToDParam = apvts_.getRawParameterValue("atToD");
  atToDepthParam = apvts_.getRawParameterValue("atToDepth");

  attackParam = apvts_.getRawParameterValue("attack");
  decayParam = apvts_.getRawParameterValue("decay");
  sustainParam = apvts_.getRawParameterValue("sustain");
  releaseParam = apvts_.getRawParameterValue("release");

  env2AttackParam = apvts_.getRawParameterValue("env2Attack");
  env2DecayParam = apvts_.getRawParameterValue("env2Decay");
  env2SustainParam = apvts_.getRawParameterValue("env2Sustain");
  env2ReleaseParam = apvts_.getRawParameterValue("env2Release");
  env2DestParam = apvts_.getRawParameterValue("env2Dest");
  env2AmountParam = apvts_.getRawParameterValue("env2Amount");

  analogDriveParam = apvts_.getRawParameterValue("analogDrive");
  analogMixParam = apvts_.getRawParameterValue("analogMix");

  oversampleModeParam = apvts_.getRawParameterValue("oversampleMode");

  volumeParam = apvts_.getRawParameterValue("volume");

  // Add voices and sound
  for (int i = 0; i < maxVoices; ++i) synth.addVoice(new WPPMVoice());
  synth.addSound(new WPPMSound());
}

SoundsgoodAudioProcessor::~SoundsgoodAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SoundsgoodAudioProcessor::createParameterLayout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  auto timeRange = juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f);

  // ---- WPPM oscillator ----
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"d", 1}, "Duration (d)",
      juce::NormalisableRange<float>(0.01f, 0.99f, 0.001f), 0.5f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"R1", 1}, "Ratio 1",
      juce::NormalisableRange<float>(0.0f, 16.0f, 0.01f), 1.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"I1", 1}, "Index 1",
      juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"P1", 1}, "Phase 1",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"R2", 1}, "Ratio 2",
      juce::NormalisableRange<float>(0.0f, 16.0f, 0.01f), 1.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"I2", 1}, "Index 2",
      juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"P2", 1}, "Phase 2",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"link", 1}, "Link Pulses", false));

  // ---- Sub oscillator ----
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"subOctave", 1}, "Sub Octave",
      juce::StringArray{"Off", "-1 Oct", "-2 Oct"}, 0));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"subMix", 1}, "Sub Mix",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  // ---- Unison ----
  params.push_back(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID{"unisonVoices", 1}, "Unison Voices", 1, 7, 1));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"unisonDetune", 1}, "Unison Detune",
      juce::NormalisableRange<float>(0.0f, 50.0f, 0.1f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"unisonSpread", 1}, "Unison Spread",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  // ---- Filter ----
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"filterMode", 1}, "Filter Mode",
      juce::StringArray{"Low Pass", "High Pass", "Band Pass"}, 0));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"filterCutoff", 1}, "Filter Cutoff",
      juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 8000.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"filterResonance", 1}, "Filter Resonance",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"filterKeyTrack", 1}, "Filter Key Track",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"filterEnvAmount", 1}, "Filter Env Amount",
      juce::NormalisableRange<float>(-48.0f, 48.0f, 0.1f), 0.0f));

  // ---- LFO ----
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"lfoShape", 1}, "LFO Shape",
      juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfoRate", 1}, "LFO Rate",
      juce::NormalisableRange<float>(0.01f, 20.0f, 0.01f, 0.4f), 1.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfoDepth", 1}, "LFO Depth",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"lfoDest", 1}, "LFO Dest",
      juce::StringArray{"d", "Filter", "Pitch", "Ratio"}, 0));

  // ---- LFO 2 ----
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"lfo2Shape", 1}, "LFO2 Shape",
      juce::StringArray{"Sine", "Triangle", "Saw", "Square"}, 0));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfo2Rate", 1}, "LFO2 Rate",
      juce::NormalisableRange<float>(0.01f, 20.0f, 0.01f, 0.4f), 1.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfo2Depth", 1}, "LFO2 Depth",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"lfo2Dest", 1}, "LFO2 Dest",
      juce::StringArray{"d", "Filter", "Pitch", "Ratio"}, 0));

  // ---- Modulation routing ----
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"velToD", 1}, "Vel -> D",
      juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"velToDepth", 1}, "Vel -> Depth",
      juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"atToD", 1}, "AT -> D",
      juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"atToDepth", 1}, "AT -> Depth",
      juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

  // ---- Envelope ADSR ----
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"attack", 1}, "Attack", timeRange, 0.01f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"decay", 1}, "Decay", timeRange, 0.3f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"sustain", 1}, "Sustain",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"release", 1}, "Release", timeRange, 0.5f));

  // ---- Envelope 2 (mod) ----
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"env2Attack", 1}, "Env2 Attack", timeRange, 0.01f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"env2Decay", 1}, "Env2 Decay", timeRange, 0.3f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"env2Sustain", 1}, "Env2 Sustain",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"env2Release", 1}, "Env2 Release", timeRange, 0.5f));

  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"env2Dest", 1}, "Env2 Dest",
      juce::StringArray{"d", "Filter", "Depth", "Ratio", "Pitch"}, 0));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"env2Amount", 1}, "Env2 Amount",
      juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));

  // ---- Analog stage ----
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"analogDrive", 1}, "Analog Drive",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"analogMix", 1}, "Analog Mix",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  // ---- Quality ----
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"oversampleMode", 1}, "Quality",
      juce::StringArray{"Eco", "High (4x)", "Ultra (8x)"}, 0));

  // ---- Output ----
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"volume", 1}, "Volume",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));

  return {params.begin(), params.end()};
}

//==============================================================================
const juce::String SoundsgoodAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool SoundsgoodAudioProcessor::acceptsMidi() const { return true; }
bool SoundsgoodAudioProcessor::producesMidi() const { return false; }
bool SoundsgoodAudioProcessor::isMidiEffect() const { return false; }
double SoundsgoodAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int SoundsgoodAudioProcessor::getNumPrograms() { return 1; }
int SoundsgoodAudioProcessor::getCurrentProgram() { return 0; }
void SoundsgoodAudioProcessor::setCurrentProgram(int /*index*/) {}
const juce::String SoundsgoodAudioProcessor::getProgramName(int /*index*/) {
  return {};
}
void SoundsgoodAudioProcessor::changeProgramName(int /*index*/,
                                                 const juce::String& /*n*/) {}

//==============================================================================
void SoundsgoodAudioProcessor::prepareToPlay(double sampleRate,
                                             int samplesPerBlock) {
  synth.setCurrentPlaybackSampleRate(sampleRate);

  for (int i = 0; i < synth.getNumVoices(); ++i) {
    if (auto* voice = dynamic_cast<WPPMVoice*>(synth.getVoice(i))) {
      voice->prepareToPlay(sampleRate, samplesPerBlock);
    }
  }
}

void SoundsgoodAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SoundsgoodAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
    return false;
  }
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif
  return true;
#endif
}
#endif

//==============================================================================
void SoundsgoodAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels();
       ++ch) {
    buffer.clear(ch, 0, buffer.getNumSamples());
  }

  int numSamples = buffer.getNumSamples();

  // --- Build voice parameters ---
  WPPMVoiceParams vp;

  vp.d = dParam->load();
  vp.R1 = r1Param->load();
  vp.I1 = i1Param->load();
  vp.P1 = p1Param->load();

  bool linked = linkParam->load() >= 0.5f;
  if (linked) {
    vp.R2 = vp.R1;
    vp.I2 = vp.I1;
    vp.P2 = vp.P1;
  } else {
    vp.R2 = r2Param->load();
    vp.I2 = i2Param->load();
    vp.P2 = p2Param->load();
  }

  // Sub oscillator
  vp.subOctave = juce::roundToInt(subOctaveParam->load());
  vp.subMix = subMixParam->load();

  // Unison
  vp.unisonVoices = juce::roundToInt(unisonVoicesParam->load());
  vp.unisonDetune = unisonDetuneParam->load();
  vp.unisonSpread = unisonSpreadParam->load();

  // Filter
  vp.filterMode =
      static_cast<FilterMode>(juce::roundToInt(filterModeParam->load()));
  vp.filterCutoff = filterCutoffParam->load();
  vp.filterResonance = filterResonanceParam->load();
  vp.filterKeyTrack = filterKeyTrackParam->load();
  vp.filterEnvAmount = filterEnvAmountParam->load();

  // LFO
  vp.lfoShape =
      static_cast<WPPMLFO::Shape>(juce::roundToInt(lfoShapeParam->load()));
  vp.lfoRate = lfoRateParam->load();
  vp.lfoDepth = lfoDepthParam->load();
  vp.lfoDest = juce::roundToInt(lfoDestParam->load());

  // LFO2
  vp.lfo2Shape =
      static_cast<WPPMLFO::Shape>(juce::roundToInt(lfo2ShapeParam->load()));
  vp.lfo2Rate = lfo2RateParam->load();
  vp.lfo2Depth = lfo2DepthParam->load();
  vp.lfo2Dest = juce::roundToInt(lfo2DestParam->load());

  // Modulation routing
  vp.velToD = velToDParam->load();
  vp.velToDepth = velToDepthParam->load();
  vp.atToD = atToDParam->load();
  vp.atToDepth = atToDepthParam->load();

  // ADSR
  vp.attack = attackParam->load();
  vp.decay = decayParam->load();
  vp.sustain = sustainParam->load();
  vp.release = releaseParam->load();

  // ENV2
  vp.env2Attack = env2AttackParam->load();
  vp.env2Decay = env2DecayParam->load();
  vp.env2Sustain = env2SustainParam->load();
  vp.env2Release = env2ReleaseParam->load();
  vp.env2Dest = juce::roundToInt(env2DestParam->load());
  vp.env2Amount = env2AmountParam->load();

  // Analog stage
  vp.analogDrive = analogDriveParam->load();
  vp.analogMix = analogMixParam->load();

  // Quality
  vp.oversampleMode = juce::roundToInt(oversampleModeParam->load());

  // Output
  vp.volume = volumeParam->load();

  // Push params to all voices
  for (int i = 0; i < synth.getNumVoices(); ++i) {
    if (auto* voice = dynamic_cast<WPPMVoice*>(synth.getVoice(i))) {
      voice->setParams(vp);
    }
  }

  // Render
  buffer.clear();
  synth.renderNextBlock(buffer, midiMessages, 0, numSamples);

  // Write to scope ring buffer
  {
    const float* ch0 = buffer.getReadPointer(0);
    int wp = scopeWritePos_.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i) {
      scopeBuffer_[wp] = ch0[i];
      wp = (wp + 1) % kScopeSize;
    }
    scopeWritePos_.store(wp, std::memory_order_relaxed);
  }
}

//==============================================================================
bool SoundsgoodAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SoundsgoodAudioProcessor::createEditor() {
  return new SoundsgoodAudioProcessorEditor(*this);
}

//==============================================================================
void SoundsgoodAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData) {
  auto state = apvts_.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void SoundsgoodAudioProcessor::setStateInformation(const void* data,
                                                   int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));

  if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType())) {
    apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
  }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new SoundsgoodAudioProcessor();
}
