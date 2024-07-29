#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

// -------------------------------------------------------------
// Hardware
// -------------------------------------------------------------

// Hardware objects
DaisySeed hw;
Led led1, led2;

// Hardware pin definitions
constexpr Pin PIN_KICK_TRIG     = D13;
constexpr Pin PIN_SNARE_TRIG    = D14;

constexpr Pin PIN_BTN1      	= D1;
constexpr Pin PIN_BTN2      	= D2;

constexpr Pin PIN_LED1      	= D26;
constexpr Pin PIN_LED2      	= D27;

constexpr Pin PIN_KICK_FREQ     = A0;
constexpr Pin PIN_KICK_DECAY    = A1;
constexpr Pin PIN_KICK_VEL 		= A2;
constexpr Pin PIN_KICK_FM      	= A3;
constexpr Pin PIN_KICK_TONE   	= A4;

constexpr Pin PIN_SNARE_FREQ    = A5;
constexpr Pin PIN_SNARE_DECAY   = A6;
constexpr Pin PIN_SNARE_VEL 	= A9;
constexpr Pin PIN_SNARE_BLEND   = A10;
constexpr Pin PIN_SNARE_TONE   	= A11;

// ADC indeces
const int NUM_ADC_CHANNELS 	= 10;

const int KICK_FREQ_INDEX   = 0;
const int KICK_DECAY_INDEX  = 1;
const int KICK_VEL_INDEX  	= 2;
const int KICK_FM_INDEX     = 3;
const int KICK_TONE_INDEX   = 4;

const int SNARE_FREQ_INDEX  = 5;
const int SNARE_DECAY_INDEX = 6;
const int SNARE_VEL_INDEX   = 7;
const int SNARE_BLEND_INDEX = 8;
const int SNARE_TONE_INDEX  = 9;

// -------------------------------------------------------------
// Sound Generator Class Definitions
// -------------------------------------------------------------

class BassDrum {
private:
    // hardware objects
    Switch m_trigger, m_btn;
    AnalogControl m_freqCtrl, m_decayCtrl, m_velocityCtrl, m_fmCtrl, m_toneCtrl;

    // Audio objects
    Oscillator m_osc;
    AdEnv m_ampEnv, m_pitchEnv;
    Svf m_svf;

    // Member variables
    float
        m_freqMin,
        m_freqMax,
        m_FmMin,
        m_FmMax,
        m_decayMin,
        m_decayMax,
        m_velMin,
        m_velMax,
        m_cutoffMin,
        m_cutoffMax;

    // Functions
    void m_HandleTrigger() {
        // Read AnalogControl values
        float freqCV = m_freqCtrl.Value();
        float decayCV = m_decayCtrl.Value();
        float velocityCV = m_velocityCtrl.Value();
        float fmCV = m_fmCtrl.Value();
        float toneCV = m_toneCtrl.Value();

        // Set the velocity (amplitude envelope max)
        m_ampEnv.SetMax(m_velMin + ((m_velMax - m_velMin) * velocityCV));

        // Set the volume decay time
        float decayTime = (decayCV * (m_decayMax - m_decayMin)) + m_decayMin;
        m_ampEnv.SetTime(ADENV_SEG_DECAY, decayTime);

        // Calculate min and max freqencies for the pitch envelope
        float freqMin = m_freqMin + (freqCV * (m_freqMax - m_freqMin));
        float fmAmt = m_FmMin + (fmCV * (m_FmMax - m_FmMin));
        float freqMax = freqMin + fmAmt;

        // Set min and max frequency for the pitch envelope
        m_pitchEnv.SetMin(freqMin);
        m_pitchEnv.SetMax(freqMax);

        // Trigger the envelopes
        m_ampEnv.Trigger();
        m_pitchEnv.Trigger();

        // Set the LPF cutoff freqency
        m_svf.SetFreq(m_cutoffMin + (toneCV * (m_cutoffMax - m_cutoffMin)));
    }

public:
    // TODO: Add setter methods for min/max values

