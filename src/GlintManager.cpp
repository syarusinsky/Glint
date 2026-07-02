#include "GlintManager.hpp"

#include "GlintConstants.hpp"
#include "SRAM_23K256.hpp"
#include "PresetManager.hpp"
#include "MidiHandler.hpp"
#include "IGlintPresetEventListener.hpp"

#include <string.h>
#include <cmath>
#include <algorithm>
#include <random>

unsigned int GlintStorageAllpassCombFilter::m_RunningDelayLineOffset = 0;

GlintManager::GlintManager (STORAGE* delayBufferStorage, MidiHandler* midiHandler, PresetManager* presetManager) :
	m_NoiseGate( 0.02f, 100.0f, 100 ),
	m_StorageMedia( delayBufferStorage ),
	m_StorageMediaSize( (Sram_23K256::SRAM_SIZE * 4) / sizeof(uint16_t) ), // size of 4 srams installed on Gen_FX_SYN rev 2
	m_MidiHandler( midiHandler ),
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
	m_PrevReverbNetBlock2Vals{ 0 },
	m_PresetToSendOrReceive( this->getState() ),
	m_PresetToSendOrReceiveNum( 0 ),
	m_DevId( 0 ),
	m_SenderId( 0 )
{
	this->bindToGlintParameterEventSystem();
	this->bindToSalSysexEventSystem();

	m_ReverbNetModOsc.setOscillatorMode( OscillatorMode::SINE );
}

GlintManager::~GlintManager()
{
	this->unbindFromGlintParameterEventSystem();
	this->unbindFromSalSysexEventSystem();
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

		// TODO might need to be modified if it's producing dc offset??? Check with vst plugin before pushing
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
	else if ( channelEnum == PARAM_CHANNEL::SEND_PRESET && m_DevId == 0 && m_SenderId == 0 ) // ensure no preset exchange is taking place
	{
		// send this preset
		m_SendingOrReceivingAllPresets = false;
		m_NibbleIndex = 0;
		m_DevId = this->generateRandomDevId(); // use a random id for the sender
		m_SenderId = 0;
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent sendPresetEvent
			= SalSysexEvent::buildRequestToSendPresetEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, m_PresetManager->getCurrentPresetNum(), numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( sendPresetEvent );
	}
	else if ( channelEnum == PARAM_CHANNEL::SEND_ALL_PRESETS && m_DevId == 0 && m_SenderId == 0 ) // ensure no preset exchange is taking place
	{
		// send all presets
		m_SendingOrReceivingAllPresets = true;
		m_NibbleIndex = 0;
		m_DevId = this->generateRandomDevId(); // use a random id for the sender
		m_SenderId = 0;
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent sendAllPresetsEvent
			= SalSysexEvent::buildRequestToSendAllPresetsEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, 0, numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( sendAllPresetsEvent );
	}
	else if ( channelEnum == PARAM_CHANNEL::ACCEPT_PRESET )
	{
		// send accepted message
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent acceptPresetOrPresetsEvent
			= SalSysexEvent::buildAcceptPresetOrPresetsEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, m_RequestedPresetNum, numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( acceptPresetOrPresetsEvent );

		// go to receiving page
		IGlintPresetEventListener::PublishEvent(
					GlintPresetEvent(this->getState(), m_RequestedPresetNum, 0, GlintPresetEventTypeEnum::ACCEPT_PRESET) );
	}
	else if ( channelEnum == PARAM_CHANNEL::DENY_PRESET )
	{
		// send denied message
		const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
		SalSysexEvent denyPresetOrPresetsEvent
			= SalSysexEvent::buildDenyPresetOrPresetsEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, m_RequestedPresetNum, numNibblesInPreset );
		m_MidiHandler->processSalSysexEvent( denyPresetOrPresetsEvent );

		// restore dev id and return to main menu
		m_DevId = 0;
		m_SenderId = 0;
		IGlintPresetEventListener::PublishEvent(
					GlintPresetEvent(this->getState(), m_PresetManager->getCurrentPresetNum(), 0, GlintPresetEventTypeEnum::DENY_PRESET) );
	}
}

