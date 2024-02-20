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
Everything is working nicely on the breadboard with 4 potentiometers
and two CV inputs which expect 0-10V. I will add the KiCAD files to
this repo once I have verified everything.

-I still need to get the CV input handling correct in the code - I've run into
some strange issues that I have yet to understand, but all of the other 
controls are currently working.

-Add variable waveshape (Sin/Tri).

-Most pressing issue - adding a trigger input in addition to the button.
Should have this resolved soon. Just need to verify on the breadboard 
before starting on the code.