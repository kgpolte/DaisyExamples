#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace seed;

DaisySeed hw;
Led led_1_blue, led_1_red, led_2_blue, led_2_red;
AnalogControl cv_1, cv_2, cv_3, cv_4;
GateIn gate_in_1, gate_in_2;
Switch switch_1, switch_2;
GPIO gate_out_1, gate_out_2;

void InitAnalogControls(float sampleRate)
{
	const int num_adc_channels = 4;
	AdcChannelConfig adcConfig[num_adc_channels];
	adcConfig[0].InitSingle(A0);
	adcConfig[1].InitSingle(A1);
	adcConfig[2].InitSingle(A2);
	adcConfig[3].InitSingle(A3);
	hw.adc.Init(adcConfig, num_adc_channels);
	hw.adc.Start();

	cv_1.Init(hw.adc.GetPtr(0), sampleRate, true);
	cv_2.Init(hw.adc.GetPtr(1), sampleRate, true);
	cv_3.Init(hw.adc.GetPtr(2), sampleRate, true);
	cv_4.Init(hw.adc.GetPtr(3), sampleRate, true);
}

void UpdateLeds()
{
	if (gate_in_1.State()) {
		led_1_blue.Set(1.0f);
	} else { 
		led_1_blue.Set(0.0f); 
	}

	if (gate_in_2.State()) {
		led_2_blue.Set(1.0f);
	} else { 
		led_2_blue.Set(0.0f); 
	}

	led_1_blue.Update();
	led_1_red.Update();
	led_2_blue.Update();
	led_2_red.Update();
}

void UpdateSwitches()
{
	switch_1.Debounce();
	switch_2.Debounce();

	if (switch_1.Pressed()) {
		led_1_red.Set(1.0f);
	} else {
		led_1_red.Set(0.0f);
	}
	if (switch_2.Pressed()) {
		led_2_red.Set(1.0f);
	} else {
		led_2_red.Set(0.0f);
	}
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
	cv_1.Process();
	cv_2.Process();
	cv_3.Process();
	cv_4.Process();

	for (size_t i = 0; i < size; i++)
	{
		out[0][i] = in[0][i];
		out[1][i] = in[1][i];
	}
}

int main(void)
{
	hw.Init();
	hw.SetAudioBlockSize(4);
	hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
	const float sampleRate = hw.AudioSampleRate();

	InitAnalogControls(sampleRate);

	led_1_blue.Init(D29, false);
	led_1_blue.Set(0.0f);
	led_1_red.Init(D30, false);
	led_1_red.Set(0.0f);
	led_2_blue.Init(D8, false);
	led_2_blue.Set(0.0f);
	led_2_red.Init(D7, false);
	led_2_red.Set(0.0f);

	gate_in_1.Init(D26);
	gate_in_2.Init(D27);

	switch_1.Init(D12, 1000.0f);
	switch_2.Init(D11, 1000.0f);

	gate_out_1.Init(D13, GPIO::Mode::OUTPUT);
	gate_out_2.Init(D14, GPIO::Mode::OUTPUT);

	DacHandle::Config dacConfig;
	dacConfig.bitdepth = DacHandle::BitDepth::BITS_12;
	dacConfig.buff_state = DacHandle::BufferState::ENABLED;
	dacConfig.mode = DacHandle::Mode::POLLING;
	dacConfig.chn = DacHandle::Channel::BOTH;
	hw.dac.Init(dacConfig);
	hw.dac.WriteValue(DacHandle::Channel::BOTH, 2048);

	// hw.StartLog(true);
	// hw.PrintLine("hello");

	hw.StartAudio(AudioCallback);

	while(1) {
		UpdateLeds();
		UpdateSwitches();
	}
}
