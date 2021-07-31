#include "GlintManager.hpp"

#include "GlintConstants.hpp"
#include "SRAM_23K256.hpp"

#include <string.h>
#include <cmath>

unsigned int GlintSimpleDelay::m_RunningDelayLineOffset = 0;

GlintManager::GlintManager (IStorageMedia* delayBufferStorage) :
	m_StorageMedia( delayBufferStorage ),
	m_StorageMediaSize( (Sram_23K256::SRAM_SIZE * 4) / sizeof(uint16_t) ), // size of 4 srams installed on Gen_FX_SYN rev 2
	m_DecayTime( 0.0f ),
	m_ModRate( 0.0f ),
	m_FiltFreq( 20000.0f ),
	m_Diffusion( 0.913f ),
	m_DiffusionAPF1( GLINT_DIFFUSE_LEN_1, m_Diffusion, 0 ),
	m_DiffusionAPF2( GLINT_DIFFUSE_LEN_2, m_Diffusion, 0 ),
	m_DiffusionAPF3( GLINT_DIFFUSE_LEN_3, m_Diffusion, 0 ),
	m_DiffusionAPF4( GLINT_DIFFUSE_LEN_4, m_Diffusion, 0 ),
	m_LowpassFilter1(),
	m_LowpassFilter2(),
	m_ReverbNetModOsc(),
	m_ReverbNet1APF1( GLINT_REVERBNET1_APF_LEN_1, m_DecayTime, 0 ),
	m_ReverbNet1SD1( delayBufferStorage, GLINT_REVERBNET1_SD_LEN_1, GLINT_REVERBNET1_SD_LEN_1, 0 ),
	m_ReverbNet1APF2( GLINT_REVERBNET1_APF_LEN_2, m_DecayTime, 0 ),
	m_ReverbNet1SD2( delayBufferStorage, GLINT_REVERBNET1_SD_LEN_2, GLINT_REVERBNET1_SD_LEN_2, 0 ),
	m_ReverbNet1ModD( GLINT_REVERBNET1_MODD_LEN, GLINT_REVERBNET1_MODD_LEN, 0 ),
	m_ReverbNet2APF1( GLINT_REVERBNET2_APF_LEN_1, m_DecayTime, 0 ),
	m_ReverbNet2SD1( delayBufferStorage, GLINT_REVERBNET2_SD_LEN_1, GLINT_REVERBNET2_SD_LEN_1, 0 ),
	m_ReverbNet2APF2( GLINT_REVERBNET2_APF_LEN_2, m_DecayTime, 0 ),
	m_ReverbNet2SD2( delayBufferStorage, GLINT_REVERBNET2_SD_LEN_2, GLINT_REVERBNET2_SD_LEN_2, 0 ),
	m_ReverbNet2ModD( GLINT_REVERBNET2_MODD_LEN, GLINT_REVERBNET2_MODD_LEN, 0 ),
	m_PrevReverbNet1Val( 0 ),
	m_PrevReverbNet2Val( 0 )
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

void GlintManager::setModRate (float modRate)
{
	m_ModRate = modRate;
}

void GlintManager::setFiltFreq (float filtFreq)
{
	m_FiltFreq = filtFreq;
}

void GlintManager::call (uint16_t* writeBuffer)
{
	m_LowpassFilter1.setCoefficients( m_FiltFreq );
	m_LowpassFilter2.setCoefficients( m_FiltFreq );

	m_ReverbNet1APF1.setFeedbackGain( m_DecayTime );
	m_ReverbNet1APF2.setFeedbackGain( m_DecayTime );
	m_ReverbNet2APF1.setFeedbackGain( m_DecayTime );
	m_ReverbNet2APF2.setFeedbackGain( m_DecayTime );

	// get the sine wave values for modulation
	// m_ReverbNetModOsc.setFrequency( m_ModRate );
	// float modVals[ABUFFER_SIZE];
	// m_ReverbNetModOsc.call( modVals );

	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		int16_t sampleVal = static_cast<int16_t>( writeBuffer[sample] ) - 2048;

		/*
		// diffusion stage
		sampleVal = m_DiffusionAPF1.processSample( sampleVal );
		sampleVal = m_DiffusionAPF2.processSample( sampleVal );
		sampleVal = m_DiffusionAPF3.processSample( sampleVal );
		sampleVal = m_DiffusionAPF4.processSample( sampleVal );

		// feedback and cross stage
		float sampleValFL = ( static_cast<float>(sampleVal) + m_PrevReverbNet2Val ) * 0.5f;
		float sampleValFR = ( static_cast<float>(sampleVal) + m_PrevReverbNet1Val ) * 0.5f;

		// lowpass stage
		sampleValFL = m_LowpassFilter1.processSample( sampleValFL );
		sampleValFR = m_LowpassFilter2.processSample( sampleValFR );

		// reverberation network stage
		// m_ReverbNet1ModD.setDelayLength( GLINT_REVERBNET1_MODD_LEN * ((modVals[sample] * 0.5f) + 1.0f) );
		// m_ReverbNet2ModD.setDelayLength( GLINT_REVERBNET2_MODD_LEN * ((modVals[sample] * 0.5f) + 1.0f) );
		int16_t sampleValL = static_cast<int16_t>( std::round(sampleValFL) );
		int16_t sampleValR = static_cast<int16_t>( std::round(sampleValFR) );
		sampleValL = m_ReverbNet1APF1.processSample( sampleValL );
		sampleValL = m_ReverbNet1SD1.processSample( sampleValL );
		// sampleValL = m_ReverbNet1APF2.processSample( sampleValL );
		// sampleValL = m_ReverbNet1SD2.processSample( sampleValL );
		// sampleValL = m_ReverbNet1ModD.processSample( sampleValL );
		m_PrevReverbNet1Val = static_cast<float>( sampleValL ) * m_DecayTime;
		sampleValL = static_cast<int16_t>( m_PrevReverbNet1Val );
		sampleValR = m_ReverbNet2APF1.processSample( sampleValR );
		sampleValR = m_ReverbNet2SD1.processSample( sampleValR );
		// sampleValR = m_ReverbNet2APF2.processSample( sampleValR );
		// sampleValR = m_ReverbNet2SD2.processSample( sampleValR );
		// sampleValR = m_ReverbNet2ModD.processSample( sampleValR );
		m_PrevReverbNet2Val = static_cast<float>( sampleValR ) * m_DecayTime;
		sampleValR = static_cast<int16_t>( m_PrevReverbNet2Val );

		// summing stage
		sampleVal = static_cast<int16_t>( (m_PrevReverbNet1Val + m_PrevReverbNet2Val) * 0.5f );
		*/

		sampleVal = m_ReverbNet1SD1.processSample( sampleVal );

		writeBuffer[sample] = static_cast<uint16_t>( sampleVal + 2048 ); // +2048 to turn it back into a uint16_t
	}
}

void GlintManager::onGlintParameterEvent (const GlintParameterEvent& paramEvent)
{
	unsigned int channel = paramEvent.getChannel();
	POT_CHANNEL channelEnum = static_cast<POT_CHANNEL>( channel );
	float valueToSet = paramEvent.getValue();

	if ( channelEnum == POT_CHANNEL::DECAY_TIME )
	{
		this->setDecayTime( valueToSet );
	}
	else if ( channelEnum == POT_CHANNEL::MOD_RATE )
	{
		this->setModRate( valueToSet );
	}
	else if ( channelEnum == POT_CHANNEL::FILT_FREQ )
	{
		this->setFiltFreq( valueToSet );
	}
}
