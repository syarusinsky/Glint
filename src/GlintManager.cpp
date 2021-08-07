#include "GlintManager.hpp"

#include "GlintConstants.hpp"
#include "SRAM_23K256.hpp"

#include <string.h>
#include <cmath>

unsigned int GlintSimpleDelay::m_RunningDelayLineOffset = 0;

GlintManager::GlintManager (STORAGE* delayBufferStorage) :
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
	m_PrevReverbNet1Vals{ 0 },
	m_PrevReverbNet2Vals{ 0 }
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

	// offset samples to maximize headroom
	const unsigned int scaleFactor = 1;
	int16_t* sampleVals = reinterpret_cast<int16_t*>( writeBuffer );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		sampleVals[sample] -= 2048;
		sampleVals[sample] *= scaleFactor;
	}

	// diffusion stage
	m_DiffusionAPF1.call( sampleVals );
	m_DiffusionAPF2.call( sampleVals );
	m_DiffusionAPF3.call( sampleVals );
	m_DiffusionAPF4.call( sampleVals );

	// feedback and cross stage
	int16_t sampleValsL[ABUFFER_SIZE] = { 0 };
	int16_t sampleValsR[ABUFFER_SIZE] = { 0 };
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		sampleValsL[sample] = ( sampleVals[sample] + m_PrevReverbNet2Vals[sample] ) / 2;
	}
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		sampleValsR[sample] = ( sampleVals[sample] + m_PrevReverbNet1Vals[sample] ) / 2;
	}

	// lowpass stage
	m_LowpassFilter1.call( sampleValsL );
	m_LowpassFilter2.call( sampleValsR );

	// reverberation network stage
	m_ReverbNet1APF1.call( sampleValsL );
	m_ReverbNet1SD1.call(  sampleValsL );
	m_ReverbNet1APF2.call( sampleValsL );
	m_ReverbNet1SD2.call(  sampleValsL );
	m_ReverbNet2APF1.call( sampleValsR );
	m_ReverbNet2SD1.call(  sampleValsR );
	m_ReverbNet2APF2.call( sampleValsR );
	m_ReverbNet2SD2.call(  sampleValsR );
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		m_PrevReverbNet1Vals[sample] = sampleValsL[sample] * m_DecayTime;
		m_PrevReverbNet2Vals[sample] = sampleValsR[sample] * m_DecayTime;
		sampleValsL[sample] = m_PrevReverbNet1Vals[sample];
		sampleValsR[sample] = m_PrevReverbNet2Vals[sample];

		// summing
		sampleVals[sample] = ( sampleValsL[sample] + sampleValsR[sample] ) / 2;
	}

	// offset samples to fit into dac range
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		sampleVals[sample] *= (1.0f / scaleFactor); // TODO ensure this is evaluated as a constant
		sampleVals[sample] += 2048;
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
