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
#include "NoiseGate.hpp"

#include <stdint.h>

#ifdef TARGET_BUILD
	#define STORAGE Sram_23K256_Manager
	#include "SRAM_23K256.hpp"
#else
	#define STORAGE FakeStorageDevice
	#include "FakeStorageDevice.hpp"
#endif

// this allpass comb filter uses IStorageMedia as opposed to an array
class GlintStorageAllpassCombFilter : public IBufferCallback<int16_t>
{
	public:
		enum class DmaStage
		{
			WRITING_FIRST_HALF,
			WRITING_FINISHING,
			READING_FIRST_HALF,
			READING_FINISHING,
			SEQUENCE_COMPLETE
		};

		GlintStorageAllpassCombFilter (STORAGE* delayBufferStorage, unsigned int maxDelayLength, unsigned int delayLength, int16_t initVal, uint8_t* sharedData) :
			m_DelayLength( maxDelayLength + 1 ), // plus one because one sample is no delay
			m_DelayBuffer( delayBufferStorage ),
			m_DelayWriteIncr( 0 ),
			m_DelayReadIncr( 0 ),
			m_DelayLineOffset( m_RunningDelayLineOffset ),
			m_SharedData( SharedData<uint8_t>::MakeSharedData(ABUFFER_SIZE * sizeof(int16_t), sharedData) ),
			m_Feedback( 0.8f ),
			m_DmaStage( DmaStage::SEQUENCE_COMPLETE )
		{
#ifdef TARGET_BUILD
			delayBufferStorage->setDmaTransferCompleteCallback( std::bind(&GlintStorageAllpassCombFilter::dmaTransferCompleteCallback, this) );
#endif
			this->setDelayLength( delayLength );

			for ( unsigned int sample = 0; sample < m_DelayLength; sample++ )
			{
				unsigned int writeOffset = ( m_DelayLineOffset + sample ) * sizeof(int16_t);
				uint8_t writeByte1 = ( initVal >> 8 );
				uint8_t writeByte2 = ( initVal & 0b11111111 );
				m_DelayBuffer->writeByte( writeOffset, writeByte1 );
				m_DelayBuffer->writeByte( writeOffset + 1, writeByte2 );
			}

			m_RunningDelayLineOffset += m_DelayLength;
		}
		~GlintStorageAllpassCombFilter() {}

		int16_t processSample (int16_t sampleVal)
		{
			// read delayed value
			unsigned int readOffset = ( m_DelayLineOffset + m_DelayReadIncr ) * sizeof(int16_t);
			uint8_t readByte1 = m_DelayBuffer->readByte( readOffset );
			uint8_t readByte2 = m_DelayBuffer->readByte( readOffset + 1 );
			int16_t delayedVal = ( readByte1 << 8 ) | ( readByte2 );

			// write current sample value
#ifdef USE_ALLPASS_FOR_GLINT_SIMPLE_DELAY
			int16_t newSampleVal = ( sampleVal - (delayedVal * m_Feedback) );
			delayedVal = ( (newSampleVal * m_Feedback) + delayedVal );
			int16_t outVal = ( delayedVal * (m_Feedback + 0.087f) ) + ( sampleVal * (1.0f - (m_Feedback + 0.087f)) );
#else // using normal delay
			sampleVal = ( sampleVal + static_cast<int16_t>(delayedVal * m_Feedback) ) / 2;
			int16_t outVal = delayedVal;
#endif
			unsigned int writeOffset = ( m_DelayLineOffset + m_DelayWriteIncr ) * sizeof(int16_t);
			uint8_t writeByte1 = ( sampleVal >> 8 );
			uint8_t writeByte2 = ( sampleVal & 0b11111111 );
			m_DelayBuffer->writeByte( writeOffset, writeByte1 );
			m_DelayBuffer->writeByte( writeOffset + 1, writeByte2 );

			m_DelayWriteIncr = ( m_DelayWriteIncr + 1 ) % m_DelayLength;
			m_DelayReadIncr = ( m_DelayReadIncr + 1 ) % m_DelayLength;

			return outVal;
		}

