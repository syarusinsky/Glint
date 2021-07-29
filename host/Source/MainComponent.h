/*
   ==============================================================================

   This file was auto-generated!

   ==============================================================================
   */

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "AudioBuffer.hpp"
#include "AudioSettingsComponent.h"
#include "FakeStorageDevice.hpp"
#include "GlintManager.hpp"
#include "GlintUiManager.hpp"
#include "IGlintLCDRefreshEventListener.hpp"

#include <iostream>
#include <fstream>

//==============================================================================
/*
   This component lives inside our window, and this is where you should put all
   your controls and content.
   */
class MainComponent   : public juce::AudioAppComponent, public juce::Slider::Listener, public juce::Button::Listener,
			public juce::Timer, public IGlintLCDRefreshEventListener
{
	public:
		//==============================================================================
		MainComponent();
		~MainComponent();

		//==============================================================================
		void timerCallback() override;

		//==============================================================================
		void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
		void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
		void releaseResources() override;

		//==============================================================================
		void paint (juce::Graphics& g) override;
		void resized() override;

		void sliderValueChanged (juce::Slider* slider) override;
		void buttonClicked (juce::Button* button) override;
		void updateToggleState (juce::Button* button);

		void onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent) override;

	private:
		//==============================================================================
		// Your private member variables go here...
		::AudioBuffer<uint16_t> sAudioBuffer;

		FakeStorageDevice fakeStorageDevice;

		GlintManager glintManager;
		GlintUiManager glintUiManager;

		juce::AudioFormatWriter* writer;

		juce::Slider decayTimeSldr;
		juce::Label decayTimeLbl;

		juce::Slider modRateSldr;
		juce::Label modRateLbl;

		juce::Slider filtFreqSldr;
		juce::Label filtFreqLbl;

		juce::TextButton audioSettingsBtn;

		juce::TextButton prevPresetBtn;
		juce::Label presetNumLbl;
		juce::TextButton nextPresetBtn;
		juce::TextButton writePresetBtn;

		AudioSettingsComponent audioSettingsComponent;

		juce::Image screenRep;

		std::ofstream testFile;

		void copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd);

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
