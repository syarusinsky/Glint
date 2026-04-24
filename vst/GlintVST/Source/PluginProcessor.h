/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "AudioBuffer.hpp"
#include "FakeStorageDevice.hpp"
#include "GlintManager.hpp"
#include "GlintUiManager.hpp"
#include "IGlintLCDRefreshEventListener.hpp"
#include "SampleRateConverter.hpp"
#include "PresetManager.hpp"

#include <JuceHeader.h>

//==============================================================================
/**
*/
class GlintVSTAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    GlintVSTAudioProcessor();
    ~GlintVSTAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    GlintUiManager& getGlintUiManager() { return glintUiManager; }

    AudioProcessorValueTreeState& getVTS() { return apvts; }

    // this is a workaround for the fact that vst plugins share memory across multiple
    // instances, so the static member variables of the event listeners end up sending
    // events to all instances of the plugin
    void dispatchEventsForIds (unsigned int processorId, const unsigned int processorEditorId);
    unsigned int getProcessorId() { return processorId; }

private:
    ::AudioBuffer<uint16_t> sAudioBuffer;

    FakeStorageDevice fakeStorageDevice;

    PresetManager presetManager;

    GlintManager glintManager;
    GlintUiManager glintUiManager;

    SampleRateConverter<float, uint16_t> sampleRateConverter;

    UndoManager undoManager;
    AudioProcessorValueTreeState apvts;

    unsigned int processorId;
    unsigned int processorEditorId = 0 - 1; // this must be correctly initialized on the create editor function

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlintVSTAudioProcessor)
};
