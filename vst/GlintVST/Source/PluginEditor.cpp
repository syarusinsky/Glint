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
      effect1Sldr(),
      effect1Lbl(),
      effect1SldrAttachment( std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getVTS(), "effect1", effect1Sldr) ),
      effect2Sldr(),
      effect2Lbl(),
      effect2SldrAttachment( std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getVTS(), "effect2", effect2Sldr) ),
      effect3Sldr(),
      effect3Lbl(),
      effect3SldrAttachment( std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getVTS(), "effect3", effect3Sldr) ),
      effect1Btn( "Effect 1" ),
      effect2Btn( "Effect 2" ),
      screenRep( juce::Image::RGB, 256, 128, true ), // this is actually double the size so we can actually see it
      processorEditorId( IEventListener::getGlobalJuceProcessorId() )
{
    // adding all child components
    addAndMakeVisible( effect1Sldr );
    effect1Sldr.setTextValueSuffix( "Seconds" );
    effect1Sldr.addListener( this );
    addAndMakeVisible( effect1Lbl );
    effect1Lbl.setText( "Decay Time", juce::dontSendNotification );
    effect1Lbl.attachToComponent( &effect1Sldr, true );

    addAndMakeVisible( effect2Sldr );
    effect2Sldr.setTextValueSuffix( "%" );
    effect2Sldr.addListener( this );
    addAndMakeVisible( effect2Lbl );
    effect2Lbl.setText( "Diffusion", juce::dontSendNotification );
    effect2Lbl.attachToComponent( &effect2Sldr, true );

    addAndMakeVisible( effect3Sldr );
    effect3Sldr.setTextValueSuffix( "Hz" );
    effect3Sldr.addListener( this );
    addAndMakeVisible( effect3Lbl );
    effect3Lbl.setText( "LPF Freq", juce::dontSendNotification );
    effect3Lbl.attachToComponent( &effect3Sldr, true );

    addAndMakeVisible( effect1Btn );
    effect1Btn.addListener( this );

    addAndMakeVisible( effect2Btn );
    effect2Btn.addListener( this );

    setSize( 800, 600 );

    this->bindToGlintLCDRefreshEventSystem();
    this->bindToGlintPresetEventSystem();

    this->startTimer( 33 );

    // draw the target ui
    audioProcessor.getGlintUiManager().draw();
}

GlintVSTAudioProcessorEditor::~GlintVSTAudioProcessorEditor()
{
}

void GlintVSTAudioProcessorEditor::timerCallback()
{
    GlintUiManager& glintUiManager = audioProcessor.getGlintUiManager();
    glintUiManager.processEffect1Btn( effect1Btn.isDown() );

    // since the effect button holding logic requires the sequencing of the button events to be in order, we need to dispatch here as well
    audioProcessor.dispatchEventsForIds( audioProcessor.getProcessorId(), processorEditorId );

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

    audioProcessor.dispatchEventsForIds( audioProcessor.getProcessorId(), processorEditorId );
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
    effect1Sldr.setBounds 	(sliderLeft, 20, getWidth() - sliderLeft - 10, 20);
    effect2Sldr.setBounds 	(sliderLeft, 60, getWidth() - sliderLeft - 10, 20);
    effect3Sldr.setBounds 	(sliderLeft, 100, getWidth() - sliderLeft - 10, 20);
    effect1Btn.setBounds      	(sliderLeft, 300, (getWidth() / 2) - sliderLeft - 10, 20);
    effect2Btn.setBounds      	(sliderLeft, 340, (getWidth() / 2) - sliderLeft - 10, 20);
}

void GlintVSTAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
}

void GlintVSTAudioProcessorEditor::buttonClicked (juce::Button* button)
{
}

bool GlintVSTAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
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

bool GlintVSTAudioProcessorEditor::keyStateChanged (bool isKeyDown)
{
    if ( ! isKeyDown ) // if a key has been released
    {
        effect1Btn.setState( juce::Button::ButtonState::buttonNormal );
        effect2Btn.setState( juce::Button::ButtonState::buttonNormal );
    }

    return true;
}

void GlintVSTAudioProcessorEditor::onGlintLCDRefreshEvent (const GlintLCDRefreshEvent& lcdRefreshEvent)
{
    this->copyFrameBufferToImage( lcdRefreshEvent.getXStart(), lcdRefreshEvent.getYStart(),
                                  lcdRefreshEvent.getXEnd(), lcdRefreshEvent.getYEnd() );
    this->repaint();
}

void GlintVSTAudioProcessorEditor::onGlintPresetChangedEvent (const GlintPresetEvent& presetEvent)
{
    GlintState state = presetEvent.getPreset();
    effect1Sldr.setValue( state.m_DecayTime );
    effect2Sldr.setValue( state.m_Diffusion );
    effect3Sldr.setValue( state.m_FiltFreq );
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
