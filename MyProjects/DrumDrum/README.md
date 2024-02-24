# DrumDrum

## Author

KG Polte

## Description

A kick and snare drum generator for the Electrosmith Daisy Seed based on
analog synthesis techniques, using an oscillator with envelopes modulating 
pitch and amplitude.

This project used the Drum.cpp from the DaisyExamples repo as a
starting point with the intention of adding as much control as possible.

This project is still in the very early stages of prototyping. I ultimately
plan to build this into a Eurorack module.

## Needed changes/additions:

-I am working on a schematic for the actual Eurorack module, including
input/output scaling and overvoltage protection for the Seed's inputs.
I will add the KiCAD files to this repo once I have verified everything.

-Add snare functionality. I have not set up the trigger on the breadboard
yet to test, but that is coming soon. I will also add another oscillator
to mix into the snare signal.

-Add an overdrive stage to the kick drum.
