/*
   ==============================================================================

   This file was auto-generated!

   ==============================================================================
   */

#ifdef __unix__
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

#include "MainComponent.h"

#include "GlintConstants.hpp"
#include "CPPFile.hpp"
#include "ColorProfile.hpp"
#include "FrameBuffer.hpp"
#include "Font.hpp"
#include "Sprite.hpp"
#include "SRAM_23K256.hpp"

// assets
#include "GlintMainImage.h"
#include "Smoll.h"

static bool resetMaxAndMins = false;

//==============================================================================
MainComponent::MainComponent() :
	sAudioBuffer(),
	fakeStorageDevice( Sram_23K256::SRAM_SIZE * 4 ), // sram size on Gen_FX_SYN boards, with four srams installed
	glintManager( &fakeStorageDevice ),
	glintUiManager( Smoll_data, GlintMainImage_data ),
	writer(),
	decayTimeSldr(),
	decayTimeLbl(),
	modRateSldr(),
	modRateLbl(),
	filtFreqSldr(),
	filtFreqLbl(),
	audioSettingsBtn( "Audio Settings" ),
	prevPresetBtn( "Prev Preset" ),
	presetNumLbl( "Preset Number", "1" ),
	nextPresetBtn( "Next Preset" ),
	writePresetBtn( "Write Preset" ),
	audioSettingsComponent( deviceManager, 2, 2, &audioSettingsBtn ),
	screenRep( juce::Image::RGB, 256, 128, true ) // this is actually double the size so we can actually see it
{
	// Some platforms require permissions to open input channels so request that here
	if ( juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
			&& ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio) )
	{
		juce::RuntimePermissions::request( juce::RuntimePermissions::recordAudio,
				[&] (bool granted) { if (granted)  setAudioChannels (2, 2); } );
	}
	else
	{
		// Specify the number of input and output channels that we want to open
		setAudioChannels (2, 2);
	}

	// juce audio device setup
	juce::AudioDeviceManager::AudioDeviceSetup deviceSetup = juce::AudioDeviceManager::AudioDeviceSetup();
	deviceSetup.sampleRate = 96000;
	deviceManager.initialise( 2, 2, 0, true, juce::String(), &deviceSetup );

	// adding all child components
	addAndMakeVisible( decayTimeSldr );
	decayTimeSldr.setRange( 0.0f, 1.0f );
	decayTimeSldr.setTextValueSuffix( "%" );
	decayTimeSldr.addListener( this );
	addAndMakeVisible( decayTimeLbl );
	decayTimeLbl.setText( "Decay Time", juce::dontSendNotification );
	decayTimeLbl.attachToComponent( &decayTimeSldr, true );

	addAndMakeVisible( modRateSldr );
	modRateSldr.setRange( 0.0f, 1.0f );
	modRateSldr.setTextValueSuffix( "%" );
	modRateSldr.addListener( this );
	addAndMakeVisible( modRateLbl );
	modRateLbl.setText( "Mod Rate", juce::dontSendNotification );
	modRateLbl.attachToComponent( &modRateSldr, true );

	addAndMakeVisible( filtFreqSldr );
	filtFreqSldr.setRange( 1, 20000 );
	filtFreqSldr.setValue( 20000, juce::dontSendNotification );
	filtFreqSldr.setTextValueSuffix( "Hz" );
	filtFreqSldr.addListener( this );
	addAndMakeVisible( filtFreqLbl );
	filtFreqLbl.setText( "LPF Freq", juce::dontSendNotification );
	filtFreqLbl.attachToComponent( &filtFreqSldr, true );

	addAndMakeVisible( audioSettingsBtn );
	audioSettingsBtn.addListener( this );

	addAndMakeVisible( prevPresetBtn );
	prevPresetBtn.addListener( this );

	addAndMakeVisible( presetNumLbl );

	addAndMakeVisible( nextPresetBtn );
	nextPresetBtn.addListener( this );

	addAndMakeVisible( writePresetBtn );
	writePresetBtn.addListener( this );

	// Make sure you set the size of the component after
	// you add any child components.
	setSize( 800, 600 );

	// start timer for fake loading
	this->startTimer( 33 );

	sAudioBuffer.registerCallback( &glintManager );

	this->bindToGlintLCDRefreshEventSystem();

	glintUiManager.draw();
}

MainComponent::~MainComponent()
{
	// This shuts down the audio device and clears the audio source.
	shutdownAudio();
	delete writer;
	testFile.close();
}

void MainComponent::timerCallback()
{
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, block size, etc) are changed.

	// You can use this function to initialise any resources you might need,
	// but be careful - it will be called on the audio thread, not the GUI thread.

	// For more details, see the help for AudioProcessor::prepareToPlay()
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
	// Your audio-processing code goes here!

	// For more details, see the help for AudioProcessor::getNextAudioBlock()
	try
	{
		const float* inBufferL = bufferToFill.buffer->getReadPointer( 0, bufferToFill.startSample );
		const float* inBufferR = bufferToFill.buffer->getReadPointer( 1, bufferToFill.startSample );
		float* outBufferL = bufferToFill.buffer->getWritePointer( 0, bufferToFill.startSample );
		float* outBufferR = bufferToFill.buffer->getWritePointer( 1, bufferToFill.startSample );

		static uint16_t maxOut = 0;
		static uint16_t minOut = 0;
		if ( resetMaxAndMins ) { maxOut = 0; minOut = 0; resetMaxAndMins = false; }
		for ( auto sample = bufferToFill.startSample; sample < bufferToFill.numSamples; ++sample )
		{
			uint16_t sampleToReadBuffer = ( (inBufferL[sample] + 1.0f) * 0.5f ) * 4096.0f;
			uint16_t sampleOut = sAudioBuffer.getNextSample( sampleToReadBuffer );
			float sampleOutFloat = static_cast<float>( ((sampleOut / 4096.0f) * 2.0f) - 1.0f );
			if ( sampleOut > maxOut ) maxOut = sampleOut;
			if ( sampleOut < minOut ) minOut = sampleOut;
			outBufferL[sample] = sampleOutFloat;
			outBufferR[sample] = sampleOutFloat;
		}
		// std::cout << "maxOut: " << std::to_string(maxOut) << std::endl;
		// std::cout << "minOut: " << std::to_string(minOut) << std::endl;

		sAudioBuffer.pollToFillBuffers();
	}
	catch ( std::exception& e )
	{
		std::cout << "Exception caught in getNextAudioBlock: " << e.what() << std::endl;
	}
}

