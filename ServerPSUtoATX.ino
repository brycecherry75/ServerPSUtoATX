/*

  Controller for converting a +12V to +3.3V/+5V server DC-DC converter by Bryce Cherry
  Used to convert a Delta AC-106A (Dell part number 00G8CN from a PowerEdge R320/R420 server) to a 12V input ATX PSU

  If the levels of the undervoltage/overvoltage comparators and/or ATX PSU_ON/ATX Power Good are different to your microcontroller, use the following circuit for each connection:

                    1K0
  High Vcc----------\/\/---\
                           |
                       C   |
               4K7   B /---*-- Output (microcontroller input/ATX Power Good)
  Low Vcc------\/\/---| BC547
                       \------ Input (comparator output/ATX PSU_ON/microcontroller output)
                       E

  High Vcc is +5VSB for ATX Power Good or the Vcc of the microcontroller controlled by this sketch and Low Vcc is a voltage rail corresponding to the voltage level of the signal at the Emitter of the BC547 transistor (NPN).

  Status LED indications:
  Off: Power off
  On: Power on
  Slow flashing: Undervoltage shutdown (can be restarted by toggling ATX PS_ON or with uncommenting of AUTO_RESTART_ON_UNDERVOLTAGE_WITH_ATX_PSUON_ASSERTED define line, automatically after UndervoltageRestartDelay) - some motherboards keep the ATX PSU_ON line asserted when this sketch shuts down the output on undervoltage detection
  Fast flashing: Overvoltage shutdown (can only be restarted by removing power and rectifying the fault)

*/

#define OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE // comment out if your PSU requires high active signals and open circuit voltage must be no higher than +5V
// #define AUTO_RESTART_ON_UNDERVOLTAGE_WITH_ATX_PSUON_ASSERTED // comment out if toggling of ATX PSU_ON is required to restart after undervoltage shutdown; else, will restart automatically when ATX_PSU_ON is still asserted after UndervoltageRestartDelay

// input pins
const byte OVP_pin = 2;
const byte UVP_pin = 3;
const byte ATX_nPSON_pin = 4; // a 1K0 pullup to +5VSB is required here

// output pins
const byte ATX_PG_pin = 5; // a 1K0 pullup to +5VSB is required here
const byte PowerEnable_pin = 6; // low active
const byte StatusLED = 13; // typical pin for onboard LED on most Arduino boards

// undervoltage and overvoltage protection parameters - IRQ type must be supported by your microcontroller
const byte UndervoltageIRQtype = LOW;
const byte OvervoltageIRQtype = LOW;

// timing parameters based on most ATX PC PSU supervisor ICs
const word UndervoltageSenseDelay = 75; // mS on assertion of ATX_nPSON_pin
const word PowerGoodDelay = 300; // mS after UndervoltageSenseDelay is complete and no undervoltage condition exists
const word PSON_OVP_UVP_DebounceDelay = 38; // mS
const word OVP_UVP_DebounceDelay = 73; // uS

// other timing parameters
const word OvervoltageLEDcycleRate = 50; // mS per on/off cycle
const word UndervoltageLEDcycleRate = 250; // mS per on/off cycle
const word UndervoltageRestartDelay = 5000; // mS - ignored when AUTO_RESTART_ON_UNDERVOLTAGE_WITH_ATX_PSUON_ASSERTED define is commented out

// working values
volatile bool UndervoltageSensed = false;

// predefined values based on undervoltage and overvoltage protection parameters
#if !defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
const byte PowerOnState = HIGH; // if LOW, a pullup resistor is required, otherwise, a pulldown resistor is required
const byte PowerOffState = LOW; // if HIGH, a pullup resistor is required, otherwise, a pulldown resistor is required
#endif

void OvervoltageIRQ() {
  delayMicroseconds(OVP_UVP_DebounceDelay);
  if (((OvervoltageIRQtype == LOW || OvervoltageIRQtype == FALLING) && digitalRead(OVP_pin) == LOW)
      || ((OvervoltageIRQtype == HIGH || OvervoltageIRQtype == RISING) && digitalRead(OVP_pin) == HIGH)) {
#if defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
    pinMode(PowerEnable_pin, INPUT); // float it
#else
    digitalWrite(PowerEnable_pin, PowerOffState);
#endif
    pinMode(ATX_PG_pin, OUTPUT); // sink it
    detachInterrupt(digitalPinToInterrupt(OVP_pin));
    while (true) { // loop until power is removed to indicate a unit which needs to be taken out of service due to overvoltage protection state
      FlashLED(OvervoltageLEDcycleRate, false);
    }
  }
}

void UndervoltageIRQ() {
  delayMicroseconds(OVP_UVP_DebounceDelay);
  if (((UndervoltageIRQtype == LOW || UndervoltageIRQtype == FALLING) && digitalRead(UVP_pin) == LOW)
      || ((UndervoltageIRQtype == HIGH || UndervoltageIRQtype == RISING) && digitalRead(UVP_pin) == HIGH)) {
#if defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
    pinMode(PowerEnable_pin, INPUT); // float it
#else
    digitalWrite(PowerEnable_pin, PowerOffState);
#endif
    pinMode(ATX_PG_pin, OUTPUT); // sink it
    UndervoltageSensed = true;
    detachInterrupt(digitalPinToInterrupt(UVP_pin));
  }
}

