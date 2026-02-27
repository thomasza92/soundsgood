/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin processor.
  ==============================================================================
*/

#include "PluginProcessor.h"

#include <unordered_map>

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
      oscillator(),
      frequencyParam(std::make_unique<juce::AudioParameterFloat>(
          "frequency", "Frequency",
          juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f), 440.0f)) {
}

SoundsgoodAudioProcessor::~SoundsgoodAudioProcessor() {}

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
  oscillator.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), 0});
  *frequencyParam = getFrequency();
}

void SoundsgoodAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SoundsgoodAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  bool isSupported = false;
  const auto main = layouts.getMainOutputChannelSet();
  if (main == juce::AudioChannelSet::mono() ||
      main == juce::AudioChannelSet::stereo()) {
    isSupported = true;
  }
  return isSupported;
#endif
}
#endif

void SoundsgoodAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages) {
  const int numChannels = buffer.getNumChannels();
  const int numSamples = buffer.getNumSamples();

  // Process MIDI messages
  for (const auto& midiMessage : midiMessages) {
    if (midiMessage.getMessage().isNoteOn()) {
      const int noteNumber = midiMessage.getMessage().getNoteNumber();
      const double velocity = midiMessage.getMessage().getVelocity();

      // Only process note-on with non-zero velocity
      if (velocity > 0.0f) {
        // Calculate frequency from MIDI note number
        const double frequency = midiNoteToFrequency(noteNumber);

        // Store active note
        activeNotes[noteNumber] = frequency;
      }
    } else if (midiMessage.getMessage().isNoteOff()) {
      const int noteNumber = midiMessage.getMessage().getNoteNumber();

      // Remove note from active notes
      activeNotes.erase(noteNumber);
    }
  }

  // Clear buffer
  buffer.clear();

  // Generate audio based on active MIDI notes
  for (int sample = 0; sample < numSamples; ++sample) {
    float output = 0.0f;

    // Sum all active notes
    for (const auto& note : activeNotes) {
      const int noteNumber = note.first;
      const double frequency = note.second;

      // Set oscillator frequency for this note
      oscillator.setFrequency(frequency);
      oscillator.initialise(getOscillatorWaveformFunction(getWaveform()));

      // Process sample
      output += oscillator.processSample(0.0f);
    }

    // Normalize output to avoid clipping when multiple notes are active
    if (!activeNotes.empty()) {
      output /= static_cast<float>(activeNotes.size());
    }

    // Output to all channels
    for (int channel = 0; channel < numChannels; ++channel) {
      buffer.setSample(channel, sample, output);
    }
  }
}

std::function<float(float)>
SoundsgoodAudioProcessor::getOscillatorWaveformFunction(Waveform waveform) {
  switch (waveform) {
    case Waveform::sine:
      return [](float x) { return std::sin(x); };
    case Waveform::square:
      return [](float x) { return std::sin(x) >= 0.0f ? 1.0f : -1.0f; };
    case Waveform::saw:
      return [](float x) {
        return (x + std::fmod(x, 2.0f * juce::MathConstants<float>::pi)) /
                   (2.0f * juce::MathConstants<float>::pi) * 2.0f -
               1.0f;
      };
    default:
      return [](float x) { return std::sin(x); };
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
  auto state = juce::ValueTree("soundsgood_state");
  state.setProperty("waveform", static_cast<int>(currentWaveform), nullptr);
  state.setProperty("frequency", currentFrequency, nullptr);

  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void SoundsgoodAudioProcessor::setStateInformation(const void* data,
                                                   int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(juce::XmlDocument::parse(
      juce::String(reinterpret_cast<const char*>(data), sizeInBytes)));

  if (xmlState.get() != nullptr) {
    if (xmlState->hasTagName("soundsgood_state")) {
      currentWaveform =
          static_cast<Waveform>(xmlState->getIntAttribute("waveform", 0));
      currentFrequency = xmlState->getDoubleAttribute("frequency", 440.0);

      oscillator.initialise(getOscillatorWaveformFunction(currentWaveform));
    }
  } else {
    currentWaveform = Waveform::sine;
    currentFrequency = 440.0;
    oscillator.initialise([](float x) { return std::sin(x); });
  }
}

//==============================================================================
// Helper to convert MIDI note number to frequency
double SoundsgoodAudioProcessor::midiNoteToFrequency(int note) {
  return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

// This creates the first time you open the editor, or when the settings
// change.

// Standalone plugin factory function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new SoundsgoodAudioProcessor();
}
