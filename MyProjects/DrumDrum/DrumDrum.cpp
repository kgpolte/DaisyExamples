#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

DaisySeed hw;
Led led1, led2;

// Hardware pin definitions
constexpr Pin PIN_TRIG      = D13;
constexpr Pin PIN_BTN1      = D2;
constexpr Pin PIN_BTN2      = D3;
constexpr Pin PIN_LED1      = D26;
constexpr Pin PIN_LED2      = D27;
constexpr Pin PIN_FREQ      = A0;
constexpr Pin PIN_DECAY     = A1;
constexpr Pin PIN_VELOCITY  = A2;
constexpr Pin PIN_FM        = A3;
constexpr Pin PIN_DRIVE     = A4;
constexpr Pin PIN_TONE      = A5;

// ADC indeces
int FREQ_INDEX      = 0;
int DECAY_INDEX     = 1;
int VELOCITY_INDEX  = 2;
int FM_INDEX        = 3;
int DRIVE_INDEX     = 4;
int TONE_INDEX      = 5;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

class BassDrum {
private:
    // hardware objects
    Switch m_trigger, m_btn;
    AnalogControl m_freqCtrl, m_decayCtrl, m_velocityCtrl, m_driveCtrl, m_fmCtrl, m_toneCtrl;

    // Audio objects
    Oscillator m_osc;
    AdEnv m_ampEnv, m_pitchEnv;
    Overdrive m_od;
    Svf m_svf;

    // Variables
    float m_FreqMin, m_FreqMax, m_FmMin, m_FmMax, m_DecayMin, m_DecayMax, m_DriveMin, m_DriveMax, m_cutoffMin, m_cutoffMax;

    // Functions
    void m_HandleTrigger() {
        // Read AnalogControl values
        float freqCV = m_freqCtrl.Value();
        float decayCV = m_decayCtrl.Value();
        float velocityCV = m_velocityCtrl.Value();
        float fmCV = m_fmCtrl.Value();
        float driveCV = m_driveCtrl.Value();
        float toneCV = m_toneCtrl.Value();

        // Set the volume decay time
        float decayTime = (decayCV * (m_DecayMax - m_DecayMin)) + m_DecayMin;
        m_ampEnv.SetTime(ADENV_SEG_DECAY, decayTime);

        // Calculate min and max freqencies for the pitch envelope
        float freqMin = m_FreqMin + (freqCV * (m_FreqMax - m_FreqMin));
        float fmAmt = m_FmMin + (fmCV * (m_FmMax - m_FmMin));
        float freqMax = freqMin + fmAmt;

        // Set min and max frequency for the pitch envelope
        m_pitchEnv.SetMin(freqMin);
        m_pitchEnv.SetMax(freqMax);

        // Trigger the envelopes
        m_ampEnv.Trigger();
        m_pitchEnv.Trigger();

        // Set the overdrive amount
        m_od.SetDrive(m_DriveMin + (driveCV * (m_DriveMax - m_DriveMin)));

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
        int driveAdcIndex,
        int toneAdcIndex
    ) {
        // Initialize AnalogControls
        m_freqCtrl.Init(hw.adc.GetPtr(freqAdcIndex), samplerate, true);
        m_decayCtrl.Init(hw.adc.GetPtr(decayAdcIndex), samplerate, true);
        m_velocityCtrl.Init(hw.adc.GetPtr(velocityAdcIndex), samplerate, true);
        m_fmCtrl.Init(hw.adc.GetPtr(fmAdcIndex), samplerate, true);
        m_driveCtrl.Init(hw.adc.GetPtr(driveAdcIndex), samplerate);
        m_toneCtrl.Init(hw.adc.GetPtr(toneAdcIndex), samplerate);

        // Set minimum/maximum control values
        m_FreqMin = 40.f;
        m_FreqMax = 100.f;
        m_DecayMin = 0.25f;
        m_DecayMax = 10.f;
        m_FmMin = 50.f;
        m_FmMax = 250.f;
        m_DriveMin = 0.2f;
        m_DriveMax = 0.5f;
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

        // Initizialize amplitdue envelope
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

        // Initialize overdrive and filter
        m_od.Init();

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
        m_driveCtrl.Process();
        m_toneCtrl.Process();

        // Check for rising/falling edges on digital inputs
        if (m_btn.RisingEdge() || m_trigger.RisingEdge())
        {
            m_HandleTrigger();
        }
    }

    float Process() {
        m_osc.SetFreq(m_pitchEnv.Process());
        float osc_amp = m_ampEnv.Process();
        m_osc.SetAmp(osc_amp);
        led1.Set(osc_amp);

        float oscOut = m_osc.Process();

        // TODO: Find a better way to mix in overdrive
        float odOut = m_od.Process(oscOut / 2);
        
        m_svf.Process(odOut);
        float sig = m_svf.Low();

        return sig;
    }
};

// -------------------------------------------------------------
//
// -------------------------------------------------------------

BassDrum drum;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    drum.Update();

    float sig;
    for(size_t i = 0; i < size; i += 2)
    {
        led1.Update();
        led2.Update();
        sig = drum.Process();
        out[i] = out[i + 1] = sig;
    }
}

// -------------------------------------------------------------
//
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
    led2.Set(1.f);

    // Configure and initialize ADCs
    AdcChannelConfig adcConfig[5];
    adcConfig[FREQ_INDEX].InitSingle(PIN_FREQ);
    adcConfig[DECAY_INDEX].InitSingle(PIN_DECAY);
    adcConfig[VELOCITY_INDEX].InitSingle(PIN_VELOCITY);
    adcConfig[FM_INDEX].InitSingle(PIN_FM);
    adcConfig[DRIVE_INDEX].InitSingle(PIN_DRIVE);
    adcConfig[TONE_INDEX].InitSingle(PIN_TONE);
    hw.adc.Init(adcConfig, 6);
    hw.adc.Start();

    // Initialize BassDrum
    drum.Init(
        samplerate,
        PIN_TRIG,
        PIN_BTN1,
        FREQ_INDEX,
        DECAY_INDEX,
        VELOCITY_INDEX,
        FM_INDEX,
        DRIVE_INDEX,
        TONE_INDEX
    );

    // Start serial logging
    // hw.StartLog(true);
    // hw.PrintLine("Daisy ready...");

    //Start calling the callback function
    hw.StartAudio(AudioCallback);

    for(;;) {}
}