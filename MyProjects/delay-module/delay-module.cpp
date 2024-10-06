// TO DO
// Add clock sync
// Add tap tempo
// Add split/linked modes for delay time

#include "daisysp.h"
#include "daisy_seed.h"

// Interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)

// Set max delay time to 1 second
#define MAX_DELAY static_cast<size_t>(48000)

using namespace daisysp;
using namespace daisy;

static DaisySeed hw;
AnalogControl cv_0, cv_1, cv_2, cv_3;

// Declare 2 DelayLines of MAX_DELAY number of floats.
static DelayLine<float, MAX_DELAY> delay_l, delay_r;

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    float   time_factor_l,
            time_factor_r,
            fdbk_amount,
            mix,
            feedback_l,
            feedback_r,
            del_out_l,
            del_out_r,
            sig_in_l,
            sig_in_r,
            sig_out_l,
            sig_out_r;

    time_factor_l = cv_0.Process();
    time_factor_r = cv_1.Process();
    fdbk_amount = cv_2.Process();
    mix = cv_3.Process();

    delay_l.SetDelay(MAX_DELAY * time_factor_l);
    delay_r.SetDelay(MAX_DELAY * time_factor_r);

    for(size_t i = 0; i < size; i += 2)
    {
        // Read from the inputs and delay line
        sig_in_l = in[LEFT];
        sig_in_r = in[RIGHT];
        del_out_l = delay_l.Read();
        del_out_r = delay_r.Read();

        // Calculate output and feedback
        sig_out_l  = del_out_l + sig_in_l;
        feedback_l = (del_out_l * fdbk_amount) + sig_in_l;
        sig_out_r  = del_out_r + sig_in_r;
        feedback_r = (del_out_r * fdbk_amount) + sig_in_r;

        // Write to the delay
        delay_l.Write(feedback_l);
        delay_r.Write(feedback_r);

        // Set the outputs
        out[LEFT]  = sig_out_l;
        out[RIGHT] = sig_out_r;
    }
}

int main(void)
{
    // initialize seed hardware and daisysp modules
    float sample_rate;
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();

	// initialize the delay lines
    delay_l.Init();
    delay_l.SetDelay(MAX_DELAY);
    delay_r.Init();
    delay_r.SetDelay(MAX_DELAY);

	// initialize the ADC
	const int num_adc_channels = 4;
	AdcChannelConfig adcConfig[num_adc_channels];
	adcConfig[0].InitSingle(seed::A0);
    adcConfig[1].InitSingle(seed::A1);
    adcConfig[2].InitSingle(seed::A2);
    adcConfig[3].InitSingle(seed::A3);
	hw.adc.Init(adcConfig, num_adc_channels);
	hw.adc.Start();

	// initialize the AnalogControls
	cv_0.Init(hw.adc.GetPtr(0), sample_rate, true, false, 0.01f);
    cv_1.Init(hw.adc.GetPtr(1), sample_rate, true, false, 0.01f);
    cv_2.Init(hw.adc.GetPtr(2), sample_rate, true, false, 0.01f);
    cv_3.Init(hw.adc.GetPtr(3), sample_rate, true, false, 0.01f);

    // start callback
    hw.StartAudio(AudioCallback);

    while(1) {
		
	}
}