    void Init(
        float samplerate,
        Pin triggerPin,
        Pin btnPin,
        int freqAdcIndex,
        int decayAdcIndex,
        int velocityAdcIndex,
        int fmAdcIndex,
        int toneAdcIndex
    ) {
        // Initialize AnalogControls
        m_freqCtrl.Init(hw.adc.GetPtr(freqAdcIndex), samplerate, true);
        m_decayCtrl.Init(hw.adc.GetPtr(decayAdcIndex), samplerate, true);
        m_velocityCtrl.Init(hw.adc.GetPtr(velocityAdcIndex), samplerate, true);
        m_fmCtrl.Init(hw.adc.GetPtr(fmAdcIndex), samplerate, true);
        m_toneCtrl.Init(hw.adc.GetPtr(toneAdcIndex), samplerate);

        // Define minimum/maximum control values
        m_freqMin   = 40.f;
        m_freqMax   = 100.f;
        m_decayMin  = 0.25f;
        m_decayMax  = 10.f;
        m_FmMin     = 50.f;
        m_FmMax     = 250.f;
        m_velMin   = 0.3f;
        m_velMax   = 1.f;
        m_cutoffMin = 40.f;
        m_cutoffMax = 400.f;

        // Initialize trigger and button inputs
        m_btn.Init(btnPin, samplerate / 48.f);
        m_trigger.Init(
            triggerPin,
            samplerate / 48.f,
            Switch::TYPE_MOMENTARY,
            Switch::POLARITY_INVERTED,
            Switch::PULL_DOWN
        );

        // Initialize oscillator
        m_osc.Init(samplerate);
        // m_osc.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
        m_osc.SetAmp(1);

        // Initizialize amplitude envelope
        // TODO: Try different curves
        m_ampEnv.Init(samplerate);
        m_ampEnv.SetTime(ADENV_SEG_ATTACK, .01);
        m_ampEnv.SetMax(1);
        m_ampEnv.SetMin(0);
        m_ampEnv.SetCurve(-20.f);

        // Initizialize pitch envelope
        // TODO: Try different curves
        m_pitchEnv.Init(samplerate);
        m_pitchEnv.SetTime(ADENV_SEG_ATTACK, .01);
        m_pitchEnv.SetTime(ADENV_SEG_DECAY, .05);

        // Initialize low pass filter
        m_svf.Init(samplerate);
        m_svf.SetDrive(0.1f);
        m_svf.SetRes(0.1f);
    }

    void Update() {
        // Debounce Switches
        m_btn.Debounce();
        m_trigger.Debounce();

        // Update AnalogControls
        m_freqCtrl.Process();
        m_decayCtrl.Process();
        m_velocityCtrl.Process();
        m_fmCtrl.Process();
        m_toneCtrl.Process();

        // Check for rising/falling edges on digital inputs
        if (m_btn.RisingEdge() || m_trigger.RisingEdge())
        {
            m_HandleTrigger();
        }
    }

    float Process() {
        float oscAmp, oscOut, sigOut;

        m_osc.SetFreq(m_pitchEnv.Process());

        oscAmp = m_ampEnv.Process();
        m_osc.SetAmp(oscAmp);
        led1.Set(oscAmp);

        oscOut = m_osc.Process();
        m_svf.Process(oscOut);
        sigOut = m_svf.Low();

        return sigOut;
    }
};

class SnareDrum {
private:
    // hardware objects
    Switch m_trigger, m_btn;
    AnalogControl m_freqCtrl, m_decayCtrl, m_velocityCtrl, m_blendCtrl, m_toneCtrl;

    // Audio objects
    // TODO: Add noise generator
    Oscillator m_osc;
    AdEnv m_ampEnv, m_pitchEnv;
    Svf m_svf;

