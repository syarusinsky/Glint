#include "GlintManager.hpp"

#include "GlintConstants.hpp"
#include "SRAM_23K256.hpp"
#include "PresetManager.hpp"
#include "IGlintPresetEventListener.hpp"

#include <string.h>
#include <cmath>

unsigned int GlintStorageAllpassCombFilter::m_RunningDelayLineOffset = 0;

GlintManager::GlintManager (STORAGE* delayBufferStorage, PresetManager* presetManager) :
	m_NoiseGate( 0.02f, 100.0f, 100 ),
	m_StorageMedia( delayBufferStorage ),
	m_StorageMediaSize( (Sram_23K256::SRAM_SIZE * 4) / sizeof(uint16_t) ), // size of 4 srams installed on Gen_FX_SYN rev 2
	m_PresetManager( presetManager ),
	m_PresetHeader( {1, 0, 0, true} ),
	m_DecayTime( 0.0f ),
	m_FiltFreq( 20000.0f ),
	m_Diffusion( 0.913f ),
	m_DiffusionAPF1( GLINT_DIFFUSE_LEN_1, m_Diffusion, 0 ),
	m_DiffusionAPF2( GLINT_DIFFUSE_LEN_2, m_Diffusion, 0 ),
	m_DiffusionAPF3( GLINT_DIFFUSE_LEN_3, m_Diffusion, 0 ),
	m_DiffusionAPF4( GLINT_DIFFUSE_LEN_4, m_Diffusion, 0 ),
	m_LowpassFilter(),
	m_ReverbNetModOsc(),
	m_ReverbNetBlock1APF1( GLINT_REVERBNET1_APF_LEN_1, m_DecayTime, 0 ),
	m_ReverbNetBlock1APF2( GLINT_REVERBNET1_APF_LEN_2, m_DecayTime, 0 ),
	m_ReverbNetBlock1APF3( GLINT_REVERBNET1_APF_LEN_3, m_DecayTime, 0 ),
	m_ReverbNetBlock1APF4( GLINT_REVERBNET1_APF_LEN_4, m_DecayTime, 0 ),
	m_ReverbNetBlock2APF1( GLINT_REVERBNET2_APF_LEN_1, m_DecayTime, 0 ),
	m_ReverbNetBlock2APF2( GLINT_REVERBNET2_APF_LEN_2, m_DecayTime, 0 ),
	m_ReverbNetBlock2APF3( GLINT_REVERBNET2_APF_LEN_3, m_DecayTime, 0 ),
	m_ReverbNetBlock2APF4( GLINT_REVERBNET2_APF_LEN_4, m_DecayTime, 0 ),
	m_ReverbNetStorageMediaAPF( delayBufferStorage, GLINT_REVERBNET_SMAF_LEN, GLINT_REVERBNET_SMAF_LEN, 0, reinterpret_cast<uint8_t*>(m_ReverbNetStorageMediaAPFBuffer) ),
	m_ReverbNetStorageMediaAPFBuffer{ 0 },
	m_PrevReverbNetVals{ 0 },
	m_PrevReverbNetBlock2Vals{ 0 }
{
	this->bindToGlintParameterEventSystem();

	m_ReverbNetModOsc.setOscillatorMode( OscillatorMode::SINE );
}

GlintManager::~GlintManager()
{
	this->unbindFromGlintParameterEventSystem();
}

void GlintManager::setDecayTime (float decayTime)
{
	m_DecayTime = decayTime;
}

void GlintManager::setDiffusion (float diffusion)
{
	m_Diffusion = diffusion;
}

void GlintManager::setFiltFreq (float filtFreq)
{
	m_FiltFreq = filtFreq;
}

GlintState GlintManager::getState()
{
	GlintState state = { m_DecayTime, m_Diffusion, m_FiltFreq };

	return state;
}

void GlintManager::setState (const GlintState& state)
{
	this->setDecayTime( state.m_DecayTime );
	this->setDiffusion( state.m_Diffusion );
	this->setFiltFreq( state.m_FiltFreq );
}

void GlintManager::loadCurrentPreset()
{
	if ( m_PresetManager )
	{
		GlintState preset = m_PresetManager->retrievePreset<GlintState>( m_PresetManager->getCurrentPresetNum() );
		this->setState( preset );
	}
}

GlintPresetHeader GlintManager::getPresetHeader()
{
	return m_PresetHeader;
}

