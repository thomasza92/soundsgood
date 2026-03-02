/*
  ==============================================================================
    PluginProcessor.cpp
    Soundsgood — Yamaha CS-30 full emulation.
    75 parameters, global LFO + sequencer, polyphonic voice management.
  ==============================================================================
*/

#include "PluginProcessor.h"

#include "PluginEditor.h"

//==============================================================================
SoundsgoodAudioProcessor::SoundsgoodAudioProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  // ── Cache all 75 parameter pointers ──

  // Pitch
  pitchTuneParam = apvts.getRawParameterValue("pitchTune");
  pitchDetuneParam = apvts.getRawParameterValue("pitchDetune");
  pitchEGDepthParam = apvts.getRawParameterValue("pitchEGDepth");
  pitchEGSelParam = apvts.getRawParameterValue("pitchEGSel");

  // VCO 1
  vco1FeetParam = apvts.getRawParameterValue("vco1Feet");
  vco1PWParam = apvts.getRawParameterValue("vco1PW");
  vco1ModVCO2Param = apvts.getRawParameterValue("vco1ModVCO2");
  vco1PWMParam = apvts.getRawParameterValue("vco1PWM");
  vco1FuncParam = apvts.getRawParameterValue("vco1Func");
  vco1DepthParam = apvts.getRawParameterValue("vco1Depth");
  vco1SrcParam = apvts.getRawParameterValue("vco1Src");
  vco1EGSelParam = apvts.getRawParameterValue("vco1EGSel");

  // VCO 2
  vco2FeetParam = apvts.getRawParameterValue("vco2Feet");
  vco2PWParam = apvts.getRawParameterValue("vco2PW");
  vco2PWMParam = apvts.getRawParameterValue("vco2PWM");
  vco2FuncParam = apvts.getRawParameterValue("vco2Func");
  vco2DepthParam = apvts.getRawParameterValue("vco2Depth");
  vco2SrcParam = apvts.getRawParameterValue("vco2Src");
  vco2EGSelParam = apvts.getRawParameterValue("vco2EGSel");

  // VCF 1
  vcf1LevelParam = apvts.getRawParameterValue("vcf1Level");
  vcf1CutoffParam = apvts.getRawParameterValue("vcf1Cutoff");
  vcf1KBDFollowParam = apvts.getRawParameterValue("vcf1KBDFollow");
  vcf1ResoParam = apvts.getRawParameterValue("vcf1Reso");
  vcf1ModDepthParam = apvts.getRawParameterValue("vcf1ModDepth");
  vcf1EGDepthParam = apvts.getRawParameterValue("vcf1EGDepth");
  vcf1ModFuncParam = apvts.getRawParameterValue("vcf1ModFunc");
  vcf1EGSelParam = apvts.getRawParameterValue("vcf1EGSel");

  // VCF 2
  vcf2LevelParam = apvts.getRawParameterValue("vcf2Level");
  vcf2CutoffParam = apvts.getRawParameterValue("vcf2Cutoff");
  vcf2KBDFollowParam = apvts.getRawParameterValue("vcf2KBDFollow");
  vcf2ResoParam = apvts.getRawParameterValue("vcf2Reso");
  vcf2ModDepthParam = apvts.getRawParameterValue("vcf2ModDepth");
  vcf2EGDepthParam = apvts.getRawParameterValue("vcf2EGDepth");
  vcf2ModFuncParam = apvts.getRawParameterValue("vcf2ModFunc");
  vcf2EGSelParam = apvts.getRawParameterValue("vcf2EGSel");

  // VCA 1
  vca1FilterTypeParam = apvts.getRawParameterValue("vca1FilterType");
  vca1VCF1Param = apvts.getRawParameterValue("vca1VCF1");
  vca1VCF2Param = apvts.getRawParameterValue("vca1VCF2");
  vca1ModDepthParam = apvts.getRawParameterValue("vca1ModDepth");
  vca1VCO1Param = apvts.getRawParameterValue("vca1VCO1");
  vca1ModFuncParam = apvts.getRawParameterValue("vca1ModFunc");

  // VCA 2
  vca2NormModParam = apvts.getRawParameterValue("vca2NormMod");
  vca2ModDepthParam = apvts.getRawParameterValue("vca2ModDepth");
  vca2RMOSrcParam = apvts.getRawParameterValue("vca2RMOSrc");
  vca2ModFuncParam = apvts.getRawParameterValue("vca2ModFunc");

  // Trigger
  trigModeParam = apvts.getRawParameterValue("trigMode");
  trigTypeParam = apvts.getRawParameterValue("trigType");

  // Sequencer
  seqClockParam = apvts.getRawParameterValue("seqClock");
  seqRunningParam = apvts.getRawParameterValue("seqRunning");
  seqHoldParam = apvts.getRawParameterValue("seqHold");
  seqStepsParam = apvts.getRawParameterValue("seqSteps");
  for (int i = 0; i < 8; ++i) {
    seqKnobParams[i] =
        apvts.getRawParameterValue("seqKnob" + juce::String(i + 1));
  }

  // EG 1
  eg1AtkParam = apvts.getRawParameterValue("eg1Atk");
  eg1DecParam = apvts.getRawParameterValue("eg1Dec");
  eg1SusParam = apvts.getRawParameterValue("eg1Sus");
  eg1RelParam = apvts.getRawParameterValue("eg1Rel");

  // EG 2
  eg2AtkParam = apvts.getRawParameterValue("eg2Atk");
  eg2DecParam = apvts.getRawParameterValue("eg2Dec");
  eg2SusParam = apvts.getRawParameterValue("eg2Sus");
  eg2RelParam = apvts.getRawParameterValue("eg2Rel");

  // EG 3
  eg3AtkParam = apvts.getRawParameterValue("eg3Atk");
  eg3DecParam = apvts.getRawParameterValue("eg3Dec");
  eg3SusParam = apvts.getRawParameterValue("eg3Sus");
  eg3RelParam = apvts.getRawParameterValue("eg3Rel");

  // LFO
  lfoEGSelParam = apvts.getRawParameterValue("lfoEGSel");
  lfoEGDepthParam = apvts.getRawParameterValue("lfoEGDepth");
  lfoSpeedParam = apvts.getRawParameterValue("lfoSpeed");

  // Portamento
  portamentoParam = apvts.getRawParameterValue("portamento");

  // Pitch Bend
  pbLimiterParam = apvts.getRawParameterValue("pbLimiter");
  pbRangeParam = apvts.getRawParameterValue("pbRange");

  // Output
  outBalanceParam = apvts.getRawParameterValue("outBalance");
  outVolumeParam = apvts.getRawParameterValue("outVolume");
  outMonoParam = apvts.getRawParameterValue("outMono");

  // ── Add voices and sound ──
  for (int i = 0; i < maxVoices; ++i) {
    synth.addVoice(new CS30Voice());
  }
  synth.addSound(new CS30Sound());
}

