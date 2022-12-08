/*!
   @file MAX2870.h

   This is part of the Arduino Library for the MAX2870 PLL wideband frequency synthesier

*/

#ifndef MAX2870_H
#define MAX2870_H
#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <BigNumber.h>
#include <BitFieldManipulation.h>
#include <BeyondByte.h>

#define MAX2870_PFD_MAX   105000000UL      ///< Maximum Frequency for Phase Detector (Integer-N)
#define MAX2870_PFD_MAX_FRAC   50000000UL  ///< Maximum Frequency for Phase Detector (Fractional-N)
#define MAX2870_PFD_MIN   125000UL        ///< Minimum Frequency for Phase Detector
#define MAX2870_REFIN_MAX   200000000UL   ///< Maximum Reference Frequency
#define MAX2870_REFIN_MIN   10000000UL   ///< Minimum Reference Frequency
#define MAX2870_REF_FREQ_DEFAULT 10000000UL  ///< Default Reference Frequency

#define MAX2870_AUX_DIVIDED 0
#define MAX2870_AUX_FUNDAMENTAL 1
#define MAX2870_REF_UNDIVIDED 0
#define MAX2870_REF_HALF 1
#define MAX2870_REF_DOUBLE 2

// common to all of the following subroutines
#define MAX2870_ERROR_NONE 0

// SetStepFreq
#define MAX2870_ERROR_STEP_FREQUENCY_EXCEEDS_PFD 1

// setf
#define MAX2870_ERROR_RF_FREQUENCY 2
#define MAX2870_ERROR_POWER_LEVEL 3
#define MAX2870_ERROR_AUX_POWER_LEVEL 4
#define MAX2870_ERROR_AUX_FREQ_DIVIDER 5
#define MAX2870_ERROR_ZERO_PFD_FREQUENCY 6
#define MAX2870_ERROR_MOD_RANGE 7
#define MAX2870_ERROR_FRAC_RANGE 8
#define MAX2870_ERROR_N_RANGE 9
#define MAX2870_ERROR_N_RANGE_FRAC 10
#define MAX2870_ERROR_RF_FREQUENCY_AND_STEP_FREQUENCY_HAS_REMAINDER 11
#define MAX2870_ERROR_PFD_EXCEEDED_WITH_FRACTIONAL_MODE 12
#define MAX2870_ERROR_PRECISION_FREQUENCY_CALCULATION_TIMEOUT 13
#define MAX2870_WARNING_FREQUENCY_ERROR 14

// setrf
#define MAX2870_ERROR_DOUBLER_EXCEEDED 15
#define MAX2870_ERROR_R_RANGE 16
#define MAX2870_ERROR_REF_FREQUENCY 17
#define MAX2870_ERROR_REF_MULTIPLIER_TYPE 18

// setf and setrf
#define MAX2870_ERROR_PFD_AND_STEP_FREQUENCY_HAS_REMAINDER 19
#define MAX2870_ERROR_PFD_LIMITS 20

#define MAX2870_RegsToWrite 6UL // for high speed sweep

// ReadCurrentFrequency
#define MAX2870_DIGITS 10
#define MAX2870_DECIMAL_PLACES 6
#define MAX2870_ReadCurrentFrequency_ArraySize (MAX2870_DIGITS + MAX2870_DECIMAL_PLACES + 2) // including decimal point and null terminator

/*!
   @brief MAX2870 chip device driver

   This class provides the overall interface for MAX2870 chip. It is used
   to define the SPI connection, initalize the chip on power up, disable/enable
   frequency generation, and set the frequency and reference frequency.

   The PLL values and register values can also be set directly with this class,
   and current settings for the chip and PLL can be read.

   As a simple frequency generator, once the target frequency and desired channel step
   value is set, the library will perform the required calculations to
   set the PLL and other values, and determine the mode (Frac-N or Int-N)
   for the PLL loop. This greatly simplifies the use of the MAX2870 chip.

   The MAX2870 datasheet should be consulted to understand the correct
   register settings. While a number of checks are provided in the library,
   not all values are checked for allowed settings, so YMMV.

*/
class MAX2870
{
  public:
    /*!
       Constructor
       creates an object and sets the SPI parameters.
       see the Arduino SPI library for the parameter values.
       @param pin the SPI Slave Select Pin to use
       @param mode the SPI Mode (see SPI mode define values)
       @param speed the SPI Serial Speed (see SPI speed values)
       @param order the SPI bit order (see SPI bit order values)
    */
    uint8_t MAX2870_PIN_SS = 10;   ///< Ard Pin for SPI Slave Select

    MAX2870();
    void WriteRegs();

    uint16_t ReadR();
    uint16_t ReadInt();
    uint16_t ReadFraction();
    uint16_t ReadMod();
    uint8_t ReadOutDivider();
    uint8_t ReadOutDivider_PowerOf2();
    uint8_t ReadRDIV2();
    uint8_t ReadRefDoubler();
    double ReadPFDfreq();
    int32_t ReadFrequencyError();

    void init(uint8_t SSpin, uint8_t LockPinNumber, bool Lock_Pin_Used, uint8_t CEpin, bool CE_Pin_Used) ;
    int SetStepFreq(uint32_t value);
    int setf(char *freq, uint8_t PowerLevel, uint8_t AuxPowerLevel, uint8_t AuxFrequencyDivider, bool PrecisionFrequency, uint32_t FrequencyTolerance, uint32_t CalculationTimeout) ; // set freq and power levels and output mode with option for precision frequency setting with tolerance in Hz
    void setfDirect(uint16_t R_divider, uint16_t INT_value,uint16_t MOD_value,uint16_t FRAC_value, uint8_t RF_DIVIDER_value, bool FRACTIONAL_MODE);
    int setrf(uint32_t f, uint16_t r, uint8_t ReferenceDivisionType) ; // set reference freq and reference divider (default is 10 MHz with divide by 1)
    int setPowerLevel(uint8_t PowerLevel);
    int setAuxPowerLevel(uint8_t PowerLevel);

    void WriteSweepValues(const uint32_t *regs);
    void ReadSweepValues(uint32_t *regs);
    void ReadCurrentFrequency(char *freq);

    SPISettings MAX2870_SPI;

    int32_t MAX2870_FrequencyError = 0;
    // power on defaults
    uint32_t MAX2870_reffreq = MAX2870_REF_FREQ_DEFAULT;
    uint32_t MAX2870_R[6] {0x007D0000, 0x2000FFF9, 0x18006E42, 0x0000000B, 0x6180B23C, 0x00400005};
    uint32_t MAX2870_ChanStep = 100000UL;

};

#endif