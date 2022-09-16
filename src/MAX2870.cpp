/*!

   @file MAX2870.cpp

   @mainpage MAX2870 Arduino library driver for Wideband Frequency Synthesizer

   @section intro_sec Introduction

   The MAX2870 chip is a wideband freqency synthesizer integrated circuit that can generate frequencies
   from 23.475 MHz to 6 GHz. It incorporates a PLL (Fraction-N and Integer-N modes) and VCO, along with
   prescalers, dividers and multipiers.  The users add a PLL loop filter and reference frequency to
   create a frequency generator with a very wide range, that is tuneable in settable frequency steps.

   The MAX2870 chip provides an SPI interface for setting the device registers that control the
   frequency and output levels, along with several IO pins for gathering chip status and
   enabling/disabling output and power modes.

   The MAX2870 library provides an Arduino API for accessing the features of the ADF chip.

   The basic PLL equations for the MAX2870 are:

   \f$ RF_{out} = f_{PFD} \times (INT +(\frac{FRAC}{MOD})) \f$

   where:

   \f$ f_{PFD} = REF_{IN} \times \left[ \frac{(1 + D)}{( R \times (1 + T))} \right]  \f$

   \f$ D = \textrm{RD2refdouble, ref doubler flag}\f$

   \f$ R = \textrm{RCounter, ref divider}\f$

   \f$ T = \textrm{RD1Rdiv2, ref divide by 2 flag}\f$




   @section dependencies Dependencies

   This library uses the BigNumber library from Nick Gammon
   Requires the BitFieldManipulation library: http://github.com/brycecherry75/BitFieldManipulation
   Requires the BeyondByte library: http://github.com/brycecherry75/BeyondByte

   @section author Author

   Bryce Cherry

*/

#include "MAX2870.h"

MAX2870::MAX2870()
{
  SPISettings MAX2870_SPI(10000000UL, MSBFIRST, SPI_MODE0);
}

void MAX2870::WriteRegs()
{
  for (int i = 5 ; i >= 0 ; i--) { // sequence according to the MAX2870 datasheet
    SPI.beginTransaction(MAX2870_SPI);
    digitalWrite(MAX2870_PIN_SS, LOW);
    delayMicroseconds(1);
    BeyondByte.writeDword(0, MAX2870_R[i], 4, BeyondByte_SPI, MSBFIRST);
    delayMicroseconds(1);
    digitalWrite(MAX2870_PIN_SS, HIGH);
    SPI.endTransaction();
    delayMicroseconds(1);
  }
}

void MAX2870::WriteSweepValues(const uint32_t *regs) {
  for (int i = 0; i < MAX2870_RegsToWrite; i++) {
    MAX2870_R[i] = regs[i];
  }
  WriteRegs();
}

void MAX2870::ReadSweepValues(uint32_t *regs) {
  for (int i = 0; i < MAX2870_RegsToWrite; i++) {
    regs[i] = MAX2870_R[i];
  }
}

uint16_t MAX2870::ReadR() {
  return BitFieldManipulation.ReadBF_dword(14, 10, MAX2870_R[0x02]);
}

uint16_t MAX2870::ReadInt() {
  return BitFieldManipulation.ReadBF_dword(15, 16, MAX2870_R[0x00]);
}

uint16_t MAX2870::ReadFraction() {
  return BitFieldManipulation.ReadBF_dword(3, 12, MAX2870_R[0x00]);
}

uint16_t MAX2870::ReadMod() {
  return BitFieldManipulation.ReadBF_dword(3, 12, MAX2870_R[0x01]);
}

uint8_t MAX2870::ReadOutDivider() {
  return (1 << BitFieldManipulation.ReadBF_dword(20, 3, MAX2870_R[0x04]));
}

uint8_t MAX2870::ReadOutDivider_PowerOf2() {
  return BitFieldManipulation.ReadBF_dword(20, 3, MAX2870_R[0x04]);
}

uint8_t MAX2870::ReadRDIV2() {
  return BitFieldManipulation.ReadBF_dword(24, 1, MAX2870_R[0x02]);
}

uint8_t MAX2870::ReadRefDoubler() {
  return BitFieldManipulation.ReadBF_dword(25, 1, MAX2870_R[0x02]);
}