    // Member variables
    float
        m_freqMin,
        m_freqMax,
        m_fmAmount,
        m_decayMin,
        m_decayMax,
        m_velMin,
        m_velMax,
        m_cutoffMin,
        m_cutoffMax;

    // Functions
    void m_HandleTrigger() {
        // Read AnalogControl values
        float freqCV = m_freqCtrl.Value();
        float decayCV = m_decayCtrl.Value();
        float velocityCV = m_velocityCtrl.Value();
        float blendCV = m_blendCtrl.Value();
        float toneCV = m_toneCtrl.Value();

        // Set the velocity (amplitude envelope max)
        m_ampEnv.SetMax(m_velMin + ((m_velMax - m_velMin) * velocityCV));

        // Set the volume decay time
        float decayTime = (decayCV * (m_decayMax - m_decayMin)) + m_decayMin;
        m_ampEnv.SetTime(ADENV_SEG_DECAY, decayTime);

        // Calculate min and max freqencies for the pitch envelope
        float freqMin = m_freqMin + (freqCV * (m_freqMax - m_freqMin));
        float freqMax = freqMin + m_fmAmount;

        // Set min and max frequency for the pitch envelope
        m_pitchEnv.SetMin(freqMin);
        m_pitchEnv.SetMax(freqMax);

        // Trigger the envelopes
        m_ampEnv.Trigger();
        m_pitchEnv.Trigger();

        // Set the LPF cutoff freqency
        m_svf.SetFreq(m_cutoffMin + (toneCV * (m_cutoffMax - m_cutoffMin)));
    }

public:
    // TODO: Add setter methods for min/max values

    void Init(
        float samplerate,
        Pin triggerPin,
        Pin btnPin,
        int freqAdcIndex,
        int decayAdcIndex,
        int velocityAdcIndex,
        int blendAdcIndex,
        int toneAdcIndex
    ) {
        // Initialize AnalogControls
        m_freqCtrl.Init(hw.adc.GetPtr(freqAdcIndex), samplerate, true);
        m_decayCtrl.Init(hw.adc.GetPtr(decayAdcIndex), samplerate, true);
        m_velocityCtrl.Init(hw.adc.GetPtr(velocityAdcIndex), samplerate, true);
        m_blendCtrl.Init(hw.adc.GetPtr(blendAdcIndex), samplerate, true);
        m_toneCtrl.Init(hw.adc.GetPtr(toneAdcIndex), samplerate);

        // Define minimum/maximum control values
        m_freqMin   = 100.f;
        m_freqMax   = 350.f;
        m_decayMin  = 0.25f;
        m_decayMax  = 10.f;
        m_fmAmount  = 100.f;
        m_velMin   = 0.3f;
        m_velMax   = 1.f;
        m_cutoffMin = 40.f;
        m_cutoffMax = 400.f;

        // Initialize trigger and button inputs
        m_btn.Init(btnPin, samplerate / 48.f);
        m_trigger.Init(
            triggerPin,
            samplerate / 48.f,
            Switch::TYPE_MOMENTARY,
            Switch::POLARITY_INVERTED,
            Switch::PULL_DOWN
        );

        // Initialize oscillator
        m_osc.Init(samplerate);
        // m_osc.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
        m_osc.SetAmp(1);

        // Initizialize amplitude envelope
        // TODO: Try different curves
        m_ampEnv.Init(samplerate);
        m_ampEnv.SetTime(ADENV_SEG_ATTACK, .01);
        m_ampEnv.SetMax(1);
        m_ampEnv.SetMin(0);
        m_ampEnv.SetCurve(-20.f);

        // Initizialize pitch envelope
        // TODO: Try different curves
        m_pitchEnv.Init(samplerate);
        m_pitchEnv.SetTime(ADENV_SEG_ATTACK, .01);
        m_pitchEnv.SetTime(ADENV_SEG_DECAY, .05);

        // Initialize low pass filter
        m_svf.Init(samplerate);
        m_svf.SetDrive(0.1f);
        m_svf.SetRes(0.1f);
    }

