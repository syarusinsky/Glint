/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "AudioConstants.hpp"
#include "FrameBuffer.hpp"

//==============================================================================
GlintVSTAudioProcessorEditor::GlintVSTAudioProcessorEditor (GlintVSTAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      decayTimeSldr(),
      decayTimeLbl(),
      decayTimeSldrAttachment( std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getVTS(), "decayTime", decayTimeSldr) ),
      diffusionSldr(),
      diffusionLbl(),
      diffusionSldrAttachment( std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getVTS(), "diffusion", diffusionSldr) ),
      filtFreqSldr(),
      filtFreqLbl(),
      filtFreqSldrAttachment( std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getVTS(), "filtFreq", filtFreqSldr) ),
      prevPresetBtn( "Prev Preset" ),
      presetNumLbl( "Preset Number", "1" ),
      nextPresetBtn( "Next Preset" ),
      writePresetBtn( "Write Preset" ),
      screenRep( juce::Image::RGB, 256, 128, true ) // this is actually double the size so we can actually see it
{
    // adding all child components
    addAndMakeVisible( decayTimeSldr );
    decayTimeSldr.setTextValueSuffix( "Seconds" );
    decayTimeSldr.addListener( this );
    addAndMakeVisible( decayTimeLbl );
    decayTimeLbl.setText( "Decay Time", juce::dontSendNotification );
    decayTimeLbl.attachToComponent( &decayTimeSldr, true );

    addAndMakeVisible( diffusionSldr );
    diffusionSldr.setTextValueSuffix( "%" );
    diffusionSldr.addListener( this );
    addAndMakeVisible( diffusionLbl );
    diffusionLbl.setText( "Diffusion", juce::dontSendNotification );
    diffusionLbl.attachToComponent( &diffusionSldr, true );

    addAndMakeVisible( filtFreqSldr );
    filtFreqSldr.setTextValueSuffix( "Hz" );
    filtFreqSldr.addListener( this );
    addAndMakeVisible( filtFreqLbl );
    filtFreqLbl.setText( "LPF Freq", juce::dontSendNotification );
    filtFreqLbl.attachToComponent( &filtFreqSldr, true );

    addAndMakeVisible( prevPresetBtn );
    prevPresetBtn.addListener( this );

    addAndMakeVisible( presetNumLbl );

    addAndMakeVisible( nextPresetBtn );
    nextPresetBtn.addListener( this );

    addAndMakeVisible( writePresetBtn );
    writePresetBtn.addListener( this );

    setSize( 800, 600 );

    this->bindToGlintLCDRefreshEventSystem();

    // set initial values
    float decayTimeSldrPercentage = (decayTimeSldr.getValue() - decayTimeSldr.getMinimum()) / (decayTimeSldr.getMaximum() - decayTimeSldr.getMinimum());
    float diffusionSldrPercentage = (diffusionSldr.getValue() - diffusionSldr.getMinimum()) / (diffusionSldr.getMaximum() - diffusionSldr.getMinimum());
    float filtFreqSldrPercentage = (filtFreqSldr.getValue() - filtFreqSldr.getMinimum()) / (filtFreqSldr.getMaximum() - filtFreqSldr.getMinimum());
    IPotEventListener::PublishEvent( PotEvent(decayTimeSldrPercentage, static_cast<unsigned int>(POT_CHANNEL::DECAY_TIME)) );
    IPotEventListener::PublishEvent( PotEvent(diffusionSldrPercentage, static_cast<unsigned int>(POT_CHANNEL::DIFFUSION)) );
    IPotEventListener::PublishEvent( PotEvent(filtFreqSldrPercentage, static_cast<unsigned int>(POT_CHANNEL::FILT_FREQ)) );

    // draw the target ui
    audioProcessor.getGlintUiManager().draw();
}

GlintVSTAudioProcessorEditor::~GlintVSTAudioProcessorEditor()
{
}

//==============================================================================
void GlintVSTAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll( getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId) );

    // You can add your drawing code here!
    g.drawImageWithin( screenRep, 0, 150, getWidth(), 120, juce::RectanglePlacement::centred | juce::RectanglePlacement::doNotResize );
}

void GlintVSTAudioProcessorEditor::resized()
{
    int sliderLeft = 120;
    decayTimeSldr.setBounds 	(sliderLeft, 20, getWidth() - sliderLeft - 10, 20);
    diffusionSldr.setBounds 	(sliderLeft, 60, getWidth() - sliderLeft - 10, 20);
    filtFreqSldr.setBounds 	(sliderLeft, 100, getWidth() - sliderLeft - 10, 20);
    prevPresetBtn.setBounds 	(sliderLeft + (getWidth() / 5) * 1, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
    presetNumLbl.setBounds 	(sliderLeft + (getWidth() / 5) * 2, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
    nextPresetBtn.setBounds 	((getWidth() / 5) * 3, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
    writePresetBtn.setBounds 	((getWidth() / 5) * 4, 1010, ((getWidth() - sliderLeft - 10) / 5), 20);
}

void GlintVSTAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    double val = slider->getValue();
    float percentage = (slider->getValue() - slider->getMinimum()) / (slider->getMaximum() - slider->getMinimum());

    if (slider == &decayTimeSldr)
    {
        IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::DECAY_TIME)) );
    }
    else if (slider == &diffusionSldr)
    {
        IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::DIFFUSION)) );
    }
    else if (slider == &filtFreqSldr)
    {
        IPotEventListener::PublishEvent( PotEvent(percentage, static_cast<unsigned int>(POT_CHANNEL::FILT_FREQ)) );
    }

    // TODO not a good way to test the target's ui since we should be only updating the dirty part of the screen, but for now I'm lazy
    audioProcessor.getGlintUiManager().draw();
}

void GlintVSTAudioProcessorEditor::buttonClicked (juce::Button* button)
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

void GlintVSTAudioProcessorEditor::onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent)
{
    this->copyFrameBufferToImage( lcdRefreshEvent.getXStart(), lcdRefreshEvent.getYStart(),
                                  lcdRefreshEvent.getXEnd(), lcdRefreshEvent.getYEnd() );
    this->repaint();
}

void GlintVSTAudioProcessorEditor::copyFrameBufferToImage (unsigned int xStart, unsigned int yStart, unsigned int xEnd, unsigned int yEnd)
{
    GlintUiManager& glintUiManager = audioProcessor.getGlintUiManager();
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