SoundsgoodAudioProcessor::~SoundsgoodAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
SoundsgoodAudioProcessor::createParameterLayout() {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  // Helper ranges
  auto knobRange = juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f);
  auto timeRange = juce::NormalisableRange<float>(0.001f, 10.0f, 0.001f, 0.3f);
  auto cutoffRange =
      juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f);
  auto unitRange = juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f);
  auto pwRange = juce::NormalisableRange<float>(0.05f, 0.95f, 0.01f);

  // Shared choice arrays
  juce::StringArray egChoices{"EG 1", "EG 2", "EG 3"};
  juce::StringArray feetChoices{"32'", "16'", "8'", "4'", "2'"};
  juce::StringArray waveChoices{"Saw", "Square", "Triangle", "Sine", "Noise"};
  juce::StringArray modFuncChoices{"Sine", "Triangle", "Saw", "Square", "S/H"};
  juce::StringArray filterTypeChoices{"Low Pass", "Band Pass", "High Pass"};

  // ── PITCH ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"pitchTune", 1}, "Pitch Tune",
      juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"pitchDetune", 1}, "Pitch Detune",
      juce::NormalisableRange<float>(-7.0f, 7.0f, 0.01f), 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"pitchEGDepth", 1}, "Pitch EG Depth", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"pitchEGSel", 1}, "Pitch EG Select", egChoices, 0));

  // ── VCO 1 ──
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vco1Feet", 1}, "VCO1 Feet", feetChoices, 2));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco1PW", 1}, "VCO1 Pulse Width", pwRange, 0.5f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco1ModVCO2", 1}, "VCO1 Mod VCO2", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco1PWM", 1}, "VCO1 PWM", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vco1Func", 1}, "VCO1 Function", waveChoices, 0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco1Depth", 1}, "VCO1 Depth", knobRange, 7.0f));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"vco1Src", 1}, "VCO1 SEQ", false));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vco1EGSel", 1}, "VCO1 EG Select", egChoices, 1));

  // ── VCO 2 ──
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vco2Feet", 1}, "VCO2 Feet", feetChoices, 2));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco2PW", 1}, "VCO2 Pulse Width", pwRange, 0.5f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco2PWM", 1}, "VCO2 PWM", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vco2Func", 1}, "VCO2 Function", waveChoices, 0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vco2Depth", 1}, "VCO2 Depth", knobRange, 5.0f));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"vco2Src", 1}, "VCO2 SEQ", false));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vco2EGSel", 1}, "VCO2 EG Select", egChoices, 1));

  // ── VCF 1 ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf1Level", 1}, "VCF1 Level", knobRange, 10.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf1Cutoff", 1}, "VCF1 Cutoff", cutoffRange, 5000.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf1KBDFollow", 1}, "VCF1 KBD Follow", knobRange,
      5.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf1Reso", 1}, "VCF1 Resonance", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf1ModDepth", 1}, "VCF1 Mod Depth", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf1EGDepth", 1}, "VCF1 EG Depth", knobRange, 5.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vcf1ModFunc", 1}, "VCF1 Mod Function", modFuncChoices,
      0));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vcf1EGSel", 1}, "VCF1 EG Select", egChoices, 0));

  // ── VCF 2 ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf2Level", 1}, "VCF2 Level", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf2Cutoff", 1}, "VCF2 Cutoff", cutoffRange, 5000.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf2KBDFollow", 1}, "VCF2 KBD Follow", knobRange,
      5.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf2Reso", 1}, "VCF2 Resonance", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf2ModDepth", 1}, "VCF2 Mod Depth", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vcf2EGDepth", 1}, "VCF2 EG Depth", knobRange, 5.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vcf2ModFunc", 1}, "VCF2 Mod Function", modFuncChoices,
      0));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vcf2EGSel", 1}, "VCF2 EG Select", egChoices, 0));

  // ── VCA 1 ──
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vca1FilterType", 1}, "VCA1 Filter Type",
      filterTypeChoices, 0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vca1VCF1", 1}, "VCA1 VCF1 Level", knobRange, 10.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vca1VCF2", 1}, "VCA1 VCF2 Level", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vca1ModDepth", 1}, "VCA1 Mod Depth", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vca1VCO1", 1}, "VCA1 VCO1 Direct", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vca1ModFunc", 1}, "VCA1 Mod Function", modFuncChoices,
      0));

  // ── VCA 2 ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vca2NormMod", 1}, "VCA2 Normal/Mod", unitRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"vca2ModDepth", 1}, "VCA2 Mod Depth", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vca2RMOSrc", 1}, "VCA2 RMO Source",
      juce::StringArray{"VCO 1", "LFO"}, 0));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"vca2ModFunc", 1}, "VCA2 Mod Function", modFuncChoices,
      0));

  // ── TRIGGER ──
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"trigMode", 1}, "Trigger LFO", false));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"trigType", 1}, "Trigger Multi", true));

  // ── SEQUENCER ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"seqClock", 1}, "Seq Clock",
      juce::NormalisableRange<float>(0.1f, 20.0f, 0.01f, 0.4f), 2.0f));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"seqRunning", 1}, "Seq Running", false));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"seqHold", 1}, "Seq Hold", false));
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"seqSteps", 1}, "Seq Steps",
      juce::StringArray{"1", "2", "3", "4", "5", "6", "7", "8"}, 7));
  // Step knobs snap to semitones: ±24 (±2 octaves)
  auto semiRange = juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f);
  for (int i = 1; i <= 8; ++i) {
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"seqKnob" + juce::String(i), 1},
        "Seq Step " + juce::String(i), semiRange, 0.0f));
  }

  // ── EG 1 ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg1Atk", 1}, "EG1 Attack", timeRange, 0.01f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg1Dec", 1}, "EG1 Decay", timeRange, 0.3f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg1Sus", 1}, "EG1 Sustain", unitRange, 0.7f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg1Rel", 1}, "EG1 Release", timeRange, 0.5f));

  // ── EG 2 ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg2Atk", 1}, "EG2 Attack", timeRange, 0.01f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg2Dec", 1}, "EG2 Decay", timeRange, 0.3f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg2Sus", 1}, "EG2 Sustain", unitRange, 0.7f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg2Rel", 1}, "EG2 Release", timeRange, 0.5f));

  // ── EG 3 ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg3Atk", 1}, "EG3 Attack", timeRange, 0.01f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg3Dec", 1}, "EG3 Decay", timeRange, 0.3f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg3Sus", 1}, "EG3 Sustain", unitRange, 0.7f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"eg3Rel", 1}, "EG3 Release", timeRange, 0.5f));

  // ── LFO ──
  params.push_back(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"lfoEGSel", 1}, "LFO EG Select", egChoices, 0));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfoEGDepth", 1}, "LFO EG Depth", knobRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfoSpeed", 1}, "LFO Speed",
      juce::NormalisableRange<float>(0.1f, 30.0f, 0.01f, 0.35f), 5.0f));

  // ── PORTAMENTO ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"portamento", 1}, "Portamento", knobRange, 0.0f));

  // ── PITCH BEND ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"pbLimiter", 1}, "PB Limiter", unitRange, 0.0f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"pbRange", 1}, "PB Range",
      juce::NormalisableRange<float>(0.0f, 12.0f, 0.5f), 2.0f));

  // ── OUTPUT ──
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"outBalance", 1}, "Output Balance", unitRange, 0.5f));
  params.push_back(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"outVolume", 1}, "Output Volume", knobRange, 8.0f));
  params.push_back(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"outMono", 1}, "Mono Mode", false));

  return {params.begin(), params.end()};
}

