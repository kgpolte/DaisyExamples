// TO DO
// Add clock sync
// Add tap tempo and clock divisions/multiplications
// Add split/linked modes for delay time
// Add time range modes (L/M/H)
// Add a ping-pong mode

#include "daisysp.h"
#include "daisy_seed.h"

// Interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)

// Set max delay time to 2 seconds and min delay time to 0.0001 second
#define MAX_DELAY static_cast<size_t>(96000 * 2.0f)
#define MIN_DELAY static_cast<size_t>(96000 * 0.0001f)

// namespaces
using namespace daisysp;
using namespace daisy;

// Daisy modules
static DaisySeed hw;
static AnalogControl cv_0, cv_1, cv_2, cv_3;
static GateIn clock, hold;
static Led led_1_b, led_1_r, led_2_b, led_2_r;
static CrossFade cross_fader;
static Switch switch_1, switch_2;

// Global variables
bool clock_sync = false;

// 2 DelayLines of MAX_DELAY number of floats.
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delay_l;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delay_r;


// --------------------------------------------------------------------

static void InitAnalogControls(float sample_rate)
{
    // initialize the hardware ADC
    const int num_adc_channels = 4;
	AdcChannelConfig adcConfig[num_adc_channels];
	adcConfig[0].InitSingle(seed::A0);
    adcConfig[1].InitSingle(seed::A1);
    adcConfig[2].InitSingle(seed::A2);
    adcConfig[3].InitSingle(seed::A3);
	hw.adc.Init(adcConfig, num_adc_channels);
	hw.adc.Start();

    // initialize the AnalogControl objects
	cv_0.Init(hw.adc.GetPtr(0), sample_rate, true, false, 0.1f);
    cv_1.Init(hw.adc.GetPtr(1), sample_rate, true, false, 0.1f);
    cv_2.Init(hw.adc.GetPtr(2), sample_rate, true, false, 0.1f);
    cv_3.Init(hw.adc.GetPtr(3), sample_rate, true, false, 0.1f);
}

static void InitDac()
{
	DacHandle::Config dac_cfg;
	dac_cfg.bitdepth = DacHandle::BitDepth::BITS_12;
	dac_cfg.buff_state = DacHandle::BufferState::ENABLED;
	dac_cfg.mode = DacHandle::Mode::POLLING;
	dac_cfg.chn = DacHandle::Channel::BOTH;
	hw.dac.Init(dac_cfg);
	hw.dac.WriteValue(DacHandle::Channel::ONE, 0);
	hw.dac.WriteValue(DacHandle::Channel::TWO, 0);
}

static void InitDspModules()
{
    // initialize the delay lines
    delay_l.Init();
    delay_r.Init();

    // initialize the cross_fader (dry/wet control)
    cross_fader.Init();
    cross_fader.SetCurve(CROSSFADE_LIN);
}

static void InitGpio()
{
    // initialize the gate inputs
    clock.Init(seed::D26);
    hold.Init(seed::D27);

    // initialize the push button switches
    switch_1.Init(seed::D12, 1000.0f);
    switch_2.Init(seed::D11, 1000.0f);

    // initialize the LEDs
    led_1_b.Init(seed::D29, false);
    led_1_r.Init(seed::D30, false);
    led_2_b.Init(seed::D8, false);
    led_2_r.Init(seed::D7, false);
}

static void ProcessSwitches()
{
    switch_1.Debounce();
    switch_2.Debounce();

    if (switch_1.RisingEdge()) {
        clock_sync = !clock_sync;
        if (!clock_sync) {
            led_1_b.Set(0.0f);
            led_1_r.Set(0.0f);
        }
    }
}

static void UpdateLeds()
{
    led_1_b.Update();
    led_1_r.Update();
    led_2_b.Update();
    led_2_r.Update();
}

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
    cross_fader.SetPos(mix);

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
        sig_out_l  = cross_fader.Process(sig_in_l, del_out_l);
        feedback_l = (del_out_l * fdbk_amount) + sig_in_l;
        sig_out_r  = cross_fader.Process(sig_in_r, del_out_r);
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
    // initialize the seed hardware
    float sample_rate;
    hw.Configure();
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();

    // other initializations
    InitAnalogControls(sample_rate);
    InitDac();
    InitGpio();
    InitDspModules();

    // start the audio callback
    hw.StartAudio(AudioCallback);

    while(1) {
        ProcessSwitches();

        if (clock_sync) {
            if (clock.State()) {
                led_1_b.Set(1.0f);
                led_1_r.Set(1.0f);
            } else {
                led_1_b.Set(0.3f);
                led_1_r.Set(0.3f);
            }
        }
        
        UpdateLeds();
	}
}
