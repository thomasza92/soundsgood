/*
  ==============================================================================
    This file contains the basic framework code for a JUCE plugin editor.
  ==============================================================================
*/

#include "PluginEditor.h"

#include "PluginProcessor.h"


//==============================================================================
SoundsgoodAudioProcessorEditor::SoundsgoodAudioProcessorEditor(
    SoundsgoodAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  // Create waveform selector combo box
  waveformComboBox = std::make_unique<juce::ComboBox>();
  waveformComboBox->setBounds(50, 30, 200, 40);
  waveformComboBox->addItem("Sine", 1);
  waveformComboBox->addItem("Square", 2);
  waveformComboBox->addItem("Saw", 3);

  // Set current selection based on processor state
  waveformComboBox->setSelectedId(
      static_cast<int>(audioProcessor.getWaveform()) + 1);

  // Add change callback to update processor when waveform changes
  waveformComboBox->onChange = [this]() {
    auto selectedId = waveformComboBox->getSelectedId();
    SoundsgoodAudioProcessor::Waveform newWaveform;

    switch (selectedId) {
      case 1:
        newWaveform = SoundsgoodAudioProcessor::Waveform::sine;
        break;
      case 2:
        newWaveform = SoundsgoodAudioProcessor::Waveform::square;
        break;
      case 3:
        newWaveform = SoundsgoodAudioProcessor::Waveform::saw;
        break;
      default:
        newWaveform = SoundsgoodAudioProcessor::Waveform::sine;
        break;
    }

    audioProcessor.setWaveform(newWaveform);
  };

  // Create frequency slider (rotary style)
  frequencySlider = std::make_unique<juce::Slider>();

  // Set bounds for the slider (positioned below the combo box)
  frequencySlider->setBounds(50, 90, 300, 120);

  // Configure the slider as a rotary knob
  frequencySlider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  frequencySlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

  // Set slider range (20 Hz to 20000 Hz)
  frequencySlider->setRange(20.0, 20000.0);

  // Set current value
  frequencySlider->setValue(audioProcessor.getFrequency());

  // Add change callback to update processor when frequency changes
  frequencySlider->addListener(this);

  // Create waveform label
  waveformLabel = std::make_unique<juce::Label>();
  waveformLabel->setText("Waveform:", juce::dontSendNotification);
  waveformLabel->setBounds(50, 10, 100, 25);
  waveformLabel->setJustificationType(juce::Justification::centredLeft);

  // Create frequency label
  frequencyLabel = std::make_unique<juce::Label>();
  frequencyLabel->setText("Frequency:", juce::dontSendNotification);
  frequencyLabel->setBounds(50, 70, 100, 25);
  frequencyLabel->setJustificationType(juce::Justification::centredLeft);

  // Add all components to the editor
  addAndMakeVisible(waveformComboBox.get());
  addAndMakeVisible(frequencySlider.get());
  addAndMakeVisible(waveformLabel.get());
  addAndMakeVisible(frequencyLabel.get());

  // Set initial size
  setSize(400, 200);
}

SoundsgoodAudioProcessorEditor::~SoundsgoodAudioProcessorEditor() {}

//==============================================================================
void SoundsgoodAudioProcessorEditor::paint(juce::Graphics &g) {
  // Fill background
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  // Draw title
  g.setColour(juce::Colours::white);
  g.setFont(24.0f);
  g.drawFittedText("soundsgood", getLocalBounds().removeFromLeft(300),
                   juce::Justification::centredTop, 1);

  // Draw current waveform display
  g.setColour(juce::Colours::lightgrey);
  g.setFont(16.0f);
  g.drawFittedText("Current: " + getWaveformName(audioProcessor.getWaveform()),
                   juce::Rectangle<int>(250, 30, 130, 40),
                   juce::Justification::centred, 1);
}

void SoundsgoodAudioProcessorEditor::resized() {
  // Update component bounds when window is resized
  auto bounds = getLocalBounds();

  // Waveform selector
  waveformComboBox->setBounds(bounds.getX() + 50, bounds.getY() + 30, 200, 40);

  // Frequency slider
  frequencySlider->setBounds(bounds.getX() + 50, bounds.getY() + 90, 300, 120);

  // Labels
  waveformLabel->setBounds(bounds.getX() + 50, bounds.getY() + 10, 100, 25);
  frequencyLabel->setBounds(bounds.getX() + 50, bounds.getY() + 70, 100, 25);
}

void SoundsgoodAudioProcessorEditor::sliderValueChanged(
    juce::Slider *sliderThatHasChanged) {
  if (sliderThatHasChanged == frequencySlider.get()) {
    audioProcessor.setFrequency(frequencySlider->getValue());
  }
}

juce::String SoundsgoodAudioProcessorEditor::getWaveformName(
    SoundsgoodAudioProcessor::Waveform waveform) {
  switch (waveform) {
    case SoundsgoodAudioProcessor::Waveform::sine:
      return "Sine";
    case SoundsgoodAudioProcessor::Waveform::square:
      return "Square";
    case SoundsgoodAudioProcessor::Waveform::saw:
      return "Saw";
    default:
      return "Unknown";
  }
}