double MAX2870::ReadPFDfreq() {
  double value = MAX2870_reffreq;
  uint16_t temp = ReadR();
  if (temp == 0) { // avoid division by zero
    return 0;
  }
  value /= temp;
  if (ReadRDIV2() != 0) {
    value /= 2;
  }
  if (ReadRefDoubler() != 0) {
    value *= 2;
  }
  return value;
}

int32_t MAX2870::ReadFrequencyError() {
  return MAX2870_FrequencyError;
}

void MAX2870::ReadCurrentFrequency(char *freq)
{
  BigNumber::begin(12);
  char tmpstr[12];
  ultoa(MAX2870_reffreq, tmpstr, 10);
  BigNumber BN_ref = BigNumber(tmpstr);
  if (ReadRDIV2() != 0 && ReadRefDoubler() == 0) {
    BN_ref /= BigNumber(2);
  }
  else if (ReadRDIV2() == 0 && ReadRefDoubler() != 0) {
    BN_ref *= BigNumber(2);
  }
  BN_ref /= BigNumber(ReadR());
  BigNumber BN_freq = BN_ref;
  BN_freq *= BigNumber(ReadInt());
  BN_ref *= BigNumber(ReadFraction());
  BN_ref /= BigNumber(ReadMod());
  BN_freq += BN_ref;
  BN_freq /= BigNumber(ReadOutDivider());
  BigNumber BN_rounding = BigNumber("0.5");
  for (int i = 0; i < MAX2870_DECIMAL_PLACES; i++) {
    BN_rounding /= BigNumber(10);
  }
  BN_freq += BN_rounding;
  char* temp = BN_freq.toString();
  BigNumber::finish();
  uint8_t DecimalPlaceToStart;
  for (int i = 0; i < (MAX2870_DIGITS + 1); i++){
    freq[i] = temp[i];
    if (temp[i] == '.') {
      DecimalPlaceToStart = i;
      DecimalPlaceToStart++;
      break;
    }
  }
  for (int i = DecimalPlaceToStart; i < (DecimalPlaceToStart + MAX2870_DECIMAL_PLACES); i++) {
    freq[i] = temp[i];
  }
  freq[(DecimalPlaceToStart + MAX2870_DECIMAL_PLACES)] = 0x00;
  free(temp);
}

void MAX2870::init(uint8_t SSpin, uint8_t LockPinNumber, bool Lock_Pin_Used, uint8_t CEpinNumber, bool CE_Pin_Used)
{
  MAX2870_PIN_SS = SSpin;
  pinMode(MAX2870_PIN_SS, OUTPUT) ;
  digitalWrite(MAX2870_PIN_SS, HIGH) ;
  if (CE_Pin_Used == true) {
    pinMode(CEpinNumber, OUTPUT) ;
  }
  if (Lock_Pin_Used == true) {
    pinMode(LockPinNumber, INPUT_PULLUP) ;
  }
  SPI.begin();
}

int MAX2870::SetStepFreq(uint32_t value) {
  if (value > ReadPFDfreq()) {
    return MAX2870_ERROR_STEP_FREQUENCY_EXCEEDS_PFD;
  }
  uint16_t Rvalue = ReadR();
  if (Rvalue == 0 || (MAX2870_reffreq % value) != 0) {
    return MAX2870_ERROR_PFD_AND_STEP_FREQUENCY_HAS_REMAINDER;
  }
  MAX2870_ChanStep = value;
  return MAX2870_ERROR_NONE;
}

