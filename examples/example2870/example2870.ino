/*

  MAX2870 demo by Bryce Cherry

  Commands:
  REF reference_frequency_in_Hz reference_divider reference_multiplier(UNDIVIDED/DOUBLE/HALF) - Set reference frequency, reference divider and reference doubler/divide by 2
  (FREQ/FREQ_P) frequency_in_Hz power_level(0-4) aux_power_level(0-4) aux_frequency_output(DIVIDED/FUNDAMENTAL) frequency_tolerance_in_Hz calculation_timeout_in_mS - set RF frequency (FREQ_P sets precision mode), power level, auxiliary output frequency mode, frequency tolerance (precision mode only), calculation timeout (precision mode only - 0 to disable)
  FREQ_DIRECT R_divider INT_value MOD_value FRAC_value RF_DIVIDER_value PRESCALER_value FRACTIONAL_MODE(true/false) - sets RF parameters directly
  (BURST/BURST_CONT/BURST_SINGLE) on_time_in_uS off time_in_uS count (AUX) - perform a on/off burst on frequency and power level set with FREQ/FREQ_P - count is only used with BURST_CONT - if AUX is used, will burst on the auxiliary output; otherwise, it will burst on the primary output
  SWEEP start_frequency stop_frequency step_in_mS(1-32767) power_level(1-4) aux_power_level(0-4) aux_frequency_output(DIVIDED/FUNDAMENTAL) - sweep RF frequency
  STEP frequency_in_Hz - set channel step
  STATUS - view status of VFO
  CE (ON/OFF) - enable/disable MAX2870

*/

#include <MAX2870.h>
#include <BigNumber.h> // obtain at https://github.com/nickgammon/BigNumber

MAX2870 vfo;

// use hardware SPI pins for Data and Clock
const byte SSpin = 10; // LE
const byte LockPin = 12; // MISO
const byte CEpin = 9;

const word SweepSteps = 14; // SweepSteps * ((4 * 6) + (2 * 3)) is the temporary memory calculation (remember to leave enough for BigNumber) - 14 is the limit which will not cause an ATmega328 based board to hang during a frequency sweep

const int CommandSize = 50;
char Command[CommandSize];

// ensures that the serial port is flushed fully on request
const unsigned long SerialPortRate = 9600;
const byte SerialPortRateTolerance = 5; // percent - increase to 50 for rates above 115200 up to 4000000
const byte SerialPortBits = 10; // start (1), data (8), stop (1)
const unsigned long TimePerByte = ((((1000000ULL * SerialPortBits) / SerialPortRate) * (100 + SerialPortRateTolerance)) / 100); // calculated on serial port rate + tolerance and rounded down to the nearest uS, long caters for even the slowest serial port of 75 bps

void FlushSerialBuffer() {
  while (true) {
    if (Serial.available() > 0) {
      byte dummy = Serial.read();
      while (Serial.available() > 0) { // flush additional bytes from serial buffer if present
        dummy = Serial.read();
      }
      if (TimePerByte <= 16383) {
        delayMicroseconds(TimePerByte); // delay in case another byte may be received via the serial port
      }
      else { // deal with delayMicroseconds limitation
        unsigned long DelayTime = TimePerByte;
        DelayTime /= 1000;
        if (DelayTime > 0) {
          delay(DelayTime);
        }
        DelayTime = TimePerByte;
        DelayTime %= 1000;
        if (DelayTime > 0) {
          delayMicroseconds(DelayTime);
        }
      }
    }
    else {
      break;
    }
  }
}

