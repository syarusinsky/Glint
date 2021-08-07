#ifndef GLINTMANAGER_HPP
#define GLINTMANAGER_HPP

#include "IBufferCallback.hpp"
#include "IStorageMedia.hpp"
#include "AudioConstants.hpp"
#include "IGlintParameterEventListener.hpp"
#include "AllpassCombFilter.hpp"
#include "OnePoleFilter.hpp"
#include "SimpleDelay.hpp"
#include "PolyBLEPOsc.hpp"

#include <stdint.h>

#ifdef TARGET_BUILD
	#define STORAGE Sram_23K256_Manager
	#include "SRAM_23K256.hpp"
#else
	#define STORAGE FakeStorageDevice
	#include "FakeStorageDevice.hpp"
#endif

// this simple delay uses IStorageMedia as opposed to an array
class GlintSimpleDelay : public IBufferCallback<int16_t>
{
	public:
		GlintSimpleDelay (STORAGE* delayBufferStorage, unsigned int maxDelayLength, unsigned int delayLength, int16_t initVal) :
			m_DelayLength( maxDelayLength + 1 ), // plus one because one sample is no delay
			m_DelayBuffer( delayBufferStorage ),
			m_DelayWriteIncr( 0 ),
			m_DelayReadIncr( 0 ),
			m_DelayLineOffset( m_RunningDelayLineOffset )
		{
			this->setDelayLength( delayLength );

			SharedData<uint8_t> data = SharedData<uint8_t>::MakeSharedData( m_DelayLength * sizeof(int16_t) );
			int16_t* dataPtr = reinterpret_cast<int16_t*>( data.getPtr() );
			for ( unsigned int sample = 0; sample < m_DelayLength; sample++ )
			{
				dataPtr[sample] = 0;
			}

			m_DelayBuffer->writeToMedia( data, m_DelayLineOffset );

			m_RunningDelayLineOffset += m_DelayLength;
		}
		~GlintSimpleDelay() {}

		int16_t processSample (int16_t sampleVal)
		{
			// read delayed value
			unsigned int readOffset = ( m_DelayLineOffset + m_DelayReadIncr ) * sizeof(int16_t);
			uint8_t readByte1 = m_DelayBuffer->readByte( readOffset );
			uint8_t readByte2 = m_DelayBuffer->readByte( readOffset + 1 );
			int16_t delayedVal = ( readByte1 << 8 ) | ( readByte2 );

			// write current sample value
			unsigned int writeOffset = ( m_DelayLineOffset + m_DelayWriteIncr ) * sizeof(int16_t);
			uint8_t writeByte1 = ( sampleVal >> 8 );
			uint8_t writeByte2 = ( sampleVal & 0b11111111 );
			m_DelayBuffer->writeByte( writeOffset, writeByte1 );
			m_DelayBuffer->writeByte( writeOffset + 1, writeByte2 );

			m_DelayWriteIncr = ( m_DelayWriteIncr + 1 ) % m_DelayLength;
			m_DelayReadIncr = ( m_DelayReadIncr + 1 ) % m_DelayLength;

			return delayedVal;
		}

		void setDelayLength (unsigned int delayLength)
		{
			m_DelayReadIncr = ( m_DelayWriteIncr + (m_DelayLength - delayLength) ) % m_DelayLength;
		}

		void call (int16_t* writeBuffer) override
		{
			SharedData<uint8_t> readData = SharedData<uint8_t>::MakeSharedData( ABUFFER_SIZE * sizeof(int16_t) );
			unsigned int endIndex = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;
			unsigned int startIndex = m_DelayReadIncr;
			if ( endIndex < startIndex )
			{
				// we need to wrap around the delay buffer
				unsigned int firstHalfSize = m_DelayLength - startIndex;
				unsigned int secondHalfSize = endIndex;
				SharedData<uint8_t> firstHalf =
					m_DelayBuffer->readFromMedia( firstHalfSize * sizeof(int16_t),
									(m_DelayLineOffset + startIndex) * sizeof(int16_t) );
				SharedData<uint8_t> secondHalf =
					m_DelayBuffer->readFromMedia( secondHalfSize * sizeof(int16_t),
									(m_DelayLineOffset) * sizeof(int16_t) );

				// fill readData buffer with read values
				for ( unsigned int byte = 0; byte < firstHalfSize * sizeof(int16_t); byte++ )
				{
					readData[byte] = firstHalf[byte];
				}
				unsigned int secondHalfOffset = firstHalfSize * sizeof(int16_t);
				for ( unsigned int byte = secondHalfOffset; byte < ABUFFER_SIZE * sizeof(int16_t); byte++ )
				{
					readData[byte] = secondHalf[byte - secondHalfOffset];
				}
			}
			else // endIndex > startIndex
			{
				// we can just read ABUFFER_SIZE contiguous samples
				readData = m_DelayBuffer->readFromMedia( ABUFFER_SIZE * sizeof(int16_t),
									(m_DelayLineOffset + startIndex) * sizeof(int16_t) );
			}

			// write read data into writeBuffer
			int16_t* readDataPtr = reinterpret_cast<int16_t*>( readData.getPtr() );
			for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
			{
				int16_t delayedVal = readDataPtr[sample];

				// use the same buffer (readData) to write to
				readDataPtr[sample] = writeBuffer[sample];

				// write the delayed value to the actual write buffer
				writeBuffer[sample] = delayedVal;
			}

			unsigned int endWriteIndex = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
			unsigned int startWriteIndex = m_DelayWriteIncr;
			if ( endWriteIndex < startWriteIndex )
			{
				// we need to wrap around the delay buffer
				unsigned int firstHalfSize = m_DelayLength - startWriteIndex;
				SharedData<uint8_t> firstHalf =
					SharedData<uint8_t>::MakeSharedDataFromRange( readData, 0, (firstHalfSize * sizeof(int16_t)) - 1 );
				SharedData<uint8_t> secondHalf =
					SharedData<uint8_t>::MakeSharedDataFromRange( readData, firstHalfSize * sizeof(int16_t),
											(ABUFFER_SIZE * sizeof(int16_t)) - 1 );

				// write both halves to buffer
				m_DelayBuffer->writeToMedia( firstHalf, (m_DelayLineOffset + startWriteIndex) * sizeof(int16_t) );
				m_DelayBuffer->writeToMedia( secondHalf, (m_DelayLineOffset) * sizeof(int16_t) );
			}
			else // endWriteIndex > startWriteIndex
			{
				// we can just write contiguous samples
				m_DelayBuffer->writeToMedia( readData, (m_DelayLineOffset + startWriteIndex) * sizeof(int16_t) );
			}

			m_DelayWriteIncr = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
			m_DelayReadIncr = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;
		}

	private:
		unsigned int 	m_DelayLength;
		STORAGE* 	m_DelayBuffer;
		unsigned int 	m_DelayWriteIncr;
		unsigned int 	m_DelayReadIncr;
		unsigned int 	m_DelayLineOffset; // since we're using one contiguous storage device, offset each delay
		static unsigned int m_RunningDelayLineOffset; // use this value to determine where to put new delay lines in memory
};

class GlintManager : public IBufferCallback<uint16_t>, public IGlintParameterEventListener
{
	public:
		GlintManager (STORAGE* delayBufferStorage);
		~GlintManager() override;

		void setDecayTime (float decayTime); // decayTime should be between 0.0f and 1.0f
		void setModRate (float modRate); // modRate should be in hertz
		void setFiltFreq (float filtFreq); // filtFreq should be in hertz

		void call (uint16_t* writeBuffer) override;

		void onGlintParameterEvent (const GlintParameterEvent& paramEvent) override;

	private:
		STORAGE* 			m_StorageMedia; // where the static delay buffers sit
		unsigned int 			m_StorageMediaSize;

		float 				m_DecayTime;
		float 				m_ModRate;
		float 				m_FiltFreq;

		// diffusion network filters
		float 				m_Diffusion; // should be between 0.0f and 1.0f
		AllpassCombFilter<int16_t> 	m_DiffusionAPF1;
		AllpassCombFilter<int16_t> 	m_DiffusionAPF2;
		AllpassCombFilter<int16_t> 	m_DiffusionAPF3;
		AllpassCombFilter<int16_t> 	m_DiffusionAPF4;

		// low-pass filters
		OnePoleFilter<int16_t> 		m_LowpassFilter1;
		OnePoleFilter<int16_t> 		m_LowpassFilter2;

		// reverberation network filters
		PolyBLEPOsc 			m_ReverbNetModOsc;
		AllpassCombFilter<int16_t> 	m_ReverbNet1APF1;
		GlintSimpleDelay 		m_ReverbNet1SD1;
		AllpassCombFilter<int16_t> 	m_ReverbNet1APF2;
		GlintSimpleDelay 		m_ReverbNet1SD2;
		SimpleDelay<int16_t> 		m_ReverbNet1ModD;
		AllpassCombFilter<int16_t> 	m_ReverbNet2APF1;
		GlintSimpleDelay 		m_ReverbNet2SD1;
		AllpassCombFilter<int16_t> 	m_ReverbNet2APF2;
		GlintSimpleDelay 		m_ReverbNet2SD2;
		SimpleDelay<int16_t> 		m_ReverbNet2ModD;

		int16_t 			m_PrevReverbNet1Vals[ABUFFER_SIZE]; // for feedback
		int16_t 			m_PrevReverbNet2Vals[ABUFFER_SIZE];

};

#endif // GLINTMANAGER_HPP