int  MAX2870::setf(char *freq, uint8_t PowerLevel, uint8_t AuxPowerLevel, uint8_t AuxFrequencyDivider, bool PrecisionFrequency, uint32_t MaximumFrequencyError, uint32_t CalculationTimeout) {
  //  calculate settings from freq
  if (PowerLevel < 0 || PowerLevel > 4) return MAX2870_ERROR_POWER_LEVEL;
  if (AuxPowerLevel < 0 || AuxPowerLevel > 4) return MAX2870_ERROR_AUX_POWER_LEVEL;
  if (AuxFrequencyDivider != MAX2870_AUX_DIVIDED && AuxFrequencyDivider != MAX2870_AUX_FUNDAMENTAL) return MAX2870_ERROR_AUX_FREQ_DIVIDER;
  if (ReadPFDfreq() == 0) return MAX2870_ERROR_ZERO_PFD_FREQUENCY;

  uint32_t ReferenceFrequency = MAX2870_reffreq;
  ReferenceFrequency /= ReadR();
  if (PrecisionFrequency == false && MAX2870_ChanStep > 1 && (ReferenceFrequency % MAX2870_ChanStep) != 0) {
    return MAX2870_ERROR_PFD_AND_STEP_FREQUENCY_HAS_REMAINDER;
  }

  BigNumber::begin(12); // for a maximum 105 MHz PFD and a 128 RF divider with frequency steps no smaller than 1 Hz, will fit the maximum of 13.44 * (10 ^ 9) for the MOD and FRAC before GCD calculation

  if (BigNumber(freq) > BigNumber("6000000000") || BigNumber(freq) < BigNumber("23437500")) {
    BigNumber::finish();
    return MAX2870_ERROR_RF_FREQUENCY;
  }

  uint8_t FrequencyPointer = 0;
  while (true) { // null out any decimal places below 1 Hz increments to avoid GCD calculation input overflow
    if (freq[FrequencyPointer] == '.') { // change the decimal point to a null terminator
      freq[FrequencyPointer] = 0x00;
      break;
    }
    if (freq[FrequencyPointer] == 0x00) { // null terminator reached
      break;
    }
    FrequencyPointer++;
  }

  char tmpstr[12]; // will fit a long including sign and terminator

  if (PrecisionFrequency == false && MAX2870_ChanStep > 1) {
    ultoa(MAX2870_ChanStep, tmpstr, 10);
    BigNumber BN_freq = BigNumber(freq);
    // BigNumber has issues with modulus calculation which always results in 0
    BN_freq /= BigNumber(tmpstr);
    uint32_t ChanSteps = (uint32_t)((uint32_t) BN_freq); // round off the decimal - overflow is not an issue for the MAX2870 frequency range
    ultoa(ChanSteps, tmpstr, 10);
    BN_freq -= BigNumber(tmpstr);
    if (BN_freq != BigNumber(0)) {
      BigNumber::finish();
      return MAX2870_ERROR_RF_FREQUENCY_AND_STEP_FREQUENCY_HAS_REMAINDER;
    }
  }

  BigNumber BN_localosc_ratio = BigNumber("3000000000") / BigNumber(freq);
  uint8_t localosc_ratio = (uint32_t)((uint32_t) BN_localosc_ratio);
  uint8_t MAX2870_outdiv = 1 ;
  uint8_t MAX2870_RfDivSel = 0 ;
  uint32_t MAX2870_N_Int;
  uint32_t MAX2870_Mod = 2;
  uint32_t MAX2870_Frac = 0;

  ultoa(MAX2870_reffreq, tmpstr, 10);
  word CurrentR = ReadR();
  uint8_t RDIV2 = ReadRDIV2();
  uint8_t RefDoubler = ReadRefDoubler();
  BigNumber BN_MAX2870_PFDFreq = (BigNumber(tmpstr) * (BigNumber(1) * BigNumber((1 + RefDoubler))) * (BigNumber(1) / BigNumber((1 + RDIV2)))) / BigNumber(CurrentR);
  uint32_t PFDFreq = (uint32_t)((uint32_t) BN_MAX2870_PFDFreq); // used for checking maximum PFD limit under Fractional Mode

  // select the output divider
  if (BigNumber(freq) > BigNumber("23437500")) {
    while (MAX2870_outdiv <= localosc_ratio && MAX2870_outdiv <= 64) {
      MAX2870_outdiv *= 2;
      MAX2870_RfDivSel++;
    }
  }
  else {
    MAX2870_outdiv = 128;
    MAX2870_RfDivSel = 7;
  }

  bool CalculationTookTooLong = false;
  BigNumber BN_MAX2870_N_Int = (BigNumber(freq) / BN_MAX2870_PFDFreq) * BigNumber(MAX2870_outdiv); // for 4007.5 MHz RF/10 MHz PFD, result is 400.75;
  MAX2870_N_Int = (uint32_t)((uint32_t) BN_MAX2870_N_Int); // round off the decimal
  ultoa(MAX2870_N_Int, tmpstr, 10);
  BigNumber BN_FrequencyRemainder;
  if (PrecisionFrequency == true) { // frequency is 4007.5 MHz, PFD is 10 MHz and output divider is 2
    uint32_t CalculationTimeStart = millis();
    BN_FrequencyRemainder = ((BN_MAX2870_PFDFreq * BigNumber(tmpstr)) / BigNumber(MAX2870_outdiv)) - BigNumber(freq); // integer is 4000 MHz, remainder is -7.5 MHz and will be converterd to a positive
    if (BN_FrequencyRemainder < BigNumber(0)) {
      BN_FrequencyRemainder *= BigNumber("-1"); // convert to a postivie
    }
    BigNumber BN_MAX2870_N_Int_Overflow = (BN_MAX2870_N_Int + BigNumber("0.00024421")); // deal with N having remainder greater than (4094 / 4095) and a frequency within ((PFD - (PFD * (1 / 4095)) / output divider)
    uint32_t MAX2870_N_Int_Overflow = (uint32_t)((uint32_t) BN_MAX2870_N_Int_Overflow);
    if (MAX2870_N_Int_Overflow == MAX2870_N_Int) { // deal with N having remainder greater than (4094 / 4095) and a frequency within ((PFD - (PFD * (1 / 4095)) / output divider)
      MAX2870_FrequencyError = (int32_t)((int32_t) BN_FrequencyRemainder); // initial value should the MOD match loop fail to result in FRAC < MOD
      if (MAX2870_FrequencyError > MaximumFrequencyError) { // use fractional division if out of tolerance
        uint32_t FreqeucnyError = MAX2870_FrequencyError;
        uint32_t PreviousFrequencyError = MAX2870_FrequencyError;
        for (word ModToMatch = 2; ModToMatch <= 4095; ModToMatch++) {
          if (CalculationTimeout > 0) {
            uint32_t CalculationTime = millis();
            CalculationTime -= CalculationTimeStart;
            if (CalculationTime > CalculationTimeout) {
              CalculationTookTooLong = true;
              break;
            }
          }
          BigNumber BN_ModFrequencyStep = BN_MAX2870_PFDFreq / BigNumber(ModToMatch) / BigNumber(MAX2870_outdiv); // For 4007.5 MHz RF/10 MHz PFD, should be 4
          BigNumber BN_TempFrac = (BN_FrequencyRemainder / BN_ModFrequencyStep) + BigNumber("0.5"); // result should be 3 to correspond with above line
          uint32_t TempFrac = (uint32_t)((uint32_t) BN_TempFrac);
          if (TempFrac <= ModToMatch) { // FRAC must be < MOD
            if (TempFrac == ModToMatch) { // FRAC must be < MOD
              TempFrac--;
            }
            ultoa(TempFrac, tmpstr, 10);
            BigNumber BN_FrequencyError = (BN_FrequencyRemainder - (BigNumber(tmpstr) * BN_ModFrequencyStep));
            if (BN_FrequencyError < BigNumber(0)) {
              BN_FrequencyError *= BigNumber("-1"); // convert to a postivie
            }
            MAX2870_FrequencyError = (int32_t)((int32_t) BN_FrequencyError);
            if (MAX2870_FrequencyError < PreviousFrequencyError) {
              PreviousFrequencyError = MAX2870_FrequencyError;
              MAX2870_Mod = ModToMatch; // result should be 4 for 4007.5 MHz/10 MHz PFD
              MAX2870_Frac = TempFrac; // result should be 3 to correspond with above line
            }
            if (MAX2870_FrequencyError <= MaximumFrequencyError) { // tolerance has been obtained - for 4007.5 MHz, MOD = 4, FRAC = 3; error = 0
              break;
            }
          }
        }
      }
    }
    else {
      MAX2870_N_Int++;
    }
    ultoa(MAX2870_N_Int, tmpstr, 10);
  }
  else {
    BN_MAX2870_N_Int = (((BigNumber(freq) * BigNumber(MAX2870_outdiv))) / BN_MAX2870_PFDFreq);
    MAX2870_N_Int = (uint32_t)((uint32_t) BN_MAX2870_N_Int);
    ultoa(MAX2870_ChanStep, tmpstr, 10);
    BigNumber BN_MAX2870_Mod = (BN_MAX2870_PFDFreq / (BigNumber(tmpstr) / BigNumber(MAX2870_outdiv)));
    ultoa(MAX2870_N_Int, tmpstr, 10);
    BigNumber BN_MAX2870_Frac = (((BN_MAX2870_N_Int - BigNumber(tmpstr)) * BN_MAX2870_Mod) + BigNumber("0.5"));
    // for a maximum 105 MHz PFD and a 128 RF divider with frequency steps no smaller than 1 Hz, maximum results for each is 13.44 * (10 ^ 9) but can be divided by the RF division ratio without error (results will be no larger than 105 * (10 ^ 6))
    BN_MAX2870_Frac /= BigNumber(MAX2870_outdiv);
    BN_MAX2870_Mod /= BigNumber(MAX2870_outdiv);

    // calculate the GCD - Mod2/Frac2 values are temporary
    uint32_t GCD_MAX2870_Mod2 = (uint32_t)((uint32_t) BN_MAX2870_Mod);
    uint32_t GCD_MAX2870_Frac2 = (uint32_t)((uint32_t) BN_MAX2870_Frac);
    uint32_t GCD_t;
    while (true) {
      if (GCD_MAX2870_Mod2 == 0) {
        GCD_t = GCD_MAX2870_Frac2;
        break;
      }
      if (GCD_MAX2870_Frac2 == 0) {
        GCD_t = GCD_MAX2870_Mod2;
        break;
      }
      if (GCD_MAX2870_Mod2 == GCD_MAX2870_Frac2) {
        GCD_t = GCD_MAX2870_Mod2;
        break;
      }
      if (GCD_MAX2870_Mod2 > GCD_MAX2870_Frac2) {
        GCD_MAX2870_Mod2 -= GCD_MAX2870_Frac2;
      }
      else {
        GCD_MAX2870_Frac2 -= GCD_MAX2870_Mod2;
      }
    }
    // restore the original Mod2/Frac2 temporary values before dividing by GCD
    GCD_MAX2870_Mod2 = (uint32_t)((uint32_t) BN_MAX2870_Mod);
    GCD_MAX2870_Frac2 = (uint32_t)((uint32_t) BN_MAX2870_Frac);
    GCD_MAX2870_Mod2 /= GCD_t;
    GCD_MAX2870_Frac2 /= GCD_t;
    if (GCD_MAX2870_Mod2 > 4095) { // outside valid range
      while (true) {
        GCD_MAX2870_Mod2 /= 2;
        GCD_MAX2870_Frac2 /= 2;
        if (GCD_MAX2870_Mod2 <= 4095) { // now within valid range
          if (GCD_MAX2870_Frac2 == GCD_MAX2870_Mod2) { // FRAC must be less than MOD
            GCD_MAX2870_Frac2--;
          }
          break;
        }
      }
    }
    // set the final FRAC/MOD values
    MAX2870_Frac = GCD_MAX2870_Frac2;
    MAX2870_Mod = GCD_MAX2870_Mod2;
  }
  if (CalculationTookTooLong == true) {
    BigNumber::finish();
    return MAX2870_ERROR_PRECISION_FREQUENCY_CALCULATION_TIMEOUT;
  }

  BN_FrequencyRemainder = (((((BN_MAX2870_PFDFreq * BigNumber(tmpstr)) + (BigNumber(MAX2870_Frac) * (BN_MAX2870_PFDFreq / BigNumber(MAX2870_Mod)))) / BigNumber(MAX2870_outdiv))) - BigNumber(freq)) + BigNumber("0.5"); // no issue with divide by 0 regarding MOD (set to 2 by default) and FRAC (set to 0 by default) - maximum is PFD maximum frequency of 105 MHz under integer mode - no issues with signed overflow or underflow
  MAX2870_FrequencyError = (int32_t)((int32_t) BN_FrequencyRemainder);

  BigNumber::finish();

  if (MAX2870_Frac == 0) { // correct the MOD to the minimum required value
    MAX2870_Mod = 2;
  }

  if ( MAX2870_Mod < 2 || MAX2870_Mod > 4095) {
    return MAX2870_ERROR_MOD_RANGE;
  }

  if ( (uint32_t) MAX2870_Frac > (MAX2870_Mod - 1) ) {
    return MAX2870_ERROR_FRAC_RANGE;
  }

  if (MAX2870_Frac == 0 && (MAX2870_N_Int < 16  || MAX2870_N_Int > 65535)) {
    return MAX2870_ERROR_N_RANGE;
  }

  if (MAX2870_Frac != 0 && (MAX2870_N_Int < 19  || MAX2870_N_Int > 4091)) {
    return MAX2870_ERROR_N_RANGE_FRAC;
  }

  if (MAX2870_Frac != 0 && PFDFreq > MAX2870_PFD_MAX_FRAC) {
    return MAX2870_ERROR_PFD_EXCEEDED_WITH_FRACTIONAL_MODE;
  }

  MAX2870_R[0x00] = BitFieldManipulation.WriteBF_dword(3, 12, MAX2870_R[0x00], MAX2870_Frac);
  MAX2870_R[0x00] = BitFieldManipulation.WriteBF_dword(15, 16, MAX2870_R[0x00], MAX2870_N_Int);

  if (MAX2870_Frac == 0) {
    MAX2870_R[0x00] = BitFieldManipulation.WriteBF_dword(31, 1, MAX2870_R[0x00], 1); // integer-n mode
    MAX2870_R[0x01] = BitFieldManipulation.WriteBF_dword(29, 2, MAX2870_R[0x01], 0); // Charge Pump Linearity
    MAX2870_R[0x01] = BitFieldManipulation.WriteBF_dword(31, 1, MAX2870_R[0x01], 1); // Charge Pump Output Clamp
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(8, 1, MAX2870_R[0x02], 1); // Lock Detect Function, int-n mode
    MAX2870_R[0x05] = BitFieldManipulation.WriteBF_dword(24, 1, MAX2870_R[0x05], 1); // integer-n mode
  }
  else {
    MAX2870_R[0x00] = BitFieldManipulation.WriteBF_dword(31, 1, MAX2870_R[0x00], 0); // integer-n mode
    MAX2870_R[0x01] = BitFieldManipulation.WriteBF_dword(29, 2, MAX2870_R[0x01], 1); // Charge Pump Linearity
    MAX2870_R[0x01] = BitFieldManipulation.WriteBF_dword(31, 1, MAX2870_R[0x01], 0); // Charge Pump Output Clamp
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(8, 1, MAX2870_R[0x02], 0); // Lock Detect Function, frac-n mode
    MAX2870_R[0x05] = BitFieldManipulation.WriteBF_dword(24, 1, MAX2870_R[0x05], 0); // integer-n mode
  }
  // (0x01, 15, 12, 1) phase
  MAX2870_R[0x01] = BitFieldManipulation.WriteBF_dword(3, 12, MAX2870_R[0x01], MAX2870_Mod);
  // (0x02, 3,1,0) counter reset
  // (0x02, 4,1,0) cp3 state
  // (0x02, 5,1,0) power down
  if (PFDFreq > 32000000UL) { // lock detect speed adjustment
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(31, 1, MAX2870_R[0x02], 1); // Lock Detect Speed
  }
  else  {
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(31, 1, MAX2870_R[0x02], 0); // Lock Detect Speed
  }
  // (0x02, 13,1,0) dbl buf
  // (0x02, 26,3,0) //  muxout, not used
  // (0x02, 29,2,0) low noise and spurs mode
  // (0x03, 15,2,1) clk div mode
  // (0x03, 17,1,0) reserved
  // (0x03, 18,6,0) reserved
  // (0x03, 24,1,0) VAS response to temperature drift
  // (0x03, 25,1,0) VAS state machine
  // (0x03, 26,6,0) VCO and VCO sub-band manual selection
  if (PowerLevel == 0) {
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(5, 1, MAX2870_R[0x04], 0);
  }
  else {
    PowerLevel--;
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(5, 1, MAX2870_R[0x04], 1);
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(3, 2, MAX2870_R[0x04], PowerLevel);
  }
  if (AuxPowerLevel == 0) {
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(8, 1, MAX2870_R[0x04], 0);
  }
  else {
    AuxPowerLevel--;
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(6, 2, MAX2870_R[0x04], AuxPowerLevel);
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(8, 1, MAX2870_R[0x04], 1);
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(9, 1, MAX2870_R[0x04], AuxFrequencyDivider);
  }
  // (0x04, 10,1,0) reserved
  // (0x04, 11,1,0) reserved
  // (0x04, 12,8,1) Band Select Clock Divider
  MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(20, 3, MAX2870_R[0x04], MAX2870_RfDivSel); // rf divider select
  // (0x04, 23,8,0) reserved
  // (0x04, 24,2,1) Band Select Clock Divider MSBs
  // (0x04, 26,6,1) reserved
  // (0x05, 3,15,0) reserved
  // (0x05, 18,1,0) MUXOUT pin mode
  // (0x05, 22,2,1) lock pin function
  // (0x05, 25,7,0) reserved
  WriteRegs();

  bool NegativeError = false;
  if (MAX2870_FrequencyError < 0) { // convert to a positive for frequency error comparison with a positive value
    MAX2870_FrequencyError ^= 0xFFFFFFFF;
    MAX2870_FrequencyError++;
    NegativeError = true;
  }
  if ((PrecisionFrequency == true && MAX2870_FrequencyError > MaximumFrequencyError) || (PrecisionFrequency == false && MAX2870_FrequencyError != 0)) {
    if (NegativeError == true) { // convert back to negative if changed from negative to positive for frequency error comparison with a positive value
      MAX2870_FrequencyError ^= 0xFFFFFFFF;
      MAX2870_FrequencyError++;
    }
    return MAX2870_WARNING_FREQUENCY_ERROR;
  }

  return MAX2870_ERROR_NONE; // ok
}