void getField (char* buffer, int index) {
  int CommandPos = 0;
  int FieldPos = 0;
  int SpaceCount = 0;
  while (CommandPos < CommandSize) {
    if (Command[CommandPos] == 0x20) {
      SpaceCount++;
      CommandPos++;
    }
    if (Command[CommandPos] == 0x0D || Command[CommandPos] == 0x0A) {
      break;
    }
    if (SpaceCount == index) {
      buffer[FieldPos] = Command[CommandPos];
      FieldPos++;
    }
    CommandPos++;
  }
  for (int ch = 0; ch < strlen(buffer); ch++) { // correct case of command
    buffer[ch] = toupper(buffer[ch]);
  }
  buffer[FieldPos] = '\0';
}

void PrintVFOstatus() {
  Serial.print(F("R: "));
  Serial.println(vfo.ReadR());
  if (vfo.ReadRDIV2() != 0 && vfo.ReadRefDoubler() != 0) {
    Serial.println(F("Reference doubler and reference divide by 2 enabled - invalid state"));
  }
  else if (vfo.ReadRDIV2() != 0) {
    Serial.println(F("Reference divide by 2"));
  }
  else if (vfo.ReadRefDoubler() != 0) {
    Serial.println(F("Reference doubler enabled"));
  }
  else {
    Serial.println(F("Reference doubler and divide by 2 disabled"));
  }
  Serial.print(F("Int: "));
  Serial.println(vfo.ReadInt());
  Serial.print(F("Fraction: "));
  Serial.println(vfo.ReadFraction());
  Serial.print(F("Mod: "));
  Serial.println(vfo.ReadMod());
  Serial.print(F("Output divider: "));
  Serial.println(vfo.ReadOutDivider());
  Serial.print(F("Output divider power of 2: "));
  Serial.println(vfo.ReadOutDivider_PowerOf2());
  Serial.print(F("PFD frequency (Hz): "));
  Serial.println(vfo.ReadPFDfreq());
  Serial.print(F("Frequency step (Hz): "));
  Serial.println(vfo.MAX2870_ChanStep);
  Serial.print(F("Frequency error (Hz): "));
  Serial.println(vfo.MAX2870_FrequencyError);
  Serial.print(F("Current frequency (Hz): "));
  char CurrentFreq[MAX2870_ReadCurrentFrequency_ArraySize];
  vfo.ReadCurrentFrequency(CurrentFreq);
  Serial.println(CurrentFreq);
}

void PrintErrorCode(byte value) {
  switch (value) {
    case MAX2870_ERROR_NONE:
      break;
    case MAX2870_ERROR_STEP_FREQUENCY_EXCEEDS_PFD:
      Serial.println(F("Step frequency exceeds PFD frequency"));
      break;
    case MAX2870_ERROR_RF_FREQUENCY:
      Serial.println(F("RF frequency out of range"));
      break;
    case MAX2870_ERROR_POWER_LEVEL:
      Serial.println(F("Power level incorrect"));
      break;
    case MAX2870_ERROR_AUX_POWER_LEVEL:
      Serial.println(F("Auxiliary power level incorrect"));
      break;
    case MAX2870_ERROR_AUX_FREQ_DIVIDER:
      Serial.println(F("Auxiliary frequency divider incorrect"));
      break;
    case MAX2870_ERROR_ZERO_PFD_FREQUENCY:
      Serial.println(F("PFD frequency is zero"));
      break;
    case MAX2870_ERROR_MOD_RANGE:
      Serial.println(F("Mod is out of range"));
      break;
    case MAX2870_ERROR_FRAC_RANGE:
      Serial.println(F("Fraction is out of range"));
      break;
    case MAX2870_ERROR_N_RANGE:
      Serial.println(F("N is of range"));
      break;
    case MAX2870_ERROR_N_RANGE_FRAC:
      Serial.println(F("N is out of range under fractional mode"));
      break;
    case MAX2870_ERROR_RF_FREQUENCY_AND_STEP_FREQUENCY_HAS_REMAINDER:
      Serial.println(F("RF frequency and step frequency division has remainder"));
      break;
    case MAX2870_ERROR_PFD_EXCEEDED_WITH_FRACTIONAL_MODE:
      Serial.println(F("PFD exceeds 50 MHz under fractional mode"));
      break;
    case MAX2870_ERROR_PRECISION_FREQUENCY_CALCULATION_TIMEOUT:
      Serial.println(F("Precision frequency calculation timeout"));
      break;
    case MAX2870_WARNING_FREQUENCY_ERROR:
      Serial.println(F("Actual frequency is different than desired"));
      break;
    case MAX2870_ERROR_DOUBLER_EXCEEDED:
      Serial.println(F("Reference frequency with doubler exceeded"));
      break;
    case MAX2870_ERROR_R_RANGE:
      Serial.println(F("R divider is out of range"));
      break;
    case MAX2870_ERROR_REF_FREQUENCY:
      Serial.println(F("Reference frequency is out of range"));
      break;
    case MAX2870_ERROR_REF_MULTIPLIER_TYPE:
      Serial.println(F("Reference multiplier type is incorrect"));
      break;
    case MAX2870_ERROR_PFD_AND_STEP_FREQUENCY_HAS_REMAINDER:
      Serial.println(F("PFD and step frequency division has remainder"));
      break;
    case MAX2870_ERROR_PFD_LIMITS:
      Serial.println(F("PFD frequency is out of range"));
      break;
  }
}

