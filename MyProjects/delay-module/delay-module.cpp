// TO DO

// Add clock sync
// Add tap tempo and clock divisions/multiplications
// Add a ping-pong mode
// Make an extended gate input class with edge detection
// Make a Red/Blue LED class

// Increase blue LED resistors to 3K3
// Separate knobs from time CV inputs

// --------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "daisysp.h"

// --------------------------------------------------------------------

// interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)

// set the absolute max delay time
#define MAX_DELAY static_cast<size_t>(96000 * 10.0f)

// set the min/max delay times for each mode
#define MIN_DELAY_0 static_cast<size_t>(96000 * 0.0001f)
#define MAX_DELAY_0 static_cast<size_t>(96000 * 0.1f)
#define MIN_DELAY_1 static_cast<size_t>(96000 * 0.01f)
#define MAX_DELAY_1 static_cast<size_t>(96000 * 0.5f)
#define MIN_DELAY_2 static_cast<size_t>(96000 * 0.1f)
#define MAX_DELAY_2 static_cast<size_t>(96000 * 2.0f)
#define MIN_DELAY_3 static_cast<size_t>(96000 * 1.0f)
#define MAX_DELAY_3 static_cast<size_t>(96000 * 4.0f)

// --------------------------------------------------------------------

using namespace daisysp;
using namespace daisy;

/** Typedef the OledDisplay to make syntax cleaner below 
 *  This is a 4Wire SPI Transport controlling an 128x64 sized SSDD1306
*/
using MyOledDisplay = OledDisplay<SSD130x4WireSpi128x64Driver>;

// --------------------------------------------------------------------

// daisy modules
static DaisySeed hw;
static AnalogControl cv_0, cv_1, cv_2, cv_3;
static GateIn clock, hold;
static Led led_1_b, led_1_r, led_2_b, led_2_r;
static CrossFade cross_fader;
static Switch switch_1, switch_2;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delay_l;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delay_r;
static MyOledDisplay display;

// --------------------------------------------------------------------

// gate input state tracking for edge detection
bool clock_state = false;
bool hold_state = false;

// mode variables
bool clock_sync = false;
bool channel_link = false;
int time_range = 0;

// clock sync variables

// misc. global settings
const float LOW_BRIGHTNESS = 0.4f;
const int long_press_time = 2000;
int sw1_last_time_held;
int sw2_last_time_held;

// analog control states
float   time_factor_l,
        time_factor_r,
        fdbk_amount,
        mix;

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
	cv_0.Init(hw.adc.GetPtr(0), sample_rate, true, false, 0.01f);
    cv_1.Init(hw.adc.GetPtr(1), sample_rate, true, false, 0.01f);
    cv_2.Init(hw.adc.GetPtr(2), sample_rate, true, false, 0.01f);
    cv_3.Init(hw.adc.GetPtr(3), sample_rate, true, false, 0.01f);
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
    led_2_b.Init(seed::D4, false);
    led_2_r.Init(seed::D3, false);
}

static void UpdateDisplay()
{
    // Max 18 characters per line at 7x10 font size
    // There's probably a better way to increase this
    // and do wrapping...
    char str_buffer[128];

    // Clear the display
    display.Fill(false);

    // Print the first line: external clock listen
    sprintf(str_buffer, clock_sync ? "Sync: On" : "Sync: Off");
    display.SetCursor(0, 0);
    display.WriteString(str_buffer, Font_7x10, true);

    // Print the second line: time range
    switch (time_range) {
        case 0: sprintf(str_buffer, "Range: X Fast"); break;
        case 1: sprintf(str_buffer, "Range: Fast"); break;
        case 2: sprintf(str_buffer, "Range: Med"); break;
        case 3: sprintf(str_buffer, "Range: Slow"); break;
    }
    display.SetCursor(0, 12);
    display.WriteString(str_buffer, Font_7x10, true);

    // Print the third line: channel link
    sprintf(str_buffer, channel_link ? "Link: On" : "Link: Off");
    display.SetCursor(0, 24);
    display.WriteString(str_buffer, Font_7x10, true);
    display.WriteStringAligned("~KG DELAY beta v0.1~", Font_6x8, display.GetBounds(), Alignment::bottomCentered, true);

    // Update the display
    display.Update();
}

static void IncrementTimeRange()
{
    time_range++;
    if (time_range >= 4) {
        time_range = 0;
    }
}

static void ToggleClockSync()
{
    clock_sync = !clock_sync;
    led_1_b.Set(0.0f);
    led_1_r.Set(0.0f);
}

static void ToggleChannelLink()
{
    channel_link = !channel_link;
}