void GlintManager::call (uint16_t* writeBuffer)
{
	int16_t* writeBufferInt16 = reinterpret_cast<int16_t*>( writeBuffer );

	// first offset for noise gate
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		// for hardware dac
		// writeBufferInt16[sample] -= 2047;

		// for spi dac
		writeBufferInt16[sample] -= 32767;
	}

	m_NoiseGate.call( writeBufferInt16 );

	m_LowpassFilter.setCoefficients( m_FiltFreq );

	m_DiffusionAPF1.setFeedbackGain( m_Diffusion );
	m_DiffusionAPF2.setFeedbackGain( m_Diffusion );
	m_DiffusionAPF3.setFeedbackGain( m_Diffusion );
	m_DiffusionAPF4.setFeedbackGain( m_Diffusion );

	m_ReverbNetBlock1APF1.setFeedbackGain( m_DecayTime );
	m_ReverbNetBlock1APF2.setFeedbackGain( m_DecayTime );
	m_ReverbNetBlock1APF3.setFeedbackGain( m_DecayTime );
	m_ReverbNetBlock1APF4.setFeedbackGain( m_DecayTime );
	m_ReverbNetStorageMediaAPF.setFeedback( m_DecayTime );
	m_ReverbNetBlock2APF1.setFeedbackGain( m_DecayTime );
	m_ReverbNetBlock2APF2.setFeedbackGain( m_DecayTime );
	m_ReverbNetBlock2APF3.setFeedbackGain( m_DecayTime );
	m_ReverbNetBlock2APF4.setFeedbackGain( m_DecayTime );

	// attenuate samples to maximize headroom and feedback from reverb network
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		writeBufferInt16[sample] = writeBufferInt16[sample] + m_PrevReverbNetVals[sample];
	}

	// // lowpass stage
	m_LowpassFilter.call( writeBufferInt16 );

	// diffusion stage
	m_DiffusionAPF1.call( writeBufferInt16 );
	m_DiffusionAPF2.call( writeBufferInt16 );
	m_DiffusionAPF3.call( writeBufferInt16 );
	m_DiffusionAPF4.call( writeBufferInt16 );

	// sample vals will be the actual output, but we also need to calculate the reverb net output values for feedback
	memcpy( m_PrevReverbNetVals, writeBufferInt16, ABUFFER_SIZE * sizeof(int16_t) );

	// feedback from reverb network block 2
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		m_PrevReverbNetVals[sample] = ( m_PrevReverbNetVals[sample] - m_PrevReverbNetBlock2Vals[sample] ) * 0.5f;
	}

	// reverberation network stage
	m_ReverbNetBlock1APF1.call( m_PrevReverbNetVals );
	m_ReverbNetBlock1APF2.call( m_PrevReverbNetVals );
	m_ReverbNetBlock1APF3.call( m_PrevReverbNetVals );
	m_ReverbNetBlock1APF4.call( m_PrevReverbNetVals );
	m_ReverbNetStorageMediaAPF.call( m_PrevReverbNetVals );
	// decay the reverb network samples
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		m_PrevReverbNetVals[sample] *= m_DecayTime;
	}
	// m_PrevReverbNetVals will be the actual output of the reverb network stage, but we also need to calculate the second block for feedback
	memcpy( m_PrevReverbNetBlock2Vals, m_PrevReverbNetVals, ABUFFER_SIZE * sizeof(int16_t) );
	m_ReverbNetBlock2APF1.call( m_PrevReverbNetBlock2Vals );
	m_ReverbNetBlock2APF2.call( m_PrevReverbNetBlock2Vals );
	m_ReverbNetBlock2APF3.call( m_PrevReverbNetBlock2Vals );
	m_ReverbNetBlock2APF4.call( m_PrevReverbNetBlock2Vals );

	// offset samples to fit into dac range
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		// for hardware dac
		// writeBufferInt16[sample] = ( std::abs(writeBufferInt16[sample] + 2047) - std::abs(writeBufferInt16[sample] - 2047) ) / 2;
		// writeBufferInt16[sample] += 2047;

		// for spi dac
		writeBufferInt16[sample] = ( std::abs(writeBufferInt16[sample] + 32767) - std::abs(writeBufferInt16[sample] - 32767) ) / 2;
		writeBufferInt16[sample] += 32767;
	}
}

void GlintManager::onGlintParameterEvent (const GlintParameterEvent& paramEvent)
{
	unsigned int channel = paramEvent.getChannel();
	PARAM_CHANNEL channelEnum = static_cast<PARAM_CHANNEL>( channel );
	float valueToSet = paramEvent.getValue();

	if ( channelEnum == PARAM_CHANNEL::DECAY_TIME )
	{
		this->setDecayTime( valueToSet );
	}
	else if ( channelEnum == PARAM_CHANNEL::DIFFUSION )
	{
		this->setDiffusion( valueToSet );
	}
	else if ( channelEnum == PARAM_CHANNEL::FILT_FREQ )
	{
		this->setFiltFreq( valueToSet );
	}
	else if ( channelEnum == PARAM_CHANNEL::NEXT_PRESET )
	{
		if ( m_PresetManager )
		{
			GlintState preset = m_PresetManager->nextPreset<GlintState>();
			this->setState( preset );
			IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0) );
		}
	}
	else if ( channelEnum == PARAM_CHANNEL::PREV_PRESET )
	{
		if ( m_PresetManager )
		{
			GlintState preset = m_PresetManager->prevPreset<GlintState>();
			this->setState( preset );
			IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0) );
		}
	}
	else if ( channelEnum == PARAM_CHANNEL::WRITE_PRESET )
	{
		if ( m_PresetManager )
		{
			GlintState presetToWrite = this->getState();
			m_PresetManager->writePreset<GlintState>( presetToWrite, m_PresetManager->getCurrentPresetNum() );
			IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0) );
		}
	}
}