void MainComponent::releaseResources()
{
	// This will be called when the audio device stops, or when it is being
	// restarted due to a setting change.
	//
	// For more details, see the help for AudioProcessor::releaseResources()
	std::cout << "Resources released, resetting max and min" << std::endl;
	resetMaxAndMins = true;
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
	// (Our component is opaque, so we must completely fill the background with a solid colour)
	g.fillAll( getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId) );

	// You can add your drawing code here!
	g.drawImageWithin( screenRep, 0, 150, getWidth(), 120, juce::RectanglePlacement::centred | juce::RectanglePlacement::doNotResize );
}

void MainComponent::resized()
{
	// This is called when the MainContentComponent is resized.
	// If you add any child components, this is where you should
	// update their positions.
	int sliderLeft = 120;
	decayTimeSldr.setBounds 	(sliderLeft, 20, getWidth() - sliderLeft - 10, 20);
	modRateSldr.setBounds 		(sliderLeft, 60, getWidth() - sliderLeft - 10, 20);
	filtFreqSldr.setBounds 		(sliderLeft, 100, getWidth() - sliderLeft - 10, 20);
	audioSettingsBtn.setBounds 	(sliderLeft, 950, getWidth() - sliderLeft - 10, 20);
	prevPresetBtn.setBounds 	(sliderLeft + (getWidth() / 5) * 1, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
	presetNumLbl.setBounds 		(sliderLeft + (getWidth() / 5) * 2, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
	nextPresetBtn.setBounds 	((getWidth() / 5) * 3, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
	writePresetBtn.setBounds 	((getWidth() / 5) * 4, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
	try
	{
		double val = slider->getValue();
		float percentage = (slider->getValue() - slider->getMinimum()) / (slider->getMaximum() - slider->getMinimum());

		if (slider == &decayTimeSldr)
		{
			IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::DECAY_TIME)) );
		}
		else if (slider == &modRateSldr)
		{
			IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::MOD_RATE)) );
		}
		else if (slider == &filtFreqSldr)
		{
			IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::FILT_FREQ)) );
		}
	}
	catch (std::exception& e)
	{
		std::cout << "Exception caught in slider handler: " << e.what() << std::endl;
	}
}

void MainComponent::buttonClicked (juce::Button* button)
{
	try
	{
		// TODO reimplement these
		/*
		if (button == &prevPresetBtn)
		{
			uiSim.processPrevPresetBtn( true ); // pressed
			uiSim.processPrevPresetBtn( false ); // released
			uiSim.processPrevPresetBtn( false ); // floating
		}
		else if (button == &nextPresetBtn)
		{
			uiSim.processNextPresetBtn( true ); // pressed
			uiSim.processNextPresetBtn( false ); // released
			uiSim.processNextPresetBtn( false ); // floating
		}
		else if (button == &writePresetBtn)
		{
			uiSim.processWritePresetBtn( true ); // pressed
			uiSim.processWritePresetBtn( false ); // released
			uiSim.processWritePresetBtn( false ); // floating
		}
		*/
	}
	catch (std::exception& e)
	{
		std::cout << "Exception caught in buttonClicked(): " << e.what() << std::endl;
	}
}

void MainComponent::updateToggleState (juce::Button* button)
{
	try
	{
		bool isPressed = button->getToggleState();
	}
	catch (std::exception& e)
	{
		std::cout << "Exception in toggle button shit: " << e.what() << std::endl;
	}
}

void MainComponent::onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent)
{
	this->copyFrameBufferToImage( lcdRefreshEvent.getXStart(), lcdRefreshEvent.getYStart(),
					lcdRefreshEvent.getXEnd(), lcdRefreshEvent.getYEnd() );
	this->repaint();
}

void MainComponent::copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd)
{
	ColorProfile* colorProfile = glintUiManager.getColorProfile();
	FrameBuffer* frameBuffer = glintUiManager.getFrameBuffer();
	unsigned int frameBufferWidth = frameBuffer->getWidth();

	for ( unsigned int pixelY = yStart; pixelY < yEnd + 1; pixelY++ )
	{
		for ( unsigned int pixelX = xStart; pixelX < xEnd + 1; pixelX++ )
		{
			if ( ! colorProfile->getPixel(frameBuffer->getPixels(), frameBufferWidth * frameBuffer->getHeight(), (pixelY * frameBufferWidth) + pixelX).m_M )
			{
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2),     juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2),     juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2) + 1, juce::Colour(0, 0, 0) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2) + 1, juce::Colour(0, 0, 0) );
			}
			else
			{
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2),     juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2),     juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2),     (pixelY * 2) + 1, juce::Colour(0, 97, 252) );
				screenRep.setPixelAt( (pixelX * 2) + 1, (pixelY * 2) + 1, juce::Colour(0, 97, 252) );
			}
		}
	}
}
