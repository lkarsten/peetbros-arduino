/*
Peet Bros. (PRO) anemometer interface.

Read the wind speed and direction interrupts from two input pins, do some math,
and output this over Serial as NMEA0183.

The anemometer itself is designed so that the two inputs does not signal
at the same time. We can use simple non-interruptable interrupts and still
be pretty sure not to lose any events.

Useful resources:
    http://arduino.cc/en/Tutorial/DigitalPins
    http://learn.parallax.com/reed-switch-arduino-demo
    http://www.agrolan.co.il/UploadProductFiles/AWVPRO.pdf

http://www.peetbros.com/shop/item.aspx?itemid=137

Connectors used:
* GND - the two ground wires towards the anemometer.
* pin 2 - rotating pin on anemometer.
* pin 3 - direction pin on anemometer.

Remaining:
* power saving, it uses ~40mA on an Uno now.

Author: Lasse Karstensen <lasse.karstensen@gmail.com>, February 2015.
*/
#include <limits.h>

#define REPORT_PERIOD 1000

volatile unsigned int rotation_took0;
volatile unsigned int rotation_took1;
volatile signed int direction_latency0;
volatile signed int direction_latency1;
volatile unsigned long last_rotation_at = millis();
unsigned long last_report = millis();

void setup() {
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(0, isr_rotated, RISING); // pin2 is int0.
  attachInterrupt(1, isr_direction, RISING); // pin3 is int1.

  rotation_took0 = 0;
  rotation_took1 = 0;
  direction_latency0 = 0;
  direction_latency1 = 0;
 
  Serial.begin(115200);
}

void print_debug() {
  Serial.print("since last_rot: ");
  Serial.print(millis() - last_rotation_at); Serial.print("ms; ");
  Serial.print("last dur: ");
  Serial.print(rotation_took0); Serial.print("ms; ");
  Serial.print("wspeed: ");
  Serial.print(wspeed_to_real()); Serial.print("kts; ");
  Serial.print("last dir_lat: ");
  Serial.print(direction_latency0); Serial.print("ms; ");
  Serial.print("wdir: ");
  Serial.print(wdir_to_degrees()); Serial.print(" degrees; ");
  Serial.println();
}

bool valid_sensordata() {
  // XXX: wrapping timers?
  if (last_rotation_at + 10*1000 < millis()) return(false);
  else return(true);
}  
  

float wspeed_to_real() {
  float mph = NAN;

  if (!valid_sensordata()) return(NAN);
  
  // avoid rewriting documented formulas.
  float r0 = 1.0 / (rotation_took0 / 1000.0);
  float r1 = 1.0 / (rotation_took1 / 1000.0);
  
  if (r0 < 0.010) mph = 0.0;
  else if (r0 < 3.229) mph = -0.1095*r1 + 2.9318*r0 - 0.1412;
  else if (r0 < 54.362) mph = 0.0052*r1 + 2.1980*r0 + 1.1091;
  else if (r0 < 66.332) mph = 0.1104*r1 - 9.5685*r0 + 329.87;
  
  if (isinf(mph) || isnan(mph) || mph < 0.0) return(NAN);
  
  float meters_per_second = mph * 0.48037;
  float knots = mph * 0.86897;
  
  return(knots);
}

float wdir_to_degrees() {
  float windangle;
  
  if (!valid_sensordata()) return(NAN);
  if (direction_latency0 < 0) return(NAN);
  
  float avg_rotation_time = ((float(rotation_took0) + float(rotation_took1)) / 2.0);
  // Serial.println(); Serial.print(avg_rotation_time);
  
  float phaseshift = float(direction_latency0) / avg_rotation_time;
  // Serial.print(" ms; "); Serial.print(phaseshift);
  
  if (isnan(phaseshift) || isinf(phaseshift)) windangle = NAN;
  else if (phaseshift == 0.0) windangle = 360.0;
  else if (phaseshift > 0.99) windangle = 360.0;
  else windangle = 360.0 * phaseshift;
  
  // Serial.print( " or "); Serial.print(awa); Serial.println(" degrees."); 
  return(windangle);
}



void loop() {
  // The interrupts will update the global counters. Report what we know.
  unsigned long t0 = millis();
  
  noInterrupts();
  // print_debug();
  float wspeed = wspeed_to_real();
  float awa = wdir_to_degrees();
  interrupts();
  
  Serial.print("windspeed="); Serial.print(wspeed);
  Serial.print(";direction="); Serial.print(awa);
  Serial.println(";");

  delay(REPORT_PERIOD - (millis() - t0));
}

/*
 * Interrupt called when the rotating part of the wind instrument
 * has completed one rotation.
 * 
 * We can find the wind speed by calculating how long the complete
 * rotation took.
*/
void isr_rotated() {
  unsigned long now = millis();
  unsigned int last_rotation_took;

  // Handle wrapping counters.
  if (now < last_rotation_at)
    last_rotation_took = now + (ULONG_MAX - last_rotation_at);
  else
    last_rotation_took = now - last_rotation_at;

  // I'd love to log this somewhere, but no Serial inside an ISR.
  if (last_rotation_took < 0)
    last_rotation_took = 0;

  // spurious interrupt? ignore it.
  // (these are probably an artifact of the push button used for development)
  if (last_rotation_took < 2) return;

  last_rotation_at = now;

  rotation_took1 = rotation_took0;
  rotation_took0 = last_rotation_took;
  
  direction_latency1 = direction_latency0;
  direction_latency0 = -1;
}

/*
 * After the rotation interrupt happened, the rotating section continues
 * into the next round. Somewhere within that rotation we get a different
 * interrupt when a certain point of the wind wane/direction indicator is
 * passed. By counting how far into the rotation (assumed to be == last rotation)
 * we can compute the angle.
 *
*/
void isr_direction() {
  unsigned long now = millis();
  unsigned int direction_latency;

  if (now < last_rotation_at)
    direction_latency = now + (ULONG_MAX - last_rotation_at);
  else
    direction_latency = now - last_rotation_at;
    
  direction_latency0 = direction_latency;
}