    void Update() {
        // Debounce Switches
        m_btn.Debounce();
        m_trigger.Debounce();

        // Update AnalogControls
        m_freqCtrl.Process();
        m_decayCtrl.Process();
        m_velocityCtrl.Process();
        m_blendCtrl.Process();
        m_toneCtrl.Process();

        // Check for rising/falling edges on digital inputs
        if (m_btn.RisingEdge() || m_trigger.RisingEdge())
        {
            m_HandleTrigger();
        }
    }

    float Process() {
        float oscAmp, oscOut, sigOut;

        m_osc.SetFreq(m_pitchEnv.Process());

        oscAmp = m_ampEnv.Process();
        m_osc.SetAmp(oscAmp);
        led2.Set(oscAmp);
        sigOut = m_osc.Process();

        return sigOut;
    }
};

// -------------------------------------------------------------
//	Sound Generator Objects
// -------------------------------------------------------------

BassDrum kick;
SnareDrum snare;

// -------------------------------------------------------------
// Audio Callback
// -------------------------------------------------------------

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    kick.Update();
    snare.Update();

    float sigOut;
    for(size_t i = 0; i < size; i += 2)
    {
        led1.Update();
        led2.Update();
        sigOut = (kick.Process() / 2) + (snare.Process() / 2);
        out[i] = out[i + 1] = sigOut;
    }
}

// -------------------------------------------------------------
// Setup
// -------------------------------------------------------------

int main(void)
{
    // Configure and Initialize the Daisy Seed
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float samplerate = hw.AudioSampleRate();

    // Initialize indicator Leds
    led1.Init(PIN_LED1, false, samplerate);
    led1.Set(0.f);
    led2.Init(PIN_LED2, false, samplerate);
    led2.Set(0.f);

    // Configure and initialize ADCs
    AdcChannelConfig adcConfig[NUM_ADC_CHANNELS];

    adcConfig[KICK_FREQ_INDEX].InitSingle(PIN_KICK_FREQ);
    adcConfig[KICK_DECAY_INDEX].InitSingle(PIN_KICK_DECAY);
    adcConfig[KICK_VEL_INDEX].InitSingle(PIN_KICK_VEL);
    adcConfig[KICK_FM_INDEX].InitSingle(PIN_KICK_FM);
    adcConfig[KICK_TONE_INDEX].InitSingle(PIN_KICK_TONE);

	adcConfig[SNARE_FREQ_INDEX].InitSingle(PIN_SNARE_FREQ);
    adcConfig[SNARE_DECAY_INDEX].InitSingle(PIN_SNARE_DECAY);
    adcConfig[SNARE_VEL_INDEX].InitSingle(PIN_SNARE_VEL);
    adcConfig[SNARE_BLEND_INDEX].InitSingle(PIN_SNARE_BLEND);
    adcConfig[SNARE_TONE_INDEX].InitSingle(PIN_SNARE_TONE);

    hw.adc.Init(adcConfig, NUM_ADC_CHANNELS);
    hw.adc.Start();

    // Initialize the BassDrum object
    kick.Init(
        samplerate,
        PIN_KICK_TRIG,
        PIN_BTN1,
        KICK_FREQ_INDEX,
        KICK_DECAY_INDEX,
        KICK_VEL_INDEX,
        KICK_FM_INDEX,
        KICK_TONE_INDEX
    );

	// Initialize the SnareDrum object
    snare.Init(
        samplerate,
        PIN_SNARE_TRIG,
        PIN_BTN2,
        SNARE_FREQ_INDEX,
        SNARE_DECAY_INDEX,
        SNARE_VEL_INDEX,
        SNARE_BLEND_INDEX,
        SNARE_TONE_INDEX
    );

    // Start the callback function
    hw.StartAudio(AudioCallback);

	// Loop forever
    for(;;) {}
}