		void setDelayLength (unsigned int delayLength)
		{
			m_DelayReadIncr = ( m_DelayWriteIncr + (m_DelayLength - delayLength) ) % m_DelayLength;
		}

		void setFeedback (float feedback)
		{
			m_Feedback = std::max<float>( feedback, 0.913f );
		}

		void dmaTransferCompleteCallback() // handles the stages of writing to and reading from the sram through dma
		{
			if ( m_DmaStage == DmaStage::WRITING_FIRST_HALF )
			{
				const unsigned int endWriteIndex = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
				const unsigned int startWriteIndex = m_DelayWriteIncr;
				const unsigned int firstHalfSize = m_DelayLength - startWriteIndex;
				const unsigned int secondHalfSize = ABUFFER_SIZE - firstHalfSize;

				const SharedData<uint8_t> secondHalf
					= SharedData<uint8_t>::MakeSharedData( secondHalfSize * sizeof(int16_t), m_SharedData.getPtr() + (firstHalfSize * sizeof(int16_t)) );

				m_DmaStage = DmaStage::WRITING_FINISHING;
				m_DelayBuffer->writeToMedia( secondHalf, (m_DelayLineOffset) * sizeof(int16_t) );
			}
			else if ( m_DmaStage == DmaStage::WRITING_FINISHING )
			{
				m_DelayWriteIncr = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
				m_DelayReadIncr = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;

				const unsigned int endIndex = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;
				const unsigned int startIndex = m_DelayReadIncr;

				if ( endIndex < startIndex && endIndex != 0 )
				{
					// we need to wrap around the delay buffer
					const unsigned int firstHalfSize = m_DelayLength - startIndex;

					const SharedData<uint8_t> firstHalf
						= SharedData<uint8_t>::MakeSharedData( firstHalfSize * sizeof(int16_t), m_SharedData.getPtr() );

					m_DmaStage = DmaStage::READING_FIRST_HALF;
					m_DelayBuffer->readFromMedia( (m_DelayLineOffset + startIndex) * sizeof(int16_t), firstHalf );
				}
				else // endIndex > startIndex
				{
					m_DmaStage = DmaStage::READING_FINISHING;

					// we can just read ABUFFER_SIZE contiguous samples
					m_DelayBuffer->readFromMedia( (m_DelayLineOffset + startIndex) * sizeof(int16_t), m_SharedData );
				}
			}
			else if ( m_DmaStage == DmaStage::READING_FIRST_HALF )
			{
				const unsigned int endIndex = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;
				const unsigned int startIndex = m_DelayReadIncr;

				const unsigned int firstHalfSize = m_DelayLength - startIndex;
				const unsigned int secondHalfSize = endIndex;

				const SharedData<uint8_t> secondHalf
					= SharedData<uint8_t>::MakeSharedData( secondHalfSize * sizeof(int16_t), m_SharedData.getPtr() + (firstHalfSize * sizeof(int16_t)) );

				m_DmaStage = DmaStage::READING_FINISHING;
				m_DelayBuffer->readFromMedia( (m_DelayLineOffset) * sizeof(int16_t), secondHalf );
			}
			else if ( m_DmaStage == DmaStage::READING_FINISHING )
			{
				m_DmaStage = DmaStage::SEQUENCE_COMPLETE;
			}
		}

