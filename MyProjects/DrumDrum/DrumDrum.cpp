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

#define FREQ_PIN 			19
#define DECAY_PIN 			20
#define FM_PIN				21
#define FALL_PIN			22
#define FREQ_CV_PIN			16
#define DECAY_CV_PIN		17
#define KICK_TRIG_PIN		15
#define SNARE_TRIG_PIN		16

// -------------------------------------------------------------
//
// -------------------------------------------------------------

int numAdcChannels = 5;

// Kick drum variables
float kickFreqMin = 40;
float kickFreqMax = 100;
float kickMinFm = 50;
float kickMaxFm = 300;
float kickMinFall = 0.01;
float kickMaxFall = 0.09;
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
		// Set the kick and pitch envelope attributes and trigger the kick
		kickVolEnv.SetTime(
			ADENV_SEG_DECAY,
			(hw.adc.GetFloat(1) * (kickMaxDecay - kickMinDecay)) + kickMinDecay
		);

		float freqMin = kickFreqMin + (hw.adc.GetFloat(0) * (kickFreqMax - kickFreqMin));
		kickPitchEnv.SetMin(freqMin);
		kickPitchEnv.SetMax(
			freqMin + (kickMinFm + (hw.adc.GetFloat(2) * (kickMaxFm - kickMinFm)))
		);
		kickPitchEnv.SetTime(
			ADENV_SEG_DECAY,
			kickMinFall + (hw.adc.GetFloat(3) * (kickMaxFall - kickMinFall))
		);

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

        //Get the next snareTrig sample
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

    //Initialize envelopes, this one's for the snareTrig amplitude
    snareEnv.Init(samplerate);
    snareEnv.SetTime(ADENV_SEG_ATTACK, .01);
    snareEnv.SetTime(ADENV_SEG_DECAY, .2);
    snareEnv.SetMax(1);
    snareEnv.SetMin(0);

    //This envelope will control the kickTrig oscillator's pitch
    //Note that this envelope is much faster than the volume
    kickPitchEnv.Init(samplerate);
    kickPitchEnv.SetTime(ADENV_SEG_ATTACK, .01);

    //This one will control the kickTrig's volume
    kickVolEnv.Init(samplerate);
    kickVolEnv.SetTime(ADENV_SEG_ATTACK, .01);
    kickVolEnv.SetMax(1);
    kickVolEnv.SetMin(0);
	kickVolEnv.SetCurve(-20.f);

    //Initialize the kickTrig and snareTrig buttons
    //The callback rate is samplerate / blocksize (48)
    snareTrig.Init(hw.GetPin(SNARE_TRIG_PIN), samplerate / 48.f);
    kickTrig.Init(hw.GetPin(KICK_TRIG_PIN), samplerate / 48.f);

	// Initialize ADCs
	AdcChannelConfig adcConfig[numAdcChannels];
	// Potentiometers
	adcConfig[0].InitSingle(hw.GetPin(FREQ_PIN));
	adcConfig[1].InitSingle(hw.GetPin(DECAY_PIN));
	adcConfig[2].InitSingle(hw.GetPin(FM_PIN));
	adcConfig[3].InitSingle(hw.GetPin(FALL_PIN));
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