void GlintManager::onSalSysexEvent (const SalSysexEvent& salSysexEvent)
{
	if ( m_DevId == 0 && m_SenderId == 0 ) // if no preset exchange is currently taking place
	{
		if ( salSysexEvent.getType() == SalSysexTypeEnum::REQUEST_TO_SEND_PRESET )
		{
			// send message to ui to give option to accept or deny
			m_SenderId = salSysexEvent.getDevId();
			m_DevId = ( m_SenderId + 1 ) % 0x7F;
			m_RequestedPresetNum = salSysexEvent.getPresetNum();
			m_SendingOrReceivingAllPresets = false;
			m_NibbleIndex = 0;
			IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, GlintPresetEventTypeEnum::SEND_PRESET_REQUEST) );
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::REQUEST_TO_SEND_ALL_PRESETS )
		{
			// send message to ui to give option to accept or deny
			m_SenderId = salSysexEvent.getDevId();
			m_DevId = ( m_SenderId + 1 ) % 0x7F;
			m_RequestedPresetNum = salSysexEvent.getPresetNum();
			m_SendingOrReceivingAllPresets = true;
			m_NibbleIndex = 0;
			IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, GlintPresetEventTypeEnum::SEND_ALL_PRESETS_REQUEST) );
		}
	}
	else if ( m_DevId == salSysexEvent.getRecId() && (m_SenderId == 0 || m_SenderId == salSysexEvent.getDevId()) ) // if preset exchange is in progress and ids match
	{
		if ( salSysexEvent.getType() == SalSysexTypeEnum::ACCEPT_PRESET_OR_PRESETS )
		{
			// send the requested preset
			// note that since sal has a limited midi message size, multiple preset chunks are usually necessary for a single preset
			if ( m_SenderId == 0 )
			{
				m_SenderId = salSysexEvent.getDevId();
			}
			const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
			const uint8_t requestedPresetNum = salSysexEvent.getPresetNum();
			if ( m_SendingOrReceivingAllPresets )
			{
				m_PresetToSendOrReceive = m_PresetManager->retrievePreset<GlintState>( requestedPresetNum );
			}
			else
			{
				m_PresetToSendOrReceive = this->getState();
			}
			SalSysexEvent sendPresetDataChunkEvent
				= SalSysexEvent::buildSendPresetDataChunkEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

			// build the chunk
			while ( m_NibbleIndex < numNibblesInPreset )
			{
				uint8_t nibble = reinterpret_cast<uint8_t*>( &m_PresetToSendOrReceive )[ m_NibbleIndex / 2 ];
				if ( (m_NibbleIndex & 0b1) == 0 )
				{
					// this is the high nibble of the byte
					nibble = nibble >> 4;
				}
				else
				{
					// this is the low nibble of the byte
					nibble = nibble & 0b1111;
				}

				if ( ! sendPresetDataChunkEvent.writeNibble(nibble) )
				{
					// unsuccessful write due to midi message being full
					break;
				}
				else
				{
					// successful write
					m_NibbleIndex++;
				}
			}

			m_MidiHandler->processSalSysexEvent( sendPresetDataChunkEvent );
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::DENY_PRESET_OR_PRESETS )
		{
			// reset and return to main page
			m_DevId = 0;
			m_SenderId = 0;
			m_SendingOrReceivingAllPresets = false;
			m_NibbleIndex = 0;
			IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, GlintPresetEventTypeEnum::DENY_PRESET) );
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::SEND_PRESET_DATA_CHUNK )
		{
			const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
			const uint8_t requestedPresetNum = salSysexEvent.getPresetNum();
			const uint8_t* presetChunkNibbles = salSysexEvent.getPresetChunkNibbles();
			uint8_t presetChunkNibblesIndex = 0;
			uint8_t maxNibblesInMessage = salSysexEvent.getMaxNumNibblesInPresetChunkNibbles();
			uint8_t* presetToSendOrReceivePtr = reinterpret_cast<uint8_t*>( &m_PresetToSendOrReceive );

			// build the preset from the chunk
			while ( m_NibbleIndex < numNibblesInPreset && presetChunkNibblesIndex < maxNibblesInMessage )
			{
				const uint8_t nibble = presetChunkNibbles[presetChunkNibblesIndex];
				const unsigned int byteIndex = m_NibbleIndex / 2;

				if ( (m_NibbleIndex & 0b1) == 0 )
				{
					// this is the high nibble of the byte
					presetToSendOrReceivePtr[byteIndex] = ( nibble << 4 );
				}
				else
				{
					// this is the low nibble of the byte
					presetToSendOrReceivePtr[byteIndex] |= nibble;
				}

				m_NibbleIndex++;
				presetChunkNibblesIndex++;
			}

			if ( m_NibbleIndex == numNibblesInPreset )
			{
				// we have the full preset, send the received preset message
				m_NibbleIndex = 0; // reset since next preset we need to start at the first nibble
				SalSysexEvent receivedPresetEvent
					= SalSysexEvent::buildReceivedPresetEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

				// save the preset
				if ( m_SendingOrReceivingAllPresets && requestedPresetNum != m_PresetManager->getMaxNumPresets() - 1)
				{
					m_PresetManager->writePreset<GlintState>( m_PresetToSendOrReceive, requestedPresetNum );
				}
				else // receiving only one preset, or finished receiving all presets
				{
					const uint8_t presetNumToSaveTo = ( m_SendingOrReceivingAllPresets ) ? requestedPresetNum : m_PresetManager->getCurrentPresetNum();
					m_PresetManager->writePreset<GlintState>( m_PresetToSendOrReceive, presetNumToSaveTo );
					this->setState( m_PresetToSendOrReceive );

					// return to main menu
					IGlintPresetEventListener::PublishEvent(
						GlintPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, GlintPresetEventTypeEnum::FINISHED_SENDING_OR_RECEIVING_PRESETS) );

					m_DevId = 0;
					m_SenderId = 0;
				}

				m_MidiHandler->processSalSysexEvent( receivedPresetEvent );
			}
			else // we don't have the full preset yet, request another chunk
			{
				SalSysexEvent acceptPresetOrPresetsEvent
					= SalSysexEvent::buildAcceptPresetOrPresetsEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

				m_MidiHandler->processSalSysexEvent( acceptPresetOrPresetsEvent );
			}
		}
		else if ( salSysexEvent.getType() == SalSysexTypeEnum::RECEIVED_PRESET )
		{
			// send the next requested preset
			// note that since sal has a limited midi message size, multiple preset chunks are usually necessary for a single preset
			const uint16_t numNibblesInPreset = this->getNumNibblesInPreset();
			const uint8_t requestedPresetNum = ( m_SendingOrReceivingAllPresets ) ? salSysexEvent.getPresetNum() + 1 : m_PresetManager->getMaxNumPresets();

			m_NibbleIndex = 0; // reset since next preset we need to start at the first nibble

			if ( requestedPresetNum == m_PresetManager->getMaxNumPresets() )
			{
				m_DevId = 0;
				m_SenderId = 0;
				m_SendingOrReceivingAllPresets = false;

				// return to main menu
				IGlintPresetEventListener::PublishEvent(
					GlintPresetEvent(this->getState(), salSysexEvent.getPresetNum(), 0, GlintPresetEventTypeEnum::FINISHED_SENDING_OR_RECEIVING_PRESETS) );
			}
			else
			{
				if ( m_SendingOrReceivingAllPresets )
				{
					m_PresetToSendOrReceive = m_PresetManager->retrievePreset<GlintState>( requestedPresetNum );
				}
				else
				{
					m_PresetToSendOrReceive = this->getState();
				}
				SalSysexEvent sendPresetDataChunkEvent
					= SalSysexEvent::buildSendPresetDataChunkEvent( m_DevId, GLINT_MODEL_ID, m_SenderId, requestedPresetNum, numNibblesInPreset );

				// build the chunk
				while ( m_NibbleIndex < numNibblesInPreset )
				{
					uint8_t nibble = reinterpret_cast<uint8_t*>( &m_PresetToSendOrReceive )[ m_NibbleIndex / 2 ];
					if ( (m_NibbleIndex & 0b1) == 0 )
					{
						// this is the high nibble of the byte
						nibble = nibble >> 4;
					}
					else
					{
						// this is the low nibble of the byte
						nibble = nibble & 0b1111;
					}

					if ( ! sendPresetDataChunkEvent.writeNibble(nibble) )
					{
						// unsuccessful write due to midi message being full
						break;
					}
					else
					{
						// successful write
						m_NibbleIndex++;
					}
				}

				m_MidiHandler->processSalSysexEvent( sendPresetDataChunkEvent );
			}
		}
	}
}

uint8_t GlintManager::generateRandomDevId()
{
	// generate a random device id
	std::random_device rd;
	std::mt19937 gen( rd() );
	std::uniform_int_distribution<int> distrib( 0x01, 0x7E ); // 0x7F since it must be a data byte instead of a status byte, ranges so that dev id and sender id are never zero

	return distrib( gen );
}

uint16_t GlintManager::getNumNibblesInPreset()
{
	return sizeof( GlintState ) * 2; // * 2 since we're handling nibbles not bytes
}
