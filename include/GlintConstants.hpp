#ifndef GLINTCONSTANTS_HPP
#define GLINTCONSTANTS_HPP

#include "AudioConstants.hpp"

// TODO haven't tested on host in a while, so all the host values are off and the faust stuff is also off

#ifdef TARGET_BUILD
const unsigned int GLINT_DIFFUSE_LEN_1 = 109;
const unsigned int GLINT_DIFFUSE_LEN_2 = 151;
const unsigned int GLINT_DIFFUSE_LEN_3 = 293;
const unsigned int GLINT_DIFFUSE_LEN_4 = 373;
#else                                         // values for 48000kHz sample rate
const unsigned int GLINT_DIFFUSE_LEN_1 = 261; // 130;
const unsigned int GLINT_DIFFUSE_LEN_2 = 319; // 159;
const unsigned int GLINT_DIFFUSE_LEN_3 = 757; // 379;
const unsigned int GLINT_DIFFUSE_LEN_4 = 557; // 277;
#endif

#ifdef TARGET_BUILD
const unsigned int GLINT_REVERBNET1_APF_LEN_1 = 2111;
const unsigned int GLINT_REVERBNET1_APF_LEN_2 = 1487;
const unsigned int GLINT_REVERBNET1_APF_LEN_3 = 887;
const unsigned int GLINT_REVERBNET1_APF_LEN_4 = 1159;
const unsigned int GLINT_REVERBNET2_APF_LEN_1 = 751;
const unsigned int GLINT_REVERBNET2_APF_LEN_2 = 2253;
const unsigned int GLINT_REVERBNET2_APF_LEN_3 = 1307;
const unsigned int GLINT_REVERBNET2_APF_LEN_4 = 2719;
const unsigned int GLINT_REVERBNET_SMAF_LEN   = 3637;
#else                                                 // values for 48000kHz sample rate
const unsigned int GLINT_REVERBNET1_APF_LEN_1 = 5077; // 2531;
const unsigned int GLINT_REVERBNET1_APF_LEN_2 = 3571; // 1789;
const unsigned int GLINT_REVERBNET1_APF_LEN_3 = 2459; // 1231;
const unsigned int GLINT_REVERBNET1_APF_LEN_4 = 1229;
const unsigned int GLINT_REVERBNET2_APF_LEN_1 = 1811; // 907;
const unsigned int GLINT_REVERBNET2_APF_LEN_2 = 5407; // 2703;
const unsigned int GLINT_REVERBNET2_APF_LEN_3 = 3137; // 1571;
const unsigned int GLINT_REVERBNET2_APF_LEN_4 = 8923; // 4451;
const unsigned int GLINT_REVERBNET_SMAF_LEN   = 3637;
#endif

const float GLINT_MAX_FILT_FREQ = 20000.0f;
const float GLINT_MIN_FILT_FREQ = 1.0f;

const unsigned int GLINT_POT_STABIL_NUM = 50; // pot stabilization stuff, see AkiDelayUiManager
const float GLINT_POT_STABIL_ALLOWED_SCATTER = 0.05f; // allow 5% of jitter on pots

enum class POT_CHANNEL : unsigned int
{
	DECAY_TIME 	= 0,
	DIFFUSION 	= 1,
	FILT_FREQ 	= 2
};

enum class BUTTON_CHANNEL: unsigned int
{
	EFFECT_BTN_1 	= 0,
	EFFECT_BTN_2 	= 1
};

#endif // GLINTCONSTANTS_HPP
