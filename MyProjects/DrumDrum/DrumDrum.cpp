#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

DaisySeed hw;

// Hardware definitions
constexpr Pin PIN_TRIG      = D7;
constexpr Pin PIN_BTN       = D9;
constexpr Pin PIN_FREQ      = A0;
constexpr Pin PIN_DECAY     = A1;
constexpr Pin PIN_FM        = A2;
constexpr Pin PIN_DRIVE     = A3;
constexpr Pin PIN_TONE      = A4;

// ADC indeces
int FREQ_INDEX      = 0;
int DECAY_INDEX     = 1;
int FM_INDEX        = 2;
int DRIVE_INDEX     = 3;
int TONE_INDEX      = 4;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

class BassDrum {
private:
    // hardware objects
    Pin m_freqPin, m_decayPin, m_fmPin, m_drivePin;
    Switch m_trigger, m_btn;
    AnalogControl m_freqCtrl, m_decayCtrl, m_fmCtrl, m_driveCtrl, m_toneCtrl;

    // Audio objects
    Oscillator m_osc;
    AdEnv m_ampEnv, m_pitchEnv;
    Overdrive m_od;
    Svf m_svf;

    // Variables
    float m_FreqMin, m_FreqMax, m_FmMin, m_FmMax, m_DecayMin, m_DecayMax, m_DriveMin, m_DriveMax, m_cutoffMin, m_cutoffMax;

    // Functions
    void m_HandleTrigger() {
        hw.SetLed(true);

        // Read AnalogControl values
        float freqCV = m_freqCtrl.Value();
        float decayCV = m_decayCtrl.Value();
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
    }

public:

    // TODO: Add setter methods for min/max values

    void Init(
        float samplerate,
        Pin triggerPin = D7,
        Pin btnPin = D9,
        int freqAdc = 0,
        int decayAdc = 1,
        int fmAdc = 2,
        int driveAdc = 3,
        int toneAdc = 4
    ) {
        // Initialize AnalogControls
        m_freqCtrl.Init(hw.adc.GetPtr(FREQ_INDEX), samplerate);
        m_decayCtrl.Init(hw.adc.GetPtr(DECAY_INDEX), samplerate);
        m_fmCtrl.Init(hw.adc.GetPtr(FM_INDEX), samplerate);
        m_driveCtrl.Init(hw.adc.GetPtr(DRIVE_INDEX), samplerate);
        m_toneCtrl.Init(hw.adc.GetPtr(TONE_INDEX), samplerate);

        // Set minimum/maximum control values
        m_FreqMin = 40.f;
        m_FreqMax = 100.f;
        m_DecayMin = 0.2f;
        m_DecayMax = 10.f;
        m_FmMin = 50.f;
        m_FmMax = 250.f;
        m_DriveMin = 0.25f;
        m_DriveMax = 0.6f;
        m_cutoffMin = 40.f;
        m_cutoffMax = 4000.f;

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
        m_svf.Init(samplerate);
    }

    void Update() {
        // Debounce Switches
        m_btn.Debounce();
        m_trigger.Debounce();

        // Update AnalogControls
        m_freqCtrl.Process();
        m_decayCtrl.Process();
        m_fmCtrl.Process();
        m_driveCtrl.Process();
        m_toneCtrl.Process();

        // Check for rising/falling edges on digital inputs
        if (m_btn.RisingEdge() || m_trigger.RisingEdge())
        {
            m_HandleTrigger();
        } else if (m_btn.FallingEdge() || m_trigger.FallingEdge()) {
            hw.SetLed(false);
        }
    }

    float Process() {
        m_osc.SetFreq(m_pitchEnv.Process());
        m_osc.SetAmp(m_ampEnv.Process());
        float oscOut = m_osc.Process();
        float odOut = m_od.Process(oscOut);

        float mix = (odOut / 2 + oscOut / 2);

        m_svf.SetFreq(300.f);
        m_svf.SetDrive(1.f);
        m_svf.SetRes(0.5f);
        m_svf.Process(mix);
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

    // Configure and initialize ADCs
    AdcChannelConfig adcConfig[5];
    adcConfig[FREQ_INDEX].InitSingle(PIN_FREQ);
    adcConfig[DECAY_INDEX].InitSingle(PIN_DECAY);
    adcConfig[FM_INDEX].InitSingle(PIN_FM);
    adcConfig[DRIVE_INDEX].InitSingle(PIN_DRIVE);
    adcConfig[TONE_INDEX].InitSingle(PIN_TONE);
    hw.adc.Init(adcConfig, 5);
    hw.adc.Start();

    // Initialize BassDrum
    drum.Init(
        samplerate,
        PIN_TRIG,
        PIN_BTN,
        FREQ_INDEX,
        DECAY_INDEX,
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