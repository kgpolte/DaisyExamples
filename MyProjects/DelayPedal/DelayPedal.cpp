
#include "daisysp.h"
#include "daisy_seed.h"

// Interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)

// Set max delay time to 0.75 of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 0.75f)

using namespace daisysp;
using namespace daisy;
using namespace seed;

static DaisySeed hw;

// Dual Delay Lines
static DelayLine<float, MAX_DELAY> del_l, del_r;


// Analog Controls
const int num_pots = 6;		// hardware variables
const int pot_pins[num_pots] = {15,	16,	17,	18,	19,	20};
const float pot_move_threshold = 0.01f;

float lvl_time = 0.5f;		// Effect parameters
float lvl_mix = 0.5f;
float lvl_feedback = 0.5f;

const int index_time = 0; 	// AdcConfig indeces
const int index_mix = 1;
const int index_feedback = 2;


// Digital Control Stuff



// ------------------------------------------------------------------

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    float
		feedback_l,
		feedback_r,
		del_l_out,
		del_r_out,
		sig_l_in,
		sig_r_in,
		sig_l_out,
		sig_r_out;

    for(size_t i = 0; i < size; i += 2)
    {
        // Read from delay lines and inputs
        del_l_out = del_l.Read();
		del_r_out = del_r.Read();
		sig_l_in = in[LEFT];
		sig_r_in = in[RIGHT];

        // Calculate outputs and feedback
        sig_l_out  = (del_l_out * lvl_mix) + (sig_l_in * (1.f - lvl_mix));
        feedback_l = (del_l_out * lvl_feedback) + sig_l_in;
		sig_r_out  = (del_r_out * lvl_mix) + (sig_r_in * (1.f - lvl_mix));
        feedback_r = (del_r_out * lvl_feedback) + sig_r_in;

        // Write to the delay lines and outputs
        del_l.Write(feedback_l);
		del_r.Write(feedback_r);
        out[LEFT]  = sig_l_out;
        out[RIGHT] = sig_r_out;
    }
}

// ------------------------------------------------------------------

void update() {
	// Only update the control level if the pot has moved enough

	// Time
	const float reading_time = hw.adc.GetFloat(index_time);
	if (abs(reading_time - lvl_time) > pot_move_threshold) {
		del_l.SetDelay(MAX_DELAY * reading_time);
		del_r.SetDelay(MAX_DELAY * reading_time);
		lvl_time = reading_time;
	}

	// Mix
	const float reading_mix = hw.adc.GetFloat(index_mix);
	if (abs(reading_mix - lvl_mix) > pot_move_threshold) {
		lvl_mix = reading_mix;
	}

	// Feedback
	const float reading_feedback = hw.adc.GetFloat(index_feedback);
	if (abs(reading_feedback - lvl_feedback) > pot_move_threshold) {
		lvl_feedback = reading_feedback;
	}
}

// ------------------------------------------------------------------

int main(void)
{
	// hardware config
    float sample_rate;
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();

	// ADC config and initialization
	AdcChannelConfig adc_config[num_pots];
	for (int i = 0; i < num_pots; i++) {
		adc_config[i].InitSingle(hw.GetPin(pot_pins[i]));
	}
	hw.adc.Init(adc_config, num_pots);
	hw.adc.Start();

	// delay line config
    del_l.Init();
	del_r.Init();

	// start the audio callback
    hw.StartAudio(AudioCallback);

    while(1) {
		update();
	}
}
