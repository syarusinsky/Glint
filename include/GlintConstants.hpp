#ifndef GLINTCONSTANTS_HPP
#define GLINTCONSTANTS_HPP

#include "AudioConstants.hpp"

#include <stdint.h>

constexpr uint8_t GLINT_MODEL_ID = 0x01;

const unsigned int GLINT_DIFFUSE_LEN_1 = 149;
const unsigned int GLINT_DIFFUSE_LEN_2 = 211;
const unsigned int GLINT_DIFFUSE_LEN_3 = 331;
const unsigned int GLINT_DIFFUSE_LEN_4 = 431;

const unsigned int GLINT_REVERBNET1_APF_LEN_1 = 613;
const unsigned int GLINT_REVERBNET1_APF_LEN_2 = 883;
const unsigned int GLINT_REVERBNET1_APF_LEN_3 = 1153;
const unsigned int GLINT_REVERBNET1_APF_LEN_4 = 1429;
const unsigned int GLINT_REVERBNET2_APF_LEN_1 = 1601;
const unsigned int GLINT_REVERBNET2_APF_LEN_2 = 1907;
const unsigned int GLINT_REVERBNET2_APF_LEN_3 = 2309;
const unsigned int GLINT_REVERBNET2_APF_LEN_4 = 2713;
const unsigned int GLINT_REVERBNET_SMAF_LEN   = 7507;

const float GLINT_MAX_FILT_FREQ = 20000.0f;
const float GLINT_MIN_FILT_FREQ = 1.0f;

const unsigned int GLINT_POT_STABIL_NUM = 50; // pot stabilization stuff, see AkiDelayUiManager
const float GLINT_POT_STABIL_ALLOWED_SCATTER = 0.05f; // allow 5% of jitter on pots

// channel order should be associated to pot channel order (ie: effect1 pot channel means decay time)
enum class PARAM_CHANNEL : unsigned int
{
	DECAY_TIME 		= 0,
	DIFFUSION 		= 1,
	FILT_FREQ 		= 2,
	NEXT_PRESET 		= 3,
	PREV_PRESET 		= 4,
	WRITE_PRESET 		= 5,
	SEND_PRESET 		= 6,
	SEND_ALL_PRESETS 	= 7,
	ACCEPT_PRESET 		= 8,
	DENY_PRESET 		= 9,
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
