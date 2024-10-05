#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// Global objects
DaisySeed hw;
Metro intClock;
GateIn extClock, hold;
Switch button1, button2;

// Pin numbers
constexpr Pin RATE_PIN      = seed::A0;
constexpr Pin CHANCE_PIN    = seed::A1;
constexpr Pin LENGTH_PIN    = seed::A2;
constexpr Pin SCALE_PIN     = seed::A3;
constexpr Pin SLIDES_PIN    = seed::A4;
constexpr Pin EXT_CLOCK_PIN = seed::D0;
constexpr Pin HOLD_PIN      = seed::D1;
constexpr Pin BTN1_PIN      = seed::D29;
constexpr Pin BTN2_PIN      = seed::D30;

// LED variables
const int numLeds = 8;
const float brightness = 0.2f;
Led leds[numLeds];

// Sequence variables
int patternSteps = 8;
int step = -1;
bool ext_clock_mode = true;


void InitADC()
{
    const int numAdcChannels = 5;
    const Pin adcPins[numAdcChannels] = {
        RATE_PIN,
        CHANCE_PIN,
        LENGTH_PIN,
        SCALE_PIN,
        SLIDES_PIN
    };

	AdcChannelConfig adcConfig[numAdcChannels];
    for (int i = 0; i < numAdcChannels; i++) {
        adcConfig[i].InitSingle(adcPins[i]);
    }
	hw.adc.Init(adcConfig, numAdcChannels);
	hw.adc.Start();
}

void InitLeds()
{
    const int ledPins[numLeds] = {7, 8, 9, 10, 11, 12, 13, 14};
    for (int i = 0; i < numLeds; i++) {
        leds[i].Init(hw.GetPin(ledPins[i]), false);
    }
}

void UpdateLeds()
{
    for (int i = 0; i < numLeds; i++) {
        if (step == i) {
            leds[i].Set(1.0f);
        } else {
            leds[i].Set(brightness);
        }
        leds[i].Update();
    }
}

void UpdateStep()
{
    if (ext_clock_mode) {
        if(extClock.Trig())
        {
            step++;
            step = step % patternSteps;
        }
    } else {
        if(intClock.Process())
        {
            step++;
            step = step % patternSteps;
        }
    }
}

void ProcessButtons()
{
    button1.Debounce();
    button2.Debounce();
    if (button1.RisingEdge()) {
        ext_clock_mode = !ext_clock_mode;
    }
    if (button2.RisingEdge()) {
        step = -1;
    }
}

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        UpdateStep();
    }
}

int main(void)
{
    // hardware
    hw.Configure();
    hw.Init();
    const float sampleRate = hw.AudioSampleRate();
    InitADC();
    InitLeds();

    // internal metronome
    intClock.Init(2, sampleRate);

    // gate inputs
    extClock.Init(EXT_CLOCK_PIN);
    hold.Init(HOLD_PIN);

    // switches (momentary push buttons)
    button1.Init(BTN1_PIN, 1000);
    button2.Init(BTN2_PIN, 1000);

    // start logging and wait for a serial connection
    // hw.StartLog(true);
    // hw.PrintLine("Initialized");

    // start the audio callback
    hw.StartAudio(AudioCallback);

    // main loop
    while(1) {
        intClock.SetFreq((hw.adc.GetFloat(0) * 50.0f) + 0.5);
        UpdateLeds();
        ProcessButtons();
    }
}