		void call (int16_t* writeBuffer) override
		{
#ifdef TARGET_BUILD // using dma
			// wait for transfer complete
			while ( m_DmaStage != DmaStage::SEQUENCE_COMPLETE ) {}

			// write read data into writeBuffer
			int16_t* readDataPtr = reinterpret_cast<int16_t*>( m_SharedData.getPtr() );
			for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
			{
				int16_t delayedVal = readDataPtr[sample];

				// use the same buffer (readData) to write to
#ifdef USE_ALLPASS_FOR_GLINT_SIMPLE_DELAY
				int16_t sampleVal = writeBuffer[sample];
				int16_t newSampleVal = ( sampleVal - (delayedVal * m_Feedback) );
				delayedVal = ( (newSampleVal * m_Feedback) + delayedVal );
				readDataPtr[sample] = ( delayedVal * (m_Feedback + 0.087f) ) + ( sampleVal * (1.0f - (m_Feedback + 0.087f)) );
#else // use normal delay
				readDataPtr[sample] = ( writeBuffer[sample] + static_cast<int16_t>(delayedVal * m_Feedback) ) / 2;
#endif

				// write the delayed value to the actual write buffer
				writeBuffer[sample] = delayedVal;
			}

			const unsigned int endWriteIndex = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
			const unsigned int startWriteIndex = m_DelayWriteIncr;

			if ( endWriteIndex < startWriteIndex && endWriteIndex != 0 )
			{
				// we need to wrap around the delay buffer
				unsigned int firstHalfSize = m_DelayLength - startWriteIndex;
				const SharedData<uint8_t> firstHalf
					= SharedData<uint8_t>::MakeSharedData( firstHalfSize * sizeof(int16_t), m_SharedData.getPtr() );

				// write first half to media
				m_DmaStage = DmaStage::WRITING_FIRST_HALF;
				m_DelayBuffer->writeToMedia( firstHalf, (m_DelayLineOffset + startWriteIndex) * sizeof(int16_t) );
			}
			else // endWriteIndex > startWriteIndex
			{
				// we can just write contiguous samples
				m_DmaStage = DmaStage::WRITING_FINISHING;
				m_DelayBuffer->writeToMedia( m_SharedData, (m_DelayLineOffset + startWriteIndex) * sizeof(int16_t) );
			}

#else // no dma on host build
			const unsigned int endIndex = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;
			const unsigned int startIndex = m_DelayReadIncr;

			if ( endIndex < startIndex && endIndex != 0 )
			{
				// we need to wrap around the delay buffer
				const unsigned int firstHalfSize = m_DelayLength - startIndex;
				const unsigned int secondHalfSize = endIndex;

				const SharedData<uint8_t> firstHalf
					= SharedData<uint8_t>::MakeSharedData( firstHalfSize * sizeof(int16_t), m_SharedData.getPtr() );
				const SharedData<uint8_t> secondHalf
					= SharedData<uint8_t>::MakeSharedData( secondHalfSize * sizeof(int16_t), m_SharedData.getPtr() + (firstHalfSize * sizeof(int16_t)) );

				// read both halves from media
				m_DelayBuffer->readFromMedia( (m_DelayLineOffset + startIndex) * sizeof(int16_t), firstHalf );
				m_DelayBuffer->readFromMedia( (m_DelayLineOffset) * sizeof(int16_t), secondHalf );
			}
			else // endIndex > startIndex
			{
				// we can just read ABUFFER_SIZE contiguous samples
				m_DelayBuffer->readFromMedia( (m_DelayLineOffset + startIndex) * sizeof(int16_t), m_SharedData );
			}

			// write read data into writeBuffer
			int16_t* readDataPtr = reinterpret_cast<int16_t*>( m_SharedData.getPtr() );
			for ( unsigned int sample = 0; sample < ABUFFER_SIZE; sample++ )
			{
				int16_t delayedVal = readDataPtr[sample];

				// use the same buffer (readData) to write to
				readDataPtr[sample] = ( writeBuffer[sample] + static_cast<int16_t>(delayedVal * m_Feedback) ) / 2;

				// write the delayed value to the actual write buffer
				writeBuffer[sample] = delayedVal;
			}

			const unsigned int endWriteIndex = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
			const unsigned int startWriteIndex = m_DelayWriteIncr;

			if ( endWriteIndex < startWriteIndex && endWriteIndex != 0 )
			{
				// we need to wrap around the delay buffer
				unsigned int firstHalfSize = m_DelayLength - startWriteIndex;
				unsigned int secondHalfSize = ABUFFER_SIZE - firstHalfSize;
				const SharedData<uint8_t> firstHalf
					= SharedData<uint8_t>::MakeSharedData( firstHalfSize * sizeof(int16_t), m_SharedData.getPtr() );
				const SharedData<uint8_t> secondHalf
					= SharedData<uint8_t>::MakeSharedData( secondHalfSize * sizeof(int16_t), m_SharedData.getPtr() + (firstHalfSize * sizeof(int16_t)) );

				// write both halves to media
				m_DelayBuffer->writeToMedia( firstHalf, (m_DelayLineOffset + startWriteIndex) * sizeof(int16_t) );
				m_DelayBuffer->writeToMedia( secondHalf, (m_DelayLineOffset) * sizeof(int16_t) );
			}
			else // endWriteIndex > startWriteIndex
			{
				// we can just write contiguous samples
				m_DelayBuffer->writeToMedia( m_SharedData, (m_DelayLineOffset + startWriteIndex) * sizeof(int16_t) );
			}

			m_DelayWriteIncr = ( m_DelayWriteIncr + ABUFFER_SIZE ) % m_DelayLength;
			m_DelayReadIncr = ( m_DelayReadIncr + ABUFFER_SIZE ) % m_DelayLength;
#endif
		}
private:
		unsigned int 			m_DelayLength;
		STORAGE* 			m_DelayBuffer;
		unsigned int 			m_DelayWriteIncr;
		unsigned int 			m_DelayReadIncr;
		unsigned int 			m_DelayLineOffset; // since we're using one contiguous storage device, offset each delay
		static unsigned int 		m_RunningDelayLineOffset; // use this value to determine where to put new delay lines in memory
		const SharedData<uint8_t> 	m_SharedData; // we'll have to use another block of memory since we're so limited
		float 				m_Feedback;
		volatile DmaStage 		m_DmaStage;
};

class GlintManager : public IBufferCallback<uint16_t>, public IGlintParameterEventListener
{
	public:
		GlintManager (STORAGE* delayBufferStorage);
		~GlintManager() override;

		void setDecayTime (float decayTime); // decayTime should be between 0.0f and 1.0f
		void setDiffusion (float diffusion); // diffusion should be between 0.0f and 1.0f
		void setFiltFreq (float filtFreq); // filtFreq should be in hertz

		void call (uint16_t* writeBuffer) override;

		void onGlintParameterEvent (const GlintParameterEvent& paramEvent) override;

	private:
		NoiseGate<int16_t> 		m_NoiseGate;

		STORAGE* 			m_StorageMedia; // where the static delay buffers sit
		unsigned int 			m_StorageMediaSize;

		float 				m_DecayTime;
		float 				m_FiltFreq;

		// diffusion network filters
		float 				m_Diffusion; // should be between 0.0f and 1.0f
		AllpassCombFilter<int16_t> 	m_DiffusionAPF1;
		AllpassCombFilter<int16_t> 	m_DiffusionAPF2;
		AllpassCombFilter<int16_t> 	m_DiffusionAPF3;
		AllpassCombFilter<int16_t> 	m_DiffusionAPF4;

		// low-pass filters
		OnePoleFilter<int16_t> 		m_LowpassFilter;

		// reverberation network filters
		PolyBLEPOsc 			m_ReverbNetModOsc;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock1APF1;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock1APF2;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock1APF3;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock1APF4;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock2APF1;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock2APF2;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock2APF3;
		AllpassCombFilter<int16_t> 	m_ReverbNetBlock2APF4;
		GlintStorageAllpassCombFilter 	m_ReverbNetStorageMediaAPF;

		int16_t 			m_ReverbNetStorageMediaAPFBuffer[ABUFFER_SIZE]; // buffer for use with GlintStorageAllpassCombFilter

		int16_t 			m_PrevReverbNetVals[ABUFFER_SIZE]; // for feedback into low-pass
		int16_t 			m_PrevReverbNetBlock2Vals[ABUFFER_SIZE]; // for feedback into reverb block 1
};

#endif // GLINTMANAGER_HPP
