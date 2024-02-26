#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisy::seed;
using namespace daisysp;

DaisySeed hw;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

class BassDrum {
private:
    Pin m_freqPin, m_decayPin, m_fmPin, m_drivePin;
    Switch m_trigger, m_btn;
    Oscillator m_osc;
    AdEnv m_ampEnv, m_pitchEnv;
    Overdrive m_od;

    int m_freqAdc, m_decayAdc, m_fmAdc, m_driveAdc;
    float m_FreqMin, m_FreqMax, m_FmMin, m_FmMax, m_DecayMin, m_DecayMax, m_DriveMin, m_DriveMax;

    void HandleTrigger() {
        hw.SetLed(true);

        // Read ADC inputs
        float freqCV = hw.adc.GetFloat(m_freqAdc);
        float decayCV = (hw.adc.GetFloat(m_decayAdc) * (m_DecayMax - m_DecayMin)) + m_DecayMin;
        float fmCV = hw.adc.GetFloat(m_fmAdc);
        float driveCV = hw.adc.GetFloat(m_driveAdc);

        // Set the volume decay time
        m_ampEnv.SetTime(ADENV_SEG_DECAY, decayCV);

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
        int driveAdc = 3
    ) {
        // Set Adc indexes
        m_freqAdc = freqAdc;
        m_decayAdc = decayAdc;
        m_fmAdc = fmAdc;
        m_driveAdc = driveAdc;

        // Set minimum/maximum control values
        m_FreqMin = 40;
        m_FreqMax = 100;
        m_DecayMin = 0.2;
        m_DecayMax = 10;
        m_FmMin = 50;
        m_FmMax = 250;
        m_DriveMin = 0.2;
        m_DriveMax = 0.8;

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

        // Initialize overdrive
        m_od.Init();
    }

    void Update() {
        m_btn.Debounce();
        m_trigger.Debounce();

        if (m_btn.RisingEdge() || m_trigger.RisingEdge())
        {
            HandleTrigger();
        } else if (m_btn.FallingEdge() || m_trigger.FallingEdge()) {
            hw.SetLed(false);
        }
    }

    float Process() {
        m_osc.SetFreq(m_pitchEnv.Process());
        m_osc.SetAmp(m_ampEnv.Process());
        float oscOut = m_osc.Process();
        float sig = m_od.Process(oscOut);
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
    float sig;

    drum.Update();

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

    drum.Init(samplerate);

    // Configure ADCs
    AdcChannelConfig adcConfig[4];
    adcConfig[0].InitSingle(A0);      // Freqency
    adcConfig[1].InitSingle(A1);      // Decay
    adcConfig[2].InitSingle(A2);      // FM
    adcConfig[3].InitSingle(A3);      // Drive
    hw.adc.Init(adcConfig, 4);
    hw.adc.Start();

    // Start serial logging
    hw.StartLog(true);
    hw.PrintLine("Daisy Ready");

    //Start calling the callback function
    hw.StartAudio(AudioCallback);

    for(;;) {}
}