//==============================================================================
const juce::String SoundsgoodAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool SoundsgoodAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool SoundsgoodAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool SoundsgoodAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double SoundsgoodAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int SoundsgoodAudioProcessor::getNumPrograms() { return 1; }

int SoundsgoodAudioProcessor::getCurrentProgram() { return 0; }

void SoundsgoodAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String SoundsgoodAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void SoundsgoodAudioProcessor::changeProgramName(int index,
                                                 const juce::String& newName) {
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void SoundsgoodAudioProcessor::prepareToPlay(double sampleRate,
                                             int samplesPerBlock) {
  synth.setCurrentPlaybackSampleRate(sampleRate);
  lfo_.prepare(sampleRate);
  sequencer_.prepare(sampleRate);

  for (int i = 0; i < synth.getNumVoices(); ++i) {
    if (auto* voice = dynamic_cast<CS30Voice*>(synth.getVoice(i))) {
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
  const auto main = layouts.getMainOutputChannelSet();
  return (main == juce::AudioChannelSet::mono() ||
          main == juce::AudioChannelSet::stereo());
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

  // ── Advance global LFO and Sequencer ──
  lfo_.setSpeed(lfoSpeedParam->load());
  sequencer_.setClockSpeed(seqClockParam->load());
  sequencer_.setRunning(seqRunningParam->load() >= 0.5f);
  sequencer_.setHold(seqHoldParam->load() >= 0.5f);
  sequencer_.setNumSteps(static_cast<int>(seqStepsParam->load()) + 1);
  for (int s = 0; s < 8; ++s) {
    sequencer_.setStepValue(s, seqKnobParams[s]->load());
  }

  for (int s = 0; s < numSamples; ++s) {
    lfo_.advance();
    sequencer_.advance();
  }

  // Read current LFO waveform outputs (all 5 simultaneously)
  float lfoSine = lfo_.getOutput(CS30LFO::Waveform::Sine);
  float lfoTri = lfo_.getOutput(CS30LFO::Waveform::Triangle);
  float lfoSaw = lfo_.getOutput(CS30LFO::Waveform::Saw);
  float lfoSq = lfo_.getOutput(CS30LFO::Waveform::Square);
  float lfoSH = lfo_.getOutput(CS30LFO::Waveform::SampleHold);

  // ── Build voice parameters struct ──
  CS30VoiceParams vp;

  // Pitch
  vp.pitchTune = pitchTuneParam->load();
  vp.pitchDetune = pitchDetuneParam->load();
  vp.pitchEGDepth = pitchEGDepthParam->load();
  vp.pitchEGSel = static_cast<int>(pitchEGSelParam->load());

  // VCO 1
  vp.vco1Feet = static_cast<int>(vco1FeetParam->load());
  vp.vco1PW = vco1PWParam->load();
  vp.vco1ModVCO2 = vco1ModVCO2Param->load();
  vp.vco1PWM = vco1PWMParam->load();
  vp.vco1Func = static_cast<CS30Oscillator::Waveform>(
      static_cast<int>(vco1FuncParam->load()));
  vp.vco1Depth = vco1DepthParam->load();
  vp.vco1Seq = (vco1SrcParam->load() >= 0.5f);
  vp.vco1EGSel = static_cast<int>(vco1EGSelParam->load());

  // VCO 2
  vp.vco2Feet = static_cast<int>(vco2FeetParam->load());
  vp.vco2PW = vco2PWParam->load();
  vp.vco2PWM = vco2PWMParam->load();
  vp.vco2Func = static_cast<CS30Oscillator::Waveform>(
      static_cast<int>(vco2FuncParam->load()));
  vp.vco2Depth = vco2DepthParam->load();
  vp.vco2Seq = (vco2SrcParam->load() >= 0.5f);
  vp.vco2EGSel = static_cast<int>(vco2EGSelParam->load());

  // VCF 1
  vp.vcf1Level = vcf1LevelParam->load();
  vp.vcf1Cutoff = vcf1CutoffParam->load();
  vp.vcf1KBDFollow = vcf1KBDFollowParam->load();
  vp.vcf1Reso = vcf1ResoParam->load();
  vp.vcf1ModDepth = vcf1ModDepthParam->load();
  vp.vcf1EGDepth = vcf1EGDepthParam->load();
  vp.vcf1ModFunc = static_cast<CS30LFO::Waveform>(
      static_cast<int>(vcf1ModFuncParam->load()));
  vp.vcf1EGSel = static_cast<int>(vcf1EGSelParam->load());

  // VCF 2
  vp.vcf2Level = vcf2LevelParam->load();
  vp.vcf2Cutoff = vcf2CutoffParam->load();
  vp.vcf2KBDFollow = vcf2KBDFollowParam->load();
  vp.vcf2Reso = vcf2ResoParam->load();
  vp.vcf2ModDepth = vcf2ModDepthParam->load();
  vp.vcf2EGDepth = vcf2EGDepthParam->load();
  vp.vcf2ModFunc = static_cast<CS30LFO::Waveform>(
      static_cast<int>(vcf2ModFuncParam->load()));
  vp.vcf2EGSel = static_cast<int>(vcf2EGSelParam->load());

  // VCA 1
  vp.vca1FilterType = static_cast<CS30Filter::Type>(
      static_cast<int>(vca1FilterTypeParam->load()));
  vp.vca1VCF1 = vca1VCF1Param->load();
  vp.vca1VCF2 = vca1VCF2Param->load();
  vp.vca1ModDepth = vca1ModDepthParam->load();
  vp.vca1VCO1 = vca1VCO1Param->load();
  vp.vca1ModFunc = static_cast<CS30LFO::Waveform>(
      static_cast<int>(vca1ModFuncParam->load()));

  // VCA 2
  vp.vca2NormMod = vca2NormModParam->load();
  vp.vca2ModDepth = vca2ModDepthParam->load();
  vp.vca2RMOSrc = static_cast<int>(vca2RMOSrcParam->load());
  vp.vca2ModFunc = static_cast<CS30LFO::Waveform>(
      static_cast<int>(vca2ModFuncParam->load()));

  // Trigger
  vp.trigLFO = (trigModeParam->load() >= 0.5f);
  vp.trigMulti = (trigTypeParam->load() >= 0.5f);

  // EG 1
  vp.eg1Params.attackTime = eg1AtkParam->load();
  vp.eg1Params.decayTime = eg1DecParam->load();
  vp.eg1Params.sustainLevel = eg1SusParam->load();
  vp.eg1Params.releaseTime = eg1RelParam->load();

  // EG 2
  vp.eg2Params.attackTime = eg2AtkParam->load();
  vp.eg2Params.decayTime = eg2DecParam->load();
  vp.eg2Params.sustainLevel = eg2SusParam->load();
  vp.eg2Params.releaseTime = eg2RelParam->load();

  // EG 3
  vp.eg3Params.attackTime = eg3AtkParam->load();
  vp.eg3Params.decayTime = eg3DecParam->load();
  vp.eg3Params.sustainLevel = eg3SusParam->load();
  vp.eg3Params.releaseTime = eg3RelParam->load();

  // Sequencer CV → pitch
  vp.sequencerCV = sequencer_.getCurrentValue();

  // LFO EG gating
  vp.lfoEGDepth = lfoEGDepthParam->load();
  vp.lfoEGSel = static_cast<int>(lfoEGSelParam->load());

  // Portamento, Pitch Bend, Output
  vp.portamento = portamentoParam->load();
  vp.pbRange = pbRangeParam->load();
  vp.outBalance = outBalanceParam->load();
  vp.outVolume = outVolumeParam->load();

  // ── Push to all voices ──
  for (int i = 0; i < synth.getNumVoices(); ++i) {
    if (auto* voice = dynamic_cast<CS30Voice*>(synth.getVoice(i))) {
      voice->setParams(vp);
      voice->setLFOValues(lfoSine, lfoTri, lfoSaw, lfoSq, lfoSH);
    }
  }

  // ── Sequencer auto-voice: CS-30 plays a base note when seq runs ──
  // Only auto-trigger if at least one VCO is set to SEQ
  bool seqRunning = seqRunningParam->load() >= 0.5f;
  bool anyVcoSeq =
      (vco1SrcParam->load() >= 0.5f) || (vco2SrcParam->load() >= 0.5f);
  bool shouldAutoPlay = seqRunning && anyVcoSeq;
  if (shouldAutoPlay && !seqNoteActive_) {
    synth.noteOn(kSeqMidiChannel, kSeqMidiNote, 0.8f);
    seqNoteActive_ = true;
  } else if (!shouldAutoPlay && seqNoteActive_) {
    synth.noteOff(kSeqMidiChannel, kSeqMidiNote, 0.0f, true);
    seqNoteActive_ = false;
  }

  // Mark the sequencer auto-voice so it mutes non-SEQ VCOs
  for (int i = 0; i < synth.getNumVoices(); ++i) {
    if (auto* voice = dynamic_cast<CS30Voice*>(synth.getVoice(i))) {
      bool isSeqVoice = seqNoteActive_ && voice->isVoiceActive() &&
                        voice->getCurrentlyPlayingNote() == kSeqMidiNote;
      voice->setSeqAutoVoice(isSeqVoice);
    }
  }

  // ── Mono mode: single-voice retrigger (like a real mono synth) ──
  // Uses a dedicated monoVoice_ pointer so the sequencer auto-voice
  // (and any other voices) are never touched by the mono logic.
  bool monoMode = (outMonoParam->load() >= 0.5f);

  buffer.clear();

  if (monoMode) {
    // Strip noteOn/noteOff from MIDI — mono code handles them directly.
    // Keep everything else (CC, pitch bend, etc.)
    juce::MidiBuffer filteredMidi;
    for (const auto metadata : midiMessages) {
      auto msg = metadata.getMessage();
      if (!msg.isNoteOn() && !msg.isNoteOff()) {
        filteredMidi.addEvent(msg, metadata.samplePosition);
      }
    }

    // Process noteOn/noteOff manually through the tracked mono voice
    for (const auto metadata : midiMessages) {
      auto msg = metadata.getMessage();

      if (msg.isNoteOn()) {
        int newNote = msg.getNoteNumber();
        float vel = msg.getFloatVelocity();

        if (monoVoice_ && monoVoice_->isVoiceActive()) {
          // Retrigger existing mono voice — instant pitch change, no overlap
          monoVoice_->retriggerNote(newNote, vel);
        } else {
          // Snapshot which voices are already active (e.g. sequencer)
          bool wasActive[maxVoices] = {};
          for (int v = 0; v < synth.getNumVoices(); ++v) {
            wasActive[v] = synth.getVoice(v)->isVoiceActive();
          }

          // Ask the Synthesiser to allocate a fresh voice
          synth.noteOn(msg.getChannel(), newNote, vel);

          // Find the NEWLY allocated voice (skip pre-existing ones)
          monoVoice_ = nullptr;
          for (int v = 0; v < synth.getNumVoices(); ++v) {
            auto* voice = dynamic_cast<CS30Voice*>(synth.getVoice(v));
            if (voice && voice->isVoiceActive() && !wasActive[v]) {
              monoVoice_ = voice;
              break;
            }
          }
        }
        monoCurrentNote_ = newNote;

      } else if (msg.isNoteOff()) {
        if (msg.getNoteNumber() == monoCurrentNote_) {
          if (monoVoice_ && monoVoice_->isVoiceActive()) {
            monoVoice_->stopNote(0.0f, true);
          }
          monoCurrentNote_ = -1;
        }
      }
    }

    synth.renderNextBlock(buffer, filteredMidi, 0, numSamples);

    // Clean up if mono voice finished its release tail
    if (monoVoice_ && !monoVoice_->isVoiceActive()) monoVoice_ = nullptr;

  } else {
    monoVoice_ = nullptr;
    monoCurrentNote_ = -1;
    synth.renderNextBlock(buffer, midiMessages, 0, numSamples);
  }

  // Write rendered output to scope ring buffer for waveform visualizer
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
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void SoundsgoodAudioProcessor::setStateInformation(const void* data,
                                                   int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));

  if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
    apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
  }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new SoundsgoodAudioProcessor();
}