static void ProcessClockInput()
{
    // get the current state of the input
    bool clock_read = clock.State();

    // check for rising and falling edges
    if (clock_read && clock_read != clock_state) {
        // set the led to full
        led_1_b.Set(1.0f);
        led_1_r.Set(1.0f);

        // update the stored clock state
        clock_state = clock_read;

    } else if (!clock_read && clock_read != clock_state) {
        // set the led to low brightness between pulses
        led_1_b.Set(0);
        led_1_r.Set(0);

        // update the stored clock state
        clock_state = clock_read;
    }
}

static void ProcessSwitches()
{
    bool sw1_released, sw2_released, sw1_long_pressed, sw2_long_pressed;

    // debounce the switches
    switch_1.Debounce();
    switch_2.Debounce();

    // switch 1
    if (switch_1.Pressed()) sw1_last_time_held = switch_1.TimeHeldMs();
    sw1_long_pressed = sw1_last_time_held >= long_press_time;
    sw1_released = switch_1.FallingEdge();
    if (sw1_released) {
        sw1_long_pressed ? ToggleChannelLink() : ToggleClockSync();
        UpdateDisplay();
    }

    // switch 2
    if (switch_2.Pressed()) sw2_last_time_held = switch_2.TimeHeldMs();
    sw2_long_pressed = sw2_last_time_held >= long_press_time;
    sw2_released = switch_2.FallingEdge();
    if (sw2_released) {
        sw2_long_pressed ? ToggleChannelLink() : IncrementTimeRange();;
        UpdateDisplay();
    }
}

static void ProcessGateInputs()
{
    if (clock_sync) {
        ProcessClockInput();
    }
}

static void ProcessAnalogControls()
{
    float set_time_l, set_time_r;

    time_factor_l = cv_0.Process();
    time_factor_r = channel_link ? time_factor_l : cv_1.Process();
    fdbk_amount = cv_2.Process();
    mix = cv_3.Process();
    cross_fader.SetPos(mix);

    switch (time_range) {
        case 0: 
            set_time_l = (MAX_DELAY_0 * time_factor_l) + MIN_DELAY_0;
            set_time_r = (MAX_DELAY_0 * time_factor_r) + MIN_DELAY_0;
            break;
        case 1:
            set_time_l = (MAX_DELAY_1 * time_factor_l) + MIN_DELAY_1;
            set_time_r = (MAX_DELAY_1 * time_factor_r) + MIN_DELAY_1;
            break;
        case 2:
            set_time_l = (MAX_DELAY_2 * time_factor_l) + MIN_DELAY_2;
            set_time_r = (MAX_DELAY_2 * time_factor_r) + MIN_DELAY_2;
            break;
        case 3:
            set_time_l = (MAX_DELAY_3 * time_factor_l) + MIN_DELAY_3;
            set_time_r = (MAX_DELAY_3 * time_factor_r) + MIN_DELAY_3;
            break;
    }

    delay_l.SetDelay(set_time_l);
    delay_r.SetDelay(set_time_r);
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
    float   feedback_l,
            feedback_r,
            del_out_l,
            del_out_r,
            sig_in_l,
            sig_in_r,
            sig_out_l,
            sig_out_r;

    ProcessAnalogControls();

    for(size_t i = 0; i < size; i += 2)
    {
        // Read from the inputs and delay line
        sig_in_l = in[LEFT];
        sig_in_r = in[RIGHT];
        del_out_l = delay_l.Read();
        del_out_r = delay_r.Read();

        // Calculate outputs and feedback
        sig_out_l  = cross_fader.Process(sig_in_l, del_out_l);
        feedback_l = (del_out_l * fdbk_amount) + sig_in_l;
        sig_out_r  = cross_fader.Process(sig_in_r, del_out_r);
        feedback_r = (del_out_r * fdbk_amount) + sig_in_r;

        // Write to the delay lines
        delay_l.Write(feedback_l);
        delay_r.Write(feedback_r);

        // Set the outputs
        out[LEFT]  = sig_out_l;
        out[RIGHT] = sig_out_r;
    }
}

int main(void)
{
    float sample_rate;

    // initialize the seed hardware
    hw.Configure();
    hw.Init();
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_96KHZ);
    hw.SetAudioBlockSize(16);
    sample_rate = hw.AudioSampleRate();

    // configure and initialize the display
    MyOledDisplay::Config disp_cfg;
    disp_cfg.driver_config.transport_config.pin_config.dc    = hw.GetPin(9);
    disp_cfg.driver_config.transport_config.pin_config.reset = hw.GetPin(0);
    display.Init(disp_cfg);
    UpdateDisplay();

    // other initializations
    InitAnalogControls(sample_rate);
    InitDac();
    InitGpio();
    InitDspModules();

    // start the audio callback
    hw.StartAudio(AudioCallback);
    
    while(1) {
        ProcessSwitches();
        ProcessGateInputs();
        UpdateLeds();
	}
}
