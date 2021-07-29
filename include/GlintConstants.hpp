#ifndef GLINTCONSTANTS_HPP
#define GLINTCONSTANTS_HPP

#include "AudioConstants.hpp"

#ifdef TARGET_BUILD
const unsigned int GLINT_DIFFUSE_LEN_1 = 59;
const unsigned int GLINT_DIFFUSE_LEN_2 = 43;
const unsigned int GLINT_DIFFUSE_LEN_3 = 157;
const unsigned int GLINT_DIFFUSE_LEN_4 = 113;
#else
const unsigned int GLINT_DIFFUSE_LEN_1 = 139;
const unsigned int GLINT_DIFFUSE_LEN_2 = 107;
const unsigned int GLINT_DIFFUSE_LEN_3 = 379;
const unsigned int GLINT_DIFFUSE_LEN_4 = 277;
#endif

const float GLINT_MAX_FILT_FREQ = 20000.0f;
const float GLINT_MIN_FILT_FREQ = 1.0f;

const unsigned int GLINT_POT_STABIL_NUM = 50; // pot stabilization stuff, see AkiDelayUiManager
const float GLINT_POT_STABIL_ALLOWED_SCATTER = 0.05f; // allow 5% of jitter on pots

enum class POT_CHANNEL : unsigned int
{
	DECAY_TIME 	= 0,
	MOD_RATE 	= 1,
	FILT_FREQ 	= 2
};

enum class BUTTON_CHANNEL: unsigned int
{
	EFFECT_BTN_1 	= 0,
	EFFECT_BTN_2 	= 1
};

#endif // GLINTCONSTANTS_HPP
