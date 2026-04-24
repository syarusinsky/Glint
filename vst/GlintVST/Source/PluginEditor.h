/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class GlintVSTAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Slider::Listener, private juce::Button::Listener,
                                         private IGlintLCDRefreshEventListener, private IGlintPresetEventListener, private juce::Timer
{
public:
    GlintVSTAudioProcessorEditor (GlintVSTAudioProcessor&);
    ~GlintVSTAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    unsigned int getProcessorEditorId() { return processorEditorId; }

private:
    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;
    void timerCallback() override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool keyStateChanged (bool isKeyDown) override;
    void onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent) override;
    void onGlintPresetChangedEvent (const GlintPresetEvent& presetEvent) override;

    void copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    GlintVSTAudioProcessor& audioProcessor;

    juce::Slider effect1Sldr;
    juce::Label effect1Lbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> effect1SldrAttachment;

    juce::Slider effect2Sldr;
    juce::Label effect2Lbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> effect2SldrAttachment;

    juce::Slider effect3Sldr;
    juce::Label effect3Lbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> effect3SldrAttachment;

    juce::TextButton effect1Btn;
    juce::TextButton effect2Btn;

    juce::Image screenRep;

    unsigned int processorEditorId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlintVSTAudioProcessorEditor)
};
