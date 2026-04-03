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
                                         private IGlintLCDRefreshEventListener
{
public:
    GlintVSTAudioProcessorEditor (GlintVSTAudioProcessor&);
    ~GlintVSTAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void sliderValueChanged (juce::Slider* slider) override;
    void buttonClicked (juce::Button* button) override;
    void onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent) override;

    void copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    GlintVSTAudioProcessor& audioProcessor;

    juce::Slider decayTimeSldr;
    juce::Label decayTimeLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayTimeSldrAttachment;

    juce::Slider diffusionSldr;
    juce::Label diffusionLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> diffusionSldrAttachment;

    juce::Slider filtFreqSldr;
    juce::Label filtFreqLbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filtFreqSldrAttachment;

    juce::TextButton prevPresetBtn;
    juce::Label presetNumLbl;
    juce::TextButton nextPresetBtn;
    juce::TextButton writePresetBtn;

    juce::Image screenRep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlintVSTAudioProcessorEditor)
};
