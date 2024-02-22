# DrumDrum

## Author

KG Polte

## Description

A kick drum generator for the Electrosmith Daisy Seed based on analog
synthesis techniques, using an oscillator with envelopes modulating 
pitch and amplitude.

This project used the Drum.cpp from the DaisyExamples repo as a
starting point with the intention of adding as much control as possible.

This project is still in the very early stages. I ultimately plan to 
build this into a Eurorack module.

## Needed changes/additions:

-I am working on a schematic for the actual Eurorack module, including
input/output scaling and overvoltage protection for the Seed's ADCs.
I will add the KiCAD files tothis repo once I have verified everything.

-I'm actively updating the CV inputs on the hardware side, so the code
here is actively being updated to reflect these changes. Most notably,
I'm making the inputs bipolar (+/- 5V), so the controls will set the
midpoint of the modulation instead of the base as I had it before.

-Add variable waveshape (Sin/Tri).

-Most pressing issue - adding a trigger input in addition to the button.
Should have this resolved soon. Just need to verify on the breadboard 
before starting on the code.