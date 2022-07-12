# ServerPSUtoATX

Convert a server DC-DC converter with +3.3/+5V converters to a 12V input ATX PSU

## Revisions:

v1.0.0 First release

## Features

+ Diagnostic indication to indicate running or turned off or shutdown on undervoltage (LED flashes slowly) and overvoltage (LED flashes quickly)
+ Overvoltage lockout to prevent enabling of power until unit is disconnected from power source and the fault is rectified
+ Option to deal with motherboards which keep ATX PSU_ON asserted after this project disables all outputs except +5VSB on an undervoltage condition to avoid lockout if desired

## How to use

Instructions including required pullup resistors are included in the .ino sketch.

The circuit is in a PDF file and the schematic file is in Autodesk Eagle format.

## Circuit description

Typically, server power supplies have a high power +12V rail for distribution to DC-DC converters for localised supplies, and server power supplies along with high current DC-DC converter modules are good candidates for repurposing.

A Delta AC-106A (Dell part number 00G8CN) DC-DC converter board has +3.3V/22A and +5V/22A DC-DC converters which is a good candidate for conversion to a 12V input ATX power supply; this board also has undervoltage and overvoltage comparators for these rails with logic level outputs but if your board does not have them, external circuitry is required which is in the circuit diagram.

IC1/C1-C2/D1-D2 and Q3 form a charge pump converter which almost doubles the +12V input to provide sufficient gate drive for N-channel MOSFETs Q1 and Q2 which switch the +12V rail with R2/ZD1/D3 protecting the gates of Q1/Q2 against overvoltage; Q3 allows the charge pump converter to be completely disabled via IC2a when the unit is switched off via the ATX PSU On signal and IC2b additionally discharges the gate drive.

Provisions for undervoltage/overvoltage protection and reference voltage regulation are provided should your DC-DC converter board not include it for certain rails and diagnostic indication is also included as described in the code sketch.