# MAX2870
Arduino Library for the MAX2870 Wideband Frequency Synthesizer chip

v1.0.0 First release

v1.0.1 Double buffering of RF frequency divider implemented by default

v1.1.0 Added current frequency read function

v1.1.1 Corrected issue with conversion in ReadCurrentFreq

v1.1.2 Add setPowerLevel function which can be used for frequency bursts

v1.1.3 Added direct entry of frequency parameters for precalculated frequencies of the highest possible precision

v1.1.4 Added configuration of charge pump current and phase detector polarity

## Introduction

This library supports the MAX2870 from Maxim on Arduinos. The chip is a wideband (23.475 MHz to 6 GHz) Phase-Locked Loop (PLL) and Voltage Controlled Oscillator (VCO), covering a very wide range frequency range
under digital control. Just add an external PLL loop filter, Reference frequency source and a power supply for a very useful 
frequency generator for applications as a Local Oscillator or Sweep Generator.  

The chip generates the frequency using a programmable Fractional-N and Integer-N Phase-Locked Loop (PLL) and Voltage Controlled Oscillator (VCO) with an external loop filter and frequency reference. The chip is controlled by 
a SPI interface, which is controlled by a microcontroller such as the Arduino.

The library provides an SPI control interface for the MAX2870, and also provides functions to calculate and set the
frequency, which greatly simplifies the integration of this chip into a design. The calculations are done using the excellent 
[Big Number Arduino Library](https://github.com/nickgammon/BigNumber) by Nick Gammon, as the integter calculations require
great than 32bit integers that are not available on the Arduino. The library also exposes all of the PLL variables, such as FRAC, Mod and INT, so they examined as needed.  

Requires the BitFieldManipulation library: http://github.com/brycecherry75/BitFieldManipulation

Requires the BeyondByte library: http://github.com/brycecherry75/BeyondByte

A low phase noise stable oscillator is required for this module. Typically, an Ovenized Crystal Oscillator (OCXO) in the 10 MHz to 100 MHz range is used.  

## Features

+ Frequency Range: 23.475 MHz to 6 GHz
+ Output Level: -4 dBm to 5 dBm (in 3 dB steps) 
+ In-Band Phase Noise: -95 dBc/Hz (10 kHz from 4.227 Ghz carrier)
+ PLL Modes: Fraction-N and Integer-N (set automatically)
+ Step Frequency: 1 kHz to 100 MHz  
+ Signal On/Off control
+ All MAX2870_R[] registers can be accessed and manipulated

## Library Use

The library is documented in the [docs directory](doc/html/), and was created using Doxygen. 

An example program using the library is provided in the source directory [example2870.ino](src/example2870.ino).

init(SSpin, LockPinNumber, Lock_Pin_Used, CEpin, CE_Pin_Used): initialize the MAX2870 with SPI SS pin, lock pin and true/false for lock pin use and CE pin use - CE pin is typically LOW (disabled) on reset if used; depending on your board, this pin along with the RF Power Down pin may have a pullup or pulldown resistor fitted and certain boards have the RF Power Down pin (low active) on the header

SetStepFreq(frequency): sets the step frequency in Hz - default is 100 kHz - returns an error code

ReadR()/ReadInt()/ReadFraction()/ReadMod(): returns a uint16_t value for the currently programmed register

ReadOutDivider()/ReadOutDivider_PowerOf2()/ReadRDIV2()/ReadRefDoubler(): returns a uint8_t value for the currently programmed register - ReadOutDivider() is automatically converted from a binary exponent to an actual division ratio and 
ReadOutDivider_PowerOf2() is a binary exponent

ReadPFDfreq(): returns a double for the PFD value

setf(*frequency, PowerLevel, AuxPowerLevel, AuxFrequencyDivider, PrecisionFrequency, FrequencyTolerance, CalculationTimeout): set the frequency (in Hz with char string) power level/auxiliary power level (1-4 in 3dBm steps from -5dBm), mode for auxiliary frequency output (MAX2870_AUX_(DIVIDED/FUNDAMENTAL)), true/false for precision frequency mode (step size is ignored if true), frequency tolerance (in Hz with uint32_t) under precision frequency mode (rounded to the nearest integer), calculation timeout (in mS with uint32_t - recommended value is 30000 in most cases, 0 to disable) under precision frequency mode - returns an error or warning code

setrf(frequency, R_divider, ReferenceDivisionType): set the reference frequency and reference divider R and reference frequency division type (MAX2870_REF_(UNDIVIDED/HALF/DOUBLE)) - default is 10 MHz/1/undivided - returns an error code

setfDirect(R_divider, INT_value, MOD_value, FRAC_value, RF_DIVIDER_value, FRACTIONAL_MODE): RF divider value is (1/2/4/8/16/32/64) and fractional mode is a true/false bool - these paramaters will not be checked for invalid values

setPowerLevel/setAuxPowerLevel(PowerLevel): set the power level (0 to disable or 1-4) and write to the MAX2870 in one operation - returns an error code

WriteSweepRegs(*regs): high speed write for registers when used for frequency sweep (*regs is uint32_t and size is as per MAX2870_RegsToWrite

ReadSweepRegs(*regs): high speed read for registers when used for frequency sweep (*regs is uint32_t and size is as per MAX2870_RegsToWrite

ReadCurrentFreq(*freq): calculation of currently programmed frequency (*freq is uint8_t and size is as per MAX2870_ReadCurrentFrequency_ArraySize)

setCPcurrent(Current): set charge pump current in mA floating

setPDpolarity(INVERTING/NONINVERTING): set phase detector polarity for your VCO loop filter

A Python script (MAX2870pf.py) can be used for calculating the required values for setfDirect for speed.

Please note that you should install the provided BigNumber library in your Arduino library directory.

Under non-precision mode, unusual step frequencies e.g. VCO (RF frequency * divider) = 1500.00353 MHz and PFD = 10 MHz (REFIN / R) should be avoided along with a PFD having decimal place(s) to avoid slow GCD calculations for FRAC/MOD and a subsequent FRAC/MOD range error - step sizes for a 10 MHz PFD which do not have this issue are any multiple of 2500/3125/4000 Hz as per this formula (all frequencies are in Hz):

MOD = PFD / step size

FRAC = (N remainder * MOD)

factor = GCD(MOD, FRAC)

MOD = MOD / factor

FRAC = FRAC / factor

At all stages, MOD and FRAC are rounded down to the nearest integer.

Under worst possible conditions (tested with 3.999997551 GHz RF/10 MHz PFD/0 Hz tolerance target error which will go through the entire permissible range of MOD values) on a 16 MHz AVR Arduino, precision frequency mode configuration takes no longer than 45 seconds.

Default settings which may need to be changed as required BEFORE execution of MAX2870 library functions (defaults listed):

Phase Detector Polarity (Register 2/Bit 6 = 1): Positive (passive or noninverting active loop filter)

Error codes:

Common to all of the following subroutines:

MAX2870_ERROR_NONE


SetStepFreq:

MAX2870_ERROR_STEP_FREQUENCY_EXCEEDS_PFD


setf:

MAX2870_ERROR_RF_FREQUENCY

MAX2870_ERROR_POWER_LEVEL

MAX2870_ERROR_AUX_POWER_LEVEL

MAX2870_ERROR_AUX_FREQ_DIVIDER

MAX2870_ERROR_ZERO_PFD_FREQUENCY

MAX2870_ERROR_MOD_RANGE

MAX2870_ERROR_FRAC_RANGE

MAX2870_ERROR_N_RANGE

MAX2870_ERROR_RF_FREQUENCY_AND_STEP_FREQUENCY_HAS_REMAINDER

MAX2870_ERROR_PFD_EXCEEDED_WITH_FRACTIONAL_MODE

MAX2870_ERROR_PRECISION_FREQUENCY_CALCULATION_TIMEOUT


setrf:

MAX2870_ERROR_DOUBLER_EXCEEDED

MAX2870_ERROR_R_RANGE

MAX2870_ERROR_REF_FREQUENCY

MAX2870_ERROR_REF_MULTIPLIER_TYPE


setf and setrf:

MAX2870_ERROR_PFD_AND_STEP_FREQUENCY_HAS_REMAINDER

MAX2870_ERROR_PFD_LIMITS

setPDpolarity:

MAX2870_ERROR_POLARITY_INVALID

Warning codes:

setf:

MAX2870_WARNING_FREQUENCY_ERROR

## Installation
Copy the `src/` directory to your Arduino sketchbook directory  (named the directory `example2870`), and install the libraries in your Arduino library directory.  You can also install the MAX2870 files separatly as a library.

## References

+ [Big Number Arduino Library](https://github.com/nickgammon/BigNumber) by Nick Gammon
+ Maxim MAX2870 datasheet
