#ifndef GLINTCONSTANTS_HPP
#define GLINTCONSTANTS_HPP

#include "AudioConstants.hpp"

const unsigned int GLINT_DIFFUSE_LEN_1 = 109;
const unsigned int GLINT_DIFFUSE_LEN_2 = 151;
const unsigned int GLINT_DIFFUSE_LEN_3 = 293;
const unsigned int GLINT_DIFFUSE_LEN_4 = 373;

const unsigned int GLINT_REVERBNET1_APF_LEN_1 = 2111;
const unsigned int GLINT_REVERBNET1_APF_LEN_2 = 1357;
const unsigned int GLINT_REVERBNET1_APF_LEN_3 = 887;
const unsigned int GLINT_REVERBNET1_APF_LEN_4 = 1149;
const unsigned int GLINT_REVERBNET2_APF_LEN_1 = 751;
const unsigned int GLINT_REVERBNET2_APF_LEN_2 = 2053;
const unsigned int GLINT_REVERBNET2_APF_LEN_3 = 1307;
const unsigned int GLINT_REVERBNET2_APF_LEN_4 = 2787;
const unsigned int GLINT_REVERBNET_SMAF_LEN   = 3637;

const float GLINT_MAX_FILT_FREQ = 20000.0f;
const float GLINT_MIN_FILT_FREQ = 1.0f;

const unsigned int GLINT_POT_STABIL_NUM = 50; // pot stabilization stuff, see AkiDelayUiManager
const float GLINT_POT_STABIL_ALLOWED_SCATTER = 0.05f; // allow 5% of jitter on pots

// channel order should be associated to pot channel order (ie: effect1 pot channel means decay time)
enum class PARAM_CHANNEL : unsigned int
{
	DECAY_TIME 	= 0,
	DIFFUSION 	= 1,
	FILT_FREQ 	= 2,
	NEXT_PRESET 	= 3,
	PREV_PRESET 	= 4,
	WRITE_PRESET 	= 5
};

enum class POT_CHANNEL : unsigned int
{
	EFFECT1 	= 0,
	EFFECT2 	= 1,
	EFFECT3 	= 2
};

enum class BUTTON_CHANNEL: unsigned int
{
	EFFECT1 	= 0,
	EFFECT2 	= 1
};

#endif // GLINTCONSTANTS_HPP
