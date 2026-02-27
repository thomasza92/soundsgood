#pragma once
#include <JuceHeader.h>

class EditableComponent : public juce::Component {
 public:
  EditableComponent() : border(this, nullptr) {
#if JUCE_DEBUG
    addAndMakeVisible(border);
#endif
  }

 private:
  juce::ResizableBorderComponent border;
};