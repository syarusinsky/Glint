#ifndef GLINTPRESETUPGRADER_HPP
#define GLINTPRESETUPGRADER_HPP

/*************************************************************************
 * The GlintPresetUpgrader is an IPresetUpgrader that is meant to be
 * passed to the PresetManager's upgradePresets function. It holds a
 * record of each version of the GlintState preset struct in its
 * cpp file for comparisons when upgrading.
*************************************************************************/

#include "PresetManager.hpp"

#include "GlintManager.hpp"

class GlintPresetUpgrader : public IPresetUpgrader
{
	public:
		GlintPresetUpgrader(const GlintState& initPreset, const GlintPresetHeader& currentPresetHeader);
		~GlintPresetUpgrader() override;

		void upgradePresets() override;

	private:
		GlintState 		m_InitPreset;
		GlintPresetHeader 	m_CurrentPresetHeader;

		// this is where the upgrade functions would go, for example
		// void upgradeFrom1_0_0To1_1_0();
};

#endif // GLINTPRESETUPGRADER_HPP
