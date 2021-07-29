#ifndef GLINTMANAGER_HPP
#define GLINTMANAGER_HPP

#include "IBufferCallback.hpp"
#include "IStorageMedia.hpp"
#include "AudioConstants.hpp"
#include "IGlintParameterEventListener.hpp"
#include "AllpassCombFilter.hpp"

#include <stdint.h>

class GlintManager : public IBufferCallback<uint16_t>, public IGlintParameterEventListener
{
	public:
		GlintManager (IStorageMedia* delayBufferStorage);
		~GlintManager() override;

		void setDecayTime (float decayTime); // decayTime should be between 0.0f and 1.0f
		void setModRate (float modRate); // modRate should be in hertz
		void setFiltFreq (float filtFreq); // filtFreq should be in hertz

		void call (uint16_t* writeBuffer) override;

		void onGlintParameterEvent (const GlintParameterEvent& paramEvent) override;

	private:
		IStorageMedia* 			m_StorageMedia; // where the static delay buffers sit
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
};

#endif // GLINTMANAGER_HPP
