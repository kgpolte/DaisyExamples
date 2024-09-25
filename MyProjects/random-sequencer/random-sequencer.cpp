#include "daisy_seed.h"

// Use the daisy namespace to prevent having to type
// daisy:: before all libdaisy functions
using namespace daisy;

// Declare a DaisySeed object called hardware
DaisySeed hardware;

int main(void)
{
    // Configure and Initialize the Daisy Seed
    // These are separate to allow reconfiguration of any of the internal
    // components before initialization.
    hardware.Configure();
    hardware.Init();

    const int num_leds = 8;
    Led leds[num_leds];
    const int led_pins[num_leds] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (int i = 0; i < num_leds; i++) {
        leds[i].Init(hardware.GetPin(led_pins[i]), false);
    }

    // Loop forever
    for(;;)
    {
        // Wait 500ms
        System::Delay(500);
    }
}