int MAX2870::setrf(uint32_t f, uint16_t r, uint8_t ReferenceDivisionType)
{
  if (f > 30000000UL && ReferenceDivisionType == MAX2870_REF_DOUBLE) return MAX2870_ERROR_DOUBLER_EXCEEDED;
  if (r > 1023 || r < 1) return MAX2870_ERROR_R_RANGE;
  if (f < MAX2870_REFIN_MIN || f > MAX2870_REFIN_MAX) return MAX2870_ERROR_REF_FREQUENCY;
  if (ReferenceDivisionType != MAX2870_REF_UNDIVIDED && ReferenceDivisionType != MAX2870_REF_HALF && ReferenceDivisionType != MAX2870_REF_DOUBLE) return MAX2870_ERROR_REF_MULTIPLIER_TYPE;

  double ReferenceFactor = 1;
  if (ReferenceDivisionType == MAX2870_REF_HALF) {
    ReferenceFactor /= 2;
  }
  else if (ReferenceDivisionType == MAX2870_REF_DOUBLE) {
    ReferenceFactor *= 2;
  }
  double newfreq  =  (double) f  * ( (double) ReferenceFactor / (double) r);  // check the loop freq

  if ( newfreq > MAX2870_PFD_MAX || newfreq < MAX2870_PFD_MIN ) return MAX2870_ERROR_PFD_LIMITS;

  MAX2870_reffreq = f ;
  MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(14, 10, MAX2870_R[0x02], r);
  if (ReferenceDivisionType == MAX2870_REF_DOUBLE) {
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(24, 2, MAX2870_R[0x02], 0b00000010);
  }
  else if (ReferenceDivisionType == MAX2870_REF_HALF) {
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(24, 2, MAX2870_R[0x02], 0b00000001);
  }
  else {
    MAX2870_R[0x02] = BitFieldManipulation.WriteBF_dword(24, 2, MAX2870_R[0x02], 0b00000000);
  }
  return MAX2870_ERROR_NONE;
}

int MAX2870::setPowerLevel(uint8_t PowerLevel) {
  if (PowerLevel < 0 && PowerLevel > 4) return MAX2870_ERROR_POWER_LEVEL;
  if (PowerLevel == 0) {
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(5, 1, MAX2870_R[0x04], 0);
  }
  else {
    PowerLevel--;
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(5, 1, MAX2870_R[0x04], 1);
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(3, 2, MAX2870_R[0x04], PowerLevel);
  }
  WriteRegs();
  return MAX2870_ERROR_NONE;
}

int MAX2870::setAuxPowerLevel(uint8_t PowerLevel) {
  if (PowerLevel < 0 && PowerLevel > 4) return MAX2870_ERROR_POWER_LEVEL;
  if (PowerLevel == 0) {
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(8, 1, MAX2870_R[0x04], 0);
  }
  else {
    PowerLevel--;
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(6, 2, MAX2870_R[0x04], PowerLevel);
    MAX2870_R[0x04] = BitFieldManipulation.WriteBF_dword(8, 1, MAX2870_R[0x04], 1);
  }
  WriteRegs();
  return MAX2870_ERROR_NONE;
}