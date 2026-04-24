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
#include "GlintPresetUpgrader.hpp"
#include "CPPFile.hpp"
#include "ColorProfile.hpp"
#include "FrameBuffer.hpp"
#include "Font.hpp"
#include "Sprite.hpp"
#include "SRAM_23K256.hpp"

// assets
#include "GlintMainImage.h"
#include "Smoll.h"

//==============================================================================
MainComponent::MainComponent() :
	sAudioBuffer(),
	fakeStorageDevice( Sram_23K256::SRAM_SIZE * 4 ), // sram size on Gen_FX_SYN boards, with four srams installed
	presetManager( sizeof(GlintPresetHeader), 20, new CPPFile("GlintPresets.spf") ),
	glintManager( &fakeStorageDevice, &presetManager ),
	glintUiManager( Smoll_data, GlintMainImage_data ),
	sampleRateConverter( 96000, SAMPLE_RATE, 512 ),
	writer(),
	effect1Sldr(),
	effect1Lbl(),
	effect2Sldr(),
	effect2Lbl(),
	effect3Sldr(),
	effect3Lbl(),
	effect1Btn( "Effect 1" ),
	effect2Btn( "Effect 2" ),
	audioSettingsBtn( "Audio Settings" ),
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
	addAndMakeVisible( effect1Sldr );
	effect1Sldr.setRange( 0.0f, 1.0f );
	effect1Sldr.setTextValueSuffix( "%" );
	effect1Sldr.addListener( this );
	addAndMakeVisible( effect1Lbl );
	effect1Lbl.setText( "Decay Time", juce::dontSendNotification );
	effect1Lbl.attachToComponent( &effect1Sldr, true );

	addAndMakeVisible( effect2Sldr );
	effect2Sldr.setRange( 0.0f, 1.0f );
	effect2Sldr.setTextValueSuffix( "%" );
	effect2Sldr.addListener( this );
	addAndMakeVisible( effect2Lbl );
	effect2Lbl.setText( "Diffusion", juce::dontSendNotification );
	effect2Lbl.attachToComponent( &effect2Sldr, true );

	addAndMakeVisible( effect3Sldr );
	effect3Sldr.setRange( 1, 20000 );
	effect3Sldr.setValue( 20000, juce::dontSendNotification );
	effect3Sldr.setTextValueSuffix( "Hz" );
	effect3Sldr.addListener( this );
	addAndMakeVisible( effect3Lbl );
	effect3Lbl.setText( "LPF Freq", juce::dontSendNotification );
	effect3Lbl.attachToComponent( &effect3Sldr, true );

	addAndMakeVisible( effect1Btn );
	effect1Btn.addListener( this );

	addAndMakeVisible( effect2Btn );
	effect2Btn.addListener( this );

	addAndMakeVisible( audioSettingsBtn );
	audioSettingsBtn.addListener( this );

	// Make sure you set the size of the component after
	// you add any child components.
	setSize( 800, 600 );

	// upgrade presets if necessary
	GlintState initPreset = { 0.0f, 0.0f, 20000.0f };
	GlintPresetUpgrader presetUpgrader( initPreset, glintManager.getPresetHeader() );
	presetManager.upgradePresets( &presetUpgrader );

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
	glintUiManager.processEffect1Btn( effect1Btn.isDown() );
	glintUiManager.processEffect2Btn( effect2Btn.isDown() );
	double effect1Val = effect1Sldr.getValue();
	float effect1Percentage = ( effect1Sldr.getValue() - effect1Sldr.getMinimum() )
					/ ( effect1Sldr.getMaximum() - effect1Sldr.getMinimum() );
	double effect2Val = effect2Sldr.getValue();
	float effect2Percentage = ( effect2Sldr.getValue() - effect2Sldr.getMinimum() )
					/ ( effect2Sldr.getMaximum() - effect2Sldr.getMinimum() );
	double effect3Val = effect3Sldr.getValue();
	float effect3Percentage = ( effect3Sldr.getValue() - effect3Sldr.getMinimum() )
					/ ( effect3Sldr.getMaximum() - effect3Sldr.getMinimum() );
	IPotEventListener::PublishEvent( PotEvent(effect1Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT1)) );
	IPotEventListener::PublishEvent( PotEvent(effect2Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT2)) );
	IPotEventListener::PublishEvent( PotEvent(effect3Percentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT3)) );
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, block size, etc) are changed.

	// You can use this function to initialise any resources you might need,
	// but be careful - it will be called on the audio thread, not the GUI thread.

	// For more details, see the help for AudioProcessor::prepareToPlay()
	sampleRateConverter.setSourceRate( static_cast<unsigned int>(sampleRate) );
	sampleRateConverter.setSourceBufferSize( samplesPerBlockExpected );
	sampleRateConverter.resetAAFilters();
	std::cout << "prepareToPlay called! rate=" << std::to_string(sampleRateConverter.getSourceRate())
		<< ", bufferSize=" << std::to_string(sampleRateConverter.getSourceBufferSize())	<< std::endl;
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

		// if downsampling, anti-alias filter the source
		if ( ! sampleRateConverter.sourceToTargetIsUpsampling() )
		{
			float* inBufferLNonConst = const_cast<float*>( inBufferL );
			sampleRateConverter.filterSourceToTargetDownsampling( inBufferLNonConst );
		}

		const unsigned int maxTargetBufferSize = static_cast<unsigned int>( std::ceil(sampleRateConverter.getFractionalTargetBufferSize()) );
		uint16_t targetBuffer[ maxTargetBufferSize ]; // ceil, since can be fractional

		const unsigned int actualTargetBufferSize = sampleRateConverter.convertFromSourceToTargetDownsampling( inBufferL, targetBuffer );

		// then pass this audio into the target
		for ( unsigned int sample = 0;
				sample < actualTargetBufferSize;
				sample++ )
		{
			targetBuffer[sample] = sAudioBuffer.getNextSample( targetBuffer[sample] );
			sAudioBuffer.pollToFillBuffers();
		}

		// now we need to convert back
		sampleRateConverter.convertFromTargetToSourceUpsampling( targetBuffer, actualTargetBufferSize, outBufferL );

		// if upsampling, anti-alias filter the source
		if ( sampleRateConverter.targetToSourceIsUpsampling() )
		{
			sampleRateConverter.filterTargetToSourceUpsampling( outBufferL );
		}

		for ( auto sample = bufferToFill.startSample; sample < bufferToFill.numSamples; sample++ )
		{
			outBufferR[sample] = outBufferL[sample];
		}
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
	effect1Sldr.setBounds 		(sliderLeft, 20, getWidth() - sliderLeft - 10, 20);
	effect2Sldr.setBounds 		(sliderLeft, 60, getWidth() - sliderLeft - 10, 20);
	effect3Sldr.setBounds 		(sliderLeft, 100, getWidth() - sliderLeft - 10, 20);
	effect1Btn.setBounds      	(sliderLeft, 300, (getWidth() / 2) - sliderLeft - 10, 20);
	effect2Btn.setBounds      	(sliderLeft, 340, (getWidth() / 2) - sliderLeft - 10, 20);
	audioSettingsBtn.setBounds 	(sliderLeft, 950, getWidth() - sliderLeft - 10, 20);
}

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
	// for holding both buttons down at the same time
	if ( k.getTextCharacter() == 'z' )
	{
		effect1Btn.setState( juce::Button::ButtonState::buttonDown );
		effect2Btn.setState( juce::Button::ButtonState::buttonDown );
	}
	else if ( k.getTextCharacter() == '9' )
	{
		effect1Btn.setState( juce::Button::ButtonState::buttonDown );
	}
	else if ( k.getTextCharacter() == '0' )
	{
		effect2Btn.setState( juce::Button::ButtonState::buttonDown );
	}

	return true;
}

bool MainComponent::keyStateChanged (bool isKeyDown)
{
	if ( ! isKeyDown ) // if a key has been released
	{
		effect1Btn.setState( juce::Button::ButtonState::buttonNormal );
		effect2Btn.setState( juce::Button::ButtonState::buttonNormal );
	}

	return true;
}

void MainComponent::sliderValueChanged (juce::Slider* slider)
{
	try
	{
		// now polled in timer callback
		/*
		double val = slider->getValue();
		float percentage = (slider->getValue() - slider->getMinimum()) / (slider->getMaximum() - slider->getMinimum());

		if (slider == &decayTimeSldr)
		{
			IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::DECAY_TIME)) );
		}
		else if (slider == &modRateSldr)
		{
			IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::DIFFUSION)) );
		}
		else if (slider == &filtFreqSldr)
		{
			IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::FILT_FREQ)) );
		}
		*/
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
