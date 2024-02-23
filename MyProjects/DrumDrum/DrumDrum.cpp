#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

DaisySeed hw;

Oscillator osc_low;
WhiteNoise noise;

AdEnv kickVolEnv, kickPitchEnv, snareEnv;

Switch kickTrig, snareTrig;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

#define FREQ_PIN 			15  // Seed Pin 22
#define DECAY_PIN 			16  // Seed Pin 23
#define FM_PIN				17  // Seed Pin 24
#define DRIVE_PIN			18  // Seed Pin 25
#define FREQ_CV_PIN			24  // Seed Pin 31
#define DECAY_CV_PIN		25  // Seed Pin 32
#define KICK_TRIG_PIN		7   // Seed Pin 8
#define SNARE_TRIG_PIN		8   // Seed Pin 9
#define BTN_PIN             9  // Seed Pin 10

// -------------------------------------------------------------
//
// -------------------------------------------------------------

// Remember to update this when adding/removing ranged controls
int numAdcChannels = 5;

// Kick drum variables
float kickFreqMin = 40;
float kickFreqMax = 100;
float kickMinFm = 50;
float kickMaxFm = 300;
float kickMinDecay = 0.2;
float kickMaxDecay = 10;

// -------------------------------------------------------------
//
// -------------------------------------------------------------

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    float osc_low_out, noise_out, snr_env_out, kck_env_out, kick_sig;

    //Get rid of any bouncing
    snareTrig.Debounce();
    kickTrig.Debounce();

    if(kickTrig.RisingEdge())
    {
		// Read ADC inputs
		float freqPot = hw.adc.GetFloat(0);
		float fmPot = hw.adc.GetFloat(2);
		float freqCV = hw.adc.GetFloat(4);
		float decayCV = (hw.adc.GetFloat(5) * (kickMaxDecay - kickMinDecay)) + kickMinDecay;
		float decayManual = (hw.adc.GetFloat(1) * (kickMaxDecay - kickMinDecay)) + kickMinDecay;

		kickVolEnv.SetTime(ADENV_SEG_DECAY, decayManual);

		//Calculate min and max freqencies for the pitch envelope
		float freqMin = kickFreqMin + (freqPot * (kickFreqMax - kickFreqMin));
		float fmAmt = kickMinFm + (fmPot * (kickMaxFm - kickMinFm));
		float freqMax = freqMin + fmAmt;

		// Set min and max frequency for the pitch envelope
		kickPitchEnv.SetMin(freqMin);
		kickPitchEnv.SetMax(freqMax);

        // Trigger the envelopes
        kickVolEnv.Trigger();
        kickPitchEnv.Trigger();
    }

    if(snareTrig.RisingEdge())
    {
        snareEnv.Trigger();
    }

    //Prepare the audio block
    for(size_t i = 0; i < size; i += 2)
    {
        //Get the next volume samples
        snr_env_out = snareEnv.Process();
        kck_env_out = kickVolEnv.Process();

        //Apply the pitch envelope to the kickTrig
        osc_low.SetFreq(kickPitchEnv.Process());
        //Set the kickTrig volume to the envelope's output
        osc_low.SetAmp(kck_env_out);
        //Process the next oscillator sample
        osc_low_out = osc_low.Process();

        //Get the next snare sample
        noise_out = noise.Process();
        //Set the sample to the correct volume
        noise_out *= snr_env_out;

		//Set both audio outputs to the kickTrig signal for now
        kick_sig = osc_low_out;
        out[i] = out[i + 1] = kick_sig;
    }
}

// -------------------------------------------------------------
//
// -------------------------------------------------------------

int main(void)
{
    // Configure and Initialize the Daisy Seed
    // These are separate to allow reconfiguration of any of the internal
    // components before initialization.
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float samplerate = hw.AudioSampleRate();

    //Initialize oscillator for kickdrum
    osc_low.Init(samplerate);
	osc_low.SetWaveform(Oscillator::WAVE_SIN);
    osc_low.SetAmp(1);

    //Initialize noise
    noise.Init();

    //Initialize envelopes, this one's for the snare amplitude
    snareEnv.Init(samplerate);
    snareEnv.SetTime(ADENV_SEG_ATTACK, .01);
    snareEnv.SetTime(ADENV_SEG_DECAY, .2);
    snareEnv.SetMax(1);
    snareEnv.SetMin(0);

    //This envelope will control the kickTrig oscillator's pitch
    //Note that this envelope is much faster than the volume
    kickPitchEnv.Init(samplerate);
    kickPitchEnv.SetTime(ADENV_SEG_ATTACK, .01);
    kickPitchEnv.SetTime(ADENV_SEG_DECAY, .05);

    //This one will control the kickTrig's volume
    kickVolEnv.Init(samplerate);
    kickVolEnv.SetTime(ADENV_SEG_ATTACK, .01);
    kickVolEnv.SetMax(1);
    kickVolEnv.SetMin(0);
	kickVolEnv.SetCurve(-20.f);

    //Initialize the kickTrig and snare buttons
    //The callback rate is samplerate / blocksize (48)
    snareTrig.Init(hw.GetPin(SNARE_TRIG_PIN), samplerate / 48.f);
    kickTrig.Init(hw.GetPin(KICK_TRIG_PIN), samplerate / 48.f);

	// Initialize ADCs
	AdcChannelConfig adcConfig[numAdcChannels];
	// Potentiometers
	adcConfig[0].InitSingle(hw.GetPin(FREQ_PIN));
	adcConfig[1].InitSingle(hw.GetPin(DECAY_PIN));
	adcConfig[2].InitSingle(hw.GetPin(FM_PIN));
	adcConfig[3].InitSingle(hw.GetPin(DRIVE_PIN));
	// CV Inputs
	adcConfig[4].InitSingle(hw.GetPin(FREQ_CV_PIN));
	adcConfig[5].InitSingle(hw.GetPin(DECAY_CV_PIN));
	// Initialize the hw ADC
	hw.adc.Init(adcConfig, numAdcChannels);
	hw.adc.Start();

    //Start calling the callback function
    hw.StartAudio(AudioCallback);

    // Loop forever
    for(;;) {}
}