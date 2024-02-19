#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;


DaisySeed      hw;
AnalogBassDrum bd;
Switch         sw_1;

// ------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------

#define NUM_ADC_CHANNELS	5
#define FREQ_PIN 			19
#define DECAY_PIN 			20
#define TONE_PIN			21
#define FREQ_CV_PIN			16
#define DECAY_CV_PIN		17

#define TRIG_PIN			15

#define MIN_FREQ			30
#define FREQ_RANGE			50

// ------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
		sw_1.Debounce();
		bool t = sw_1.RisingEdge();
		
		// If a trigger is received read the controls and set the drum's properties
		if (t) { 
			float freqToSet = MIN_FREQ + (hw.adc.GetFloat(0) * 30.f) + (hw.adc.GetFloat(3) * FREQ_RANGE);
			if (freqToSet > MIN_FREQ + FREQ_RANGE) {
				freqToSet = MIN_FREQ + FREQ_RANGE;
			}

			float decayToSet = hw.adc.GetFloat(1) + hw.adc.GetFloat(4);
			if (decayToSet > 1) {
				decayToSet = 1.f;
			}

			bd.SetFreq(freqToSet);
			bd.SetDecay(decayToSet);
			bd.SetTone(hw.adc.GetFloat(2));
		}

        out[0][i] = out[1][i] = bd.Process(t);
    }
}

// ------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------

int main(void)
{
	// Initialize the hardware
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

	// Initialize the AnalogBassDrum object
    bd.Init(sample_rate);
	bd.SetAttackFmAmount(.1f);
    bd.SetSelfFmAmount(.8f);

	// Initialize the trigger switch
    sw_1.Init(hw.GetPin(TRIG_PIN), sample_rate / 48.f);

	// Create an array to hold all ADC configs
	AdcChannelConfig adcConfig[NUM_ADC_CHANNELS];

	// Initialize the individual ADC pins
	adcConfig[0].InitSingle(hw.GetPin(FREQ_PIN));
	adcConfig[1].InitSingle(hw.GetPin(DECAY_PIN));
	adcConfig[2].InitSingle(hw.GetPin(TONE_PIN));
	adcConfig[3].InitSingle(hw.GetPin(FREQ_CV_PIN));
	adcConfig[4].InitSingle(hw.GetPin(DECAY_CV_PIN));

	// Initialize the hardware ADC
	hw.adc.Init(adcConfig, NUM_ADC_CHANNELS);
	hw.adc.Start();

    hw.StartAudio(AudioCallback);
    while(1) {}
}