void setup() {
  Serial.begin(SerialPortRate);
  vfo.init(SSpin, LockPin, true, CEpin, true);
  digitalWrite(CEpin, HIGH); // enable the MAX2870
}

void loop() {
  static int ByteCount = 0;
  if (Serial.available() > 0) {
    char value = Serial.read();
    if (value != '\n' && ByteCount < CommandSize) {
      Command[ByteCount] = value;
      ByteCount++;
    }
    else {
      ByteCount = 0;
      bool ValidField = true;
      char field[20];
      getField(field, 0);
      if (strcmp(field, "REF") == 0) {
        getField(field, 1);
        unsigned long ReferenceFreq = atol(field);
        getField(field, 2);
        word ReferenceDivider = atoi(field);
        getField(field, 3);
        byte ReferenceHalfDouble = MAX2870_REF_UNDIVIDED;
        if (strcmp(field, "DOUBLE") == 0) {
          ReferenceHalfDouble = MAX2870_REF_DOUBLE;
        }
        else if (strcmp(field, "HALF") == 0) {
          ReferenceHalfDouble = MAX2870_REF_HALF;
        }
        byte ErrorCode = vfo.setrf(ReferenceFreq, ReferenceDivider, ReferenceHalfDouble);
        if (ErrorCode != MAX2870_ERROR_NONE) {
          ValidField = false;
          PrintErrorCode(ErrorCode);
        }
      }
      else if (strcmp(field, "FREQ") == 0 || strcmp(field, "FREQ_P") == 0) {
        bool PrecisionRequired = false;
        if (strcmp(field, "FREQ_P") == 0) {
          PrecisionRequired = true;
        }
        getField(field, 2);
        byte PowerLevel = atoi(field);
        getField(field, 3);
        byte AuxPowerLevel = atoi(field);
        getField(field, 4);
        byte AuxFrequencyDivider;
        if (strcmp(field, "DIVIDED") == 0) {
          AuxFrequencyDivider = MAX2870_AUX_DIVIDED;
        }
        else if (strcmp(field, "FUNDAMENTAL") == 0) {
          AuxFrequencyDivider = MAX2870_AUX_FUNDAMENTAL;
        }
        else {
          ValidField = false;
        }
        unsigned long FrequencyTolerance = 0;
        if (PrecisionRequired == true) {
          getField(field, 5);
          FrequencyTolerance = atol(field);
        }
        getField(field, 6);
        unsigned long CalculationTimeout = atol(field);
        unsigned long FrequencyWriteTimeStart = millis();
        if (ValidField == true) {
          getField(field, 1);
          byte ErrorCode = vfo.setf(field, PowerLevel, AuxPowerLevel, AuxFrequencyDivider, PrecisionRequired, FrequencyTolerance, CalculationTimeout);
          if (ErrorCode != MAX2870_ERROR_NONE && ErrorCode != MAX2870_WARNING_FREQUENCY_ERROR) {
            ValidField = false;
          }
          PrintErrorCode(ErrorCode);
          if (ValidField == true) {
            unsigned long FrequencyWriteTime = millis();
            FrequencyWriteTime -= FrequencyWriteTimeStart;
            Serial.print(F("Time measured during setf() with CPU speed of "));
            Serial.print((F_CPU / 1000000UL));
            Serial.print(F("."));
            Serial.print((F_CPU % 1000000UL));
            Serial.print(F(" MHz: "));
            Serial.print((FrequencyWriteTime / 1000));
            Serial.print(F("."));
            Serial.print((FrequencyWriteTime % 1000));
            Serial.println(F(" seconds"));
            PrintVFOstatus();
          }
        }
      }
      else if (strcmp(field, "FREQ_DIRECT") == 0) {
        getField(field, 1);
        word R_divider = atoi(field);
        getField(field, 2);
        word INT_value = atol(field);
        getField(field, 3);
        word MOD_value = atoi(field);
        getField(field, 4);
        word FRAC_value = atoi(field);
        getField(field, 5);
        word RF_DIVIDER_value = atoi(field);
        getField(field, 6);
        if (strcmp(field, "TRUE") == 0 || strcmp(field, "FALSE") == 0) {
          bool FRACTIONAL_MODE = false;
          if (strcmp(field, "TRUE") == 0) {
            FRACTIONAL_MODE = true;
          }
          vfo.setfDirect(R_divider, INT_value, MOD_value, FRAC_value, RF_DIVIDER_value, FRACTIONAL_MODE);
        }
        else {
          ValidField = false;
        }
      }
      else if (strcmp(field, "BURST") == 0 || strcmp(field, "BURST_CONT") == 0 || strcmp(field, "BURST_SINGLE") == 0) {
        bool ContinuousBurst = false;
        bool SingleBurst = false;
        unsigned long BurstCount;
        if (strcmp(field, "BURST_CONT") == 0) {
          ContinuousBurst = true;
        }
        else if (strcmp(field, "BURST_SINGLE") == 0) {
          SingleBurst = true;
        }
        bool AuxOutput = false;
        getField(field, 1);
        unsigned long BurstOnTime = atol(field);
        getField(field, 2);
        unsigned long BurstOffTime = atol(field);
        getField(field, 3);
        if (strcmp(field, "AUX") == 0) {
          AuxOutput = true;
        }
        else if (ContinuousBurst == false && SingleBurst == false) {
          BurstCount = atol(field);
          getField(field, 4);
          if (strcmp(field, "AUX") == 0) {
            AuxOutput = true;
          }
        }
        unsigned long OnBurstData[MAX2870_RegsToWrite];
        vfo.ReadSweepValues(OnBurstData);
        if (AuxOutput == false) {
          vfo.setPowerLevel(0);
        }
        else {
          vfo.setAuxPowerLevel(0);
        }
        unsigned long OffBurstData[MAX2870_RegsToWrite];
        vfo.ReadSweepValues(OffBurstData);
        Serial.print(F("Burst "));
        Serial.print((BurstOnTime / 1000));
        Serial.print(F("."));
        Serial.print((BurstOnTime % 1000));
        Serial.print(F(" mS on, "));
        Serial.print((BurstOffTime / 1000));
        Serial.print(F("."));
        Serial.print((BurstOffTime % 1000));
        Serial.println(F(" mS off"));
        if (SingleBurst == true) {
          vfo.WriteSweepValues(OffBurstData);
          if (BurstOffTime <= 16383) {
            delayMicroseconds(BurstOffTime);
          }
          else {
            delay((BurstOffTime / 1000));
            delayMicroseconds((BurstOffTime % 1000));
          }
        }
        if (ContinuousBurst == false && SingleBurst == false && BurstCount == 0) {
          ValidField = false;
        }
        if (ValidField == true) {
          FlushSerialBuffer();
          while (true) {
            vfo.WriteSweepValues(OnBurstData);
            if (BurstOnTime <= 16383) {
              delayMicroseconds(BurstOnTime);
            }
            else {
              delay((BurstOnTime / 1000));
              delayMicroseconds((BurstOnTime % 1000));
            }
            vfo.WriteSweepValues(OffBurstData);
            if (ContinuousBurst == false && SingleBurst == false) {
              BurstCount--;
            }
            if ((ContinuousBurst == false && BurstCount == 0) || SingleBurst == true || Serial.available() > 0) {
              for (int i = 0; i < MAX2870_RegsToWrite; i++) {
                vfo.MAX2870_R[i] = OnBurstData[i];
              }
              Serial.println(F("End of burst"));
              break;
            }
            if (BurstOffTime <= 16383) {
              delayMicroseconds(BurstOffTime);
            }
            else {
              delay((BurstOffTime / 1000));
              delayMicroseconds((BurstOffTime % 1000));
            }
          }
        }
      }
      else if (strcmp(field, "SWEEP") == 0) {
        BigNumber::begin(12); // will finish on setf()
        getField(field, 1);
        BigNumber BN_StartFrequency(field);
        getField(field, 2);
        BigNumber BN_StopFrequency(field);
        getField(field, 3);
        word SweepStepTime = atoi(field);
        getField(field, 4);
        byte PowerLevel = atoi(field);
        getField(field, 5);
        byte AuxPowerLevel = atoi(field);
        getField(field, 6);
        byte AuxFrequencyDivider;
        if (strcmp(field, "DIVIDED") == 0) {
          AuxFrequencyDivider = MAX2870_AUX_DIVIDED;
        }
        else if (strcmp(field, "FUNDAMENTAL") == 0) {
          AuxFrequencyDivider = MAX2870_AUX_FUNDAMENTAL;
        }
        else {
          ValidField = false;
        }
        if (ValidField == true) {
          if (BN_StartFrequency < BN_StopFrequency) {
            char tmpstr[12];
            ultoa(vfo.MAX2870_ChanStep, tmpstr, 10);
            char tmpstr2[12];
            ultoa(SweepSteps, tmpstr2, 10);
            BigNumber BN_StepSize = ((BN_StopFrequency - BN_StartFrequency) / (BigNumber(tmpstr2)) - BigNumber("1"));
            if (BN_StepSize >= BigNumber(tmpstr)) {
              BigNumber BN_StepSizeRounding = (BN_StepSize / BigNumber(tmpstr));
              uint32_t StepSizeRounding = (uint32_t)((uint32_t) BN_StepSizeRounding);
              ultoa(StepSizeRounding, tmpstr2, 10);
              BN_StepSize = (BigNumber(tmpstr) * BigNumber(tmpstr2));
              char StepSize[14];
              char StartFrequency[14];
              char* tempstring1 = BN_StepSize.toString();
              for (int i = 0; i < 14; i++) {
                byte temp = tempstring1[i];
                if (temp == '.') {
                  StepSize[i] = 0x00;
                  break;
                }
                StepSize[i] = temp;
              }
              free(tempstring1);
              char* tempstring2 = BN_StartFrequency.toString();
              BigNumber::finish();
              for (int i = 0; i < 14; i++) {
                byte temp = tempstring2[i];
                if (temp == '.') {
                  StepSize[i] = 0x00;
                  break;
                }
                StartFrequency[i] = temp;
              }
              free(tempstring2);
              uint32_t regs[(MAX2870_RegsToWrite * SweepSteps)];
              uint32_t reg_temp[MAX2870_RegsToWrite];
              for (word SweepCount = 0; SweepCount < SweepSteps; SweepCount++) {
                Serial.print(F("Calculating step "));
                Serial.print(SweepCount);
                char CurrentFrequency[14];
                BigNumber::begin(12);
                BigNumber BN_CurrentFrequency = (BigNumber(StartFrequency) + (BigNumber(StepSize) * BigNumber(SweepCount)));
                char* tempstring3 = BN_CurrentFrequency.toString();
                BigNumber::finish();
                for (int y = 0; y < 14; y++) {
                  byte temp = tempstring3[y];
                  if (temp == '.') {
                    CurrentFrequency[y] = 0x00;
                    break;
                  }
                  CurrentFrequency[y] = temp;
                }
                free(tempstring3);
                Serial.print(F(" - frequency is now "));
                Serial.print(CurrentFrequency);
                Serial.println(F(" Hz"));
                byte ErrorCode = vfo.setf(CurrentFrequency, PowerLevel, AuxPowerLevel, AuxFrequencyDivider, false, 0, 0);
                if (ErrorCode != MAX2870_ERROR_NONE) {
                  ValidField = false;
                  PrintErrorCode(ErrorCode);
                  break;
                }
                PrintVFOstatus();
                vfo.ReadSweepValues(reg_temp);
                for (int y = 0; y < MAX2870_RegsToWrite; y++) {
                  regs[(y + (MAX2870_RegsToWrite * SweepCount))] = reg_temp[y];
                }
              }
              if (ValidField == true) {
                Serial.println(F("Now sweeping"));
                FlushSerialBuffer();
                while (true) {
                  if (Serial.available() > 0) {
                    break;
                  }
                  for (word SweepCount = 0; SweepCount < SweepSteps; SweepCount++) {
                    if (Serial.available() > 0) {
                      break;
                    }
                    for (int y = 0; y < MAX2870_RegsToWrite; y++) {
                      reg_temp[y] = regs[(y + (MAX2870_RegsToWrite * SweepCount))];
                    }
                    vfo.WriteSweepValues(reg_temp);
                    delay(SweepStepTime);
                  }
                  Serial.print(F("*"));
                }
                Serial.print(F(""));
                Serial.println(F("End of sweep"));
              }
            }
            else {
              BigNumber::finish();
              Serial.println(F("Calculated frequency step is smaller than preset frequency step"));
              ValidField = false;
            }
          }
          else {
            BigNumber::finish();
            Serial.println(F("Stop frequency must be greater than start frequency"));
            ValidField = false;
          }
        }
        else {
          BigNumber::finish();
        }
      }
      else if (strcmp(field, "STEP") == 0) {
        getField(field, 1);
        unsigned long StepFrequency = atol(field);
        byte ErrorCode = vfo.SetStepFreq(StepFrequency);
        if (ErrorCode != MAX2870_ERROR_NONE) {
          ValidField = false;
          PrintErrorCode(ErrorCode);
        }
      }
      else if (strcmp(field, "STATUS") == 0) {
        PrintVFOstatus();
        SPI.end();
        if (digitalRead(LockPin) == LOW) {
          Serial.println(F("Lock pin LOW"));
        }
        else {
          Serial.println(F("Lock pin HIGH"));
        }
        SPI.begin();
      }
      else if (strcmp(field, "CE") == 0) {
        getField(field, 1);
        if (strcmp(field, "ON") == 0) {
          digitalWrite(CEpin, HIGH);
        }
        else if (strcmp(field, "OFF") == 0) {
          digitalWrite(CEpin, LOW);
        }
        else {
          ValidField = false;
        }
      }
      else {
        ValidField = false;
      }
      FlushSerialBuffer();
      if (ValidField == true) {
        Serial.println(F("OK"));
      }
      else {
        Serial.println(F("ERROR"));
      }
    }
  }
}