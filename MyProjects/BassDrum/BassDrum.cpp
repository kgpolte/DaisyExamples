#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed      hw;
AnalogBassDrum bd;
Switch         sw_1;

#define NUM_ADC_CHANNELS	3
#define FREQ_PIN 			17
#define DECAY_PIN 			18
#define TONE_PIN			19

// ------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
	sw_1.Debounce();

    for(size_t i = 0; i < size; i++)
    {
		bool t = sw_1.RisingEdge();
		
		if (t) { // If a trigger is received set the controls to the current pot values
			bd.SetFreq(40 + (hw.adc.GetFloat(0) * 30.f));
			bd.SetDecay(hw.adc.GetFloat(1));
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
    bd.SetFreq(50.f);
	bd.SetTone(.5f);
    bd.SetDecay(.5f);
    bd.SetSelfFmAmount(.5f);

	// Initialize the trigger switch
    sw_1.Init(hw.GetPin(15), sample_rate / 48.f);

	// Create an array to hold all ADC configs
	AdcChannelConfig adcConfig[NUM_ADC_CHANNELS];

	// Initialize the individual ADC pins
	adcConfig[0].InitSingle(hw.GetPin(FREQ_PIN));
	adcConfig[1].InitSingle(hw.GetPin(DECAY_PIN));
	adcConfig[2].InitSingle(hw.GetPin(TONE_PIN));

	// Initialize the hardware ADC
	hw.adc.Init(adcConfig, NUM_ADC_CHANNELS);
	hw.adc.Start();

    hw.StartAudio(AudioCallback);
    while(1) {}
}