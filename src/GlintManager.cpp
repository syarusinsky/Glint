#include "GlintManager.hpp"

#include "GlintConstants.hpp"
#include "SRAM_23K256.hpp"

#include <string.h>

// TODO remove this after testing
#include <iostream>

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
	m_DiffusionAPF4( GLINT_DIFFUSE_LEN_4, m_Diffusion, 0 )
{
	this->bindToGlintParameterEventSystem();
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
	// m_Filt.setCoefficients( filtFreq );
}

void GlintManager::call (uint16_t* writeBuffer)
{
	for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
	{
		int16_t sampleVal = static_cast<int16_t>( writeBuffer[sample] ) - 2048;
		sampleVal = m_DiffusionAPF1.processSample( sampleVal );
		sampleVal = m_DiffusionAPF2.processSample( sampleVal );
		sampleVal = m_DiffusionAPF3.processSample( sampleVal );
		sampleVal = m_DiffusionAPF4.processSample( sampleVal );

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