void delayTimerless(word value, bool StandbyForPowerOn) {
  for (word i = 0; i < value; i++) {
    if (StandbyForPowerOn == true && digitalRead(ATX_nPSON_pin) == LOW) {
      break;
    }
    delayMicroseconds(1000);
  }
}

void FlashLED(word TimeBetweenCycles, bool StandbyForPowerOn) {
  delayTimerless(TimeBetweenCycles, StandbyForPowerOn);
  digitalWrite(StatusLED, HIGH);
  delayTimerless(TimeBetweenCycles, StandbyForPowerOn);
  digitalWrite(StatusLED, LOW);
}

void UndervoltageFault() {
#if defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
  pinMode(PowerEnable_pin, INPUT); // float it
#else
  digitalWrite(PowerEnable_pin, PowerOffState);
#endif
  pinMode(ATX_PG_pin, OUTPUT); // sink it
#if defined(AUTO_RESTART_ON_UNDERVOLTAGE_WITH_ATX_PSUON_ASSERTED)
  for (int i = 0; i < (UndervoltageRestartDelay / UndervoltageLEDcycleRate / 2); i++) {
    FlashLED(UndervoltageLEDcycleRate, false);
  }
#else
  while (digitalRead(ATX_nPSON_pin) == LOW) {
    FlashLED(UndervoltageLEDcycleRate, false);
  }
#endif
  while (digitalRead(ATX_nPSON_pin) == HIGH) {
    if (digitalRead(ATX_nPSON_pin) == LOW) {
      break;
    }
    FlashLED(UndervoltageLEDcycleRate, true);
  }
}

void setup() {
  digitalWrite(StatusLED, LOW);
  digitalWrite(ATX_PG_pin, LOW); // open collector
  pinMode(OVP_pin, INPUT_PULLUP);
  pinMode(UVP_pin, INPUT_PULLUP);
  pinMode(ATX_nPSON_pin, INPUT_PULLUP);
  pinMode(ATX_PG_pin, OUTPUT); // sink it
  pinMode(StatusLED, OUTPUT);
#if defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
  digitalWrite(PowerEnable_pin, LOW); // sink it if it is an output
  pinMode(PowerEnable_pin, INPUT); // float it
#else
  digitalWrite(PowerEnable_pin, PowerOffState);
  pinMode(PowerEnable_pin, OUTPUT);
#endif
  attachInterrupt(digitalPinToInterrupt(OVP_pin), OvervoltageIRQ, OvervoltageIRQtype);
}

void loop() {
  detachInterrupt(digitalPinToInterrupt(UVP_pin));
  UndervoltageSensed = false;
  while (digitalRead(ATX_nPSON_pin) == HIGH) { // wait until system asserts the ATX /PSU_ON signal
  }
  delayTimerless(PSON_OVP_UVP_DebounceDelay, false);
  if (digitalRead(ATX_nPSON_pin) == LOW) {
#if defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
    pinMode(PowerEnable_pin, OUTPUT); // sink it
#else
    digitalWrite(PowerEnable_pin, PowerOnState);
#endif
    delayTimerless(UndervoltageSenseDelay, false);
    if (((UndervoltageIRQtype == LOW || UndervoltageIRQtype == FALLING) && digitalRead(UVP_pin) == LOW)
        || ((UndervoltageIRQtype == HIGH || UndervoltageIRQtype == RISING) && digitalRead(UVP_pin) == HIGH)) {
      delayMicroseconds(OVP_UVP_DebounceDelay);
      if (((UndervoltageIRQtype == LOW || UndervoltageIRQtype == FALLING) && digitalRead(UVP_pin) == LOW)
          || ((UndervoltageIRQtype == HIGH || UndervoltageIRQtype == RISING) && digitalRead(UVP_pin) == HIGH)) {
        UndervoltageSensed = true;
      }
    }
    if (UndervoltageSensed == false) {
      attachInterrupt(digitalPinToInterrupt(UVP_pin), UndervoltageIRQ, UndervoltageIRQtype);
      delayTimerless(PowerGoodDelay, false); // Power Good delay
      if (UndervoltageSensed == false) {
        pinMode(ATX_PG_pin, INPUT); // float it
      }
      digitalWrite(StatusLED, HIGH);
      while (true) { // wait until system negates the ATX /PSU_ON signal or when a fault occurs
        if (UndervoltageSensed == true) {
          break;
        }
        if (digitalRead(ATX_nPSON_pin) == HIGH) {
          delayTimerless(PSON_OVP_UVP_DebounceDelay, false);
          if (digitalRead(ATX_nPSON_pin) == HIGH) {
            break;
          }
        }
      }
      pinMode(ATX_PG_pin, OUTPUT); // sink it
#if defined(OPEN_COLLECTOR_LOW_ACTIVE_POWER_ENABLE)
      pinMode(PowerEnable_pin, INPUT); // float it
#else
      digitalWrite(PowerEnable_pin, PowerOffState);
#endif
      digitalWrite(StatusLED, LOW);
      if (UndervoltageSensed == true) { // undervoltage state may be causes by an overload or short or PSU defect but can be reset by negating the ATX /PSU_ON signal or automatically with AUTO_RESTART_ON_UNDERVOLTAGE_WITH_ATX_PSUON_ASSERTED define uncommented
        UndervoltageFault();
      }
    }
    else { // undervoltage fault
      UndervoltageFault();
    }
  }
}
