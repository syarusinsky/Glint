/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "GlintConstants.hpp"
#include "GlintPresetUpgrader.hpp"
#include "CPPFile.hpp"
#include "SRAM_23K256.hpp"

// assets
#include "GlintMainImage.h"
#include "Smoll.h"

//==============================================================================
GlintVSTAudioProcessor::GlintVSTAudioProcessor()
    : sAudioBuffer(),
      fakeStorageDevice( Sram_23K256::SRAM_SIZE * 4 ), // sram size on Gen_FX_SYN boards, with four srams installed
      presetManager( sizeof(GlintPresetHeader), 20, new CPPFile("GlintPresets.spf") ),
      glintManager( &fakeStorageDevice, &presetManager ),
      glintUiManager( Smoll_data, GlintMainImage_data ),
      sampleRateConverter( 96000, SAMPLE_RATE, 512 ),
      undoManager(),
      apvts( *this, &undoManager, "PARAMETERS",
                                  { std::make_unique<AudioParameterFloat> ("effect1", "Decay Time", NormalisableRange<float> (0.0f, 1.0f), 0),
                                    std::make_unique<AudioParameterFloat> ("effect2", "Diffusion", NormalisableRange<float> (0.0f, 1.0f), 0),
                                    std::make_unique<AudioParameterInt> ("effect3", "Filter Freq", 1, 20000, 20000),
                                  }),
      processorId( IEventListener::getGlobalJuceProcessorId() )
#ifndef JucePlugin_PreferredChannelConfigurations
      , AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // clear storage device to prevent noise
    SharedData<uint8_t> data = SharedData<uint8_t>::MakeSharedData( Sram_23K256::SRAM_SIZE * 4 );
    for ( unsigned int byteNum = 0; byteNum < Sram_23K256::SRAM_SIZE * 4; byteNum++ )
    {
        data[byteNum] = 32768;
    }
    fakeStorageDevice.writeToMedia( data, 0 );

    // upgrade presets if necessary
    GlintState initPreset = { 0.0f, 0.0f, 20000.0f };
    GlintPresetUpgrader presetUpgrader( initPreset, glintManager.getPresetHeader() );
    presetManager.upgradePresets( &presetUpgrader );

    sAudioBuffer.registerCallback( &glintManager );
}

GlintVSTAudioProcessor::~GlintVSTAudioProcessor()
{
}

//==============================================================================
const juce::String GlintVSTAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GlintVSTAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool GlintVSTAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool GlintVSTAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double GlintVSTAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GlintVSTAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int GlintVSTAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GlintVSTAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String GlintVSTAudioProcessor::getProgramName (int index)
{
    return {};
}

void GlintVSTAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void GlintVSTAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    sampleRateConverter.setSourceRate( static_cast<unsigned int>(sampleRate) );
    sampleRateConverter.setSourceBufferSize( samplesPerBlock );
    sampleRateConverter.resetAAFilters();
}

void GlintVSTAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GlintVSTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void GlintVSTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const float* inBufferL = buffer.getReadPointer( 0, 0 );
    float* outBufferL = buffer.getWritePointer( 0, 0 );

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
    for ( unsigned int sample = 0; sample < actualTargetBufferSize; sample++ )
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

    // fill the rest of the channels with the mono audio from channel 0
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        for ( auto sample = 0; sample < buffer.getNumSamples(); sample++ )
        {
            channelData[sample] = outBufferL[sample];
        }
    }

    this->dispatchEventsForIds( processorId, processorEditorId );
}

//==============================================================================
bool GlintVSTAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GlintVSTAudioProcessor::createEditor()
{
    GlintVSTAudioProcessorEditor* editor = new GlintVSTAudioProcessorEditor( *this );
    processorEditorId = editor->getProcessorEditorId();
    this->dispatchEventsForIds( processorId, processorEditorId );

    return editor;
}

//==============================================================================
void GlintVSTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GlintVSTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

            // set initial values
            juce::AudioParameterFloat* e1 = dynamic_cast<juce::AudioParameterFloat*>( apvts.getParameter("effect1") );
            juce::AudioParameterInt* e2 = dynamic_cast<juce::AudioParameterInt*>( apvts.getParameter("effect2") );
            juce::AudioParameterInt* e3 = dynamic_cast<juce::AudioParameterInt*>( apvts.getParameter("effect3") );
            juce::Range<float> e1Range = e1->getNormalisableRange().getRange();
            juce::Range<int> e2Range = e2->getRange();
            juce::Range<int> e3Range = e3->getRange();
            float effect1SldrPercentage = ( e1->get() - e1Range.getStart()) / (e1Range.getEnd() - e1Range.getStart() );
            float effect2SldrPercentage = ( e2->get() - e2Range.getStart()) / (e2Range.getEnd() - e2Range.getStart() );
            float effect3SldrPercentage = ( e3->get() - e3Range.getStart()) / (e3Range.getEnd() - e3Range.getStart() );
            IPotEventListener::PublishEvent( PotEvent(effect1SldrPercentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT1), true) );
            IPotEventListener::PublishEvent( PotEvent(effect2SldrPercentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT2), true) );
            IPotEventListener::PublishEvent( PotEvent(effect3SldrPercentage, static_cast<unsigned int>(POT_CHANNEL::EFFECT3), true) );

            this->dispatchEventsForIds( processorId, processorEditorId );
        }
    }
}

void GlintVSTAudioProcessor::dispatchEventsForIds (const unsigned int processorId, const unsigned int processorEditorId)
{
    // The sequencing of these calls is extremely important and it's possible for other projects that the juceDispatchQueuedEvents function
    // may need to be called more than once if the event handling of a different event listener publishes new events to an event listener that
    // has already called it's juceDispatchQueuedEvents function. For example with this project IPotEventListener and IButtonEventListener handling
    // publishes IGlintParameterEventListener and IGlintParameterEventListener events, so they must be called first. Likewise, the handling of
    // IGlintParameterEventListener and IGlintLCDRefreshEventListener events publishes IGlintLCDRefreshEventListener events, so those must
    // be called before IGlintLCDRefreshEventListener. The onus is on the user to sequence these correctly in the most performant way possible.
    EventDispatcher<IPotEventListener, PotEvent, &IPotEventListener::onPotEvent>::juceDispatchQueuedEvents( processorId, processorEditorId );
    EventDispatcher<IButtonEventListener, ButtonEvent, &IButtonEventListener::onButtonEvent>::juceDispatchQueuedEvents( processorId, processorEditorId );
    EventDispatcher<IGlintParameterEventListener, GlintParameterEvent,
                    &IGlintParameterEventListener::onGlintParameterEvent>::juceDispatchQueuedEvents( processorId, processorEditorId );
    EventDispatcher<IGlintPresetEventListener, GlintPresetEvent,
                    &IGlintPresetEventListener::onGlintPresetChangedEvent>::juceDispatchQueuedEvents( processorId, processorEditorId );
    EventDispatcher<IGlintLCDRefreshEventListener, GlintLCDRefreshEvent,
                    &IGlintLCDRefreshEventListener::onGlintLCDRefreshEvent>::juceDispatchQueuedEvents( processorId, processorEditorId );
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GlintVSTAudioProcessor();
}
