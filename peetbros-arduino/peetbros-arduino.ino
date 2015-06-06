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
* calibration!

Author: Lasse Karstensen <lasse.karstensen@gmail.com>, February 2015.
*/

#include <limits.h>

#define REPORT_PERIOD 1000


// All time deltas are in milliseconds.
struct sample {
  unsigned int rotation_took;
  unsigned int direction_latency;
  struct sample *prev;
};

volatile unsigned int rotation_took0;
volatile unsigned int rotation_took1;
volatile unsigned int direction_latency0;
volatile unsigned int direction_latency1;
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

/*
void nmea_checksum(char *input, char *output) {
  char *ptr = input;
  int checksum = 0;
  
  while (*(ptr++) != '\0')
    checksum ^= *ptr;
  snprintf(output, 6, "*%02X\n", checksum);
}

void output_nmea(int wspeed, int wdirection) {
  char output[255];
  // char sumstr[6];
  snprintf(output, 255, "$$MWN,%i,R,%i,K,A", wdirection, wspeed);
  // nmea_checksum((char *)&output, (char *)&sumstr);
  // Serial.print("$");
  Serial.print(output);
  // Serial.print(sumstr);
}
*/

void print_debug() {
  Serial.print(" since last rotation: ");
  Serial.print(millis() - last_rotation_at);
  Serial.print("ms; ");
  Serial.print(" last duration: ");
  Serial.print(rotation_took0);
  Serial.print("ms; last dir_latency: ");
  Serial.print(direction_latency0);
  Serial.print("ms; ");
  Serial.println();
}

float timedelta_to_real() {
  float mph;
  
  if (rotation_took0 < 0.010) mph = 0.0;
  else if (rotation_took0 < 3.229) mph = -0.1095*rotation_took1 + 2.9318*rotation_took0 - 0.1412;
  else if (rotation_took0 < 54.362) mph = 0.0052*rotation_took1 + 2.1980*rotation_took0 + 1.1091;
  else if (rotation_took0 < 66.332) mph = 0.1104*rotation_took1 - 9.5685*rotation_took0 + 329.87;
  else mph = 0.0; // no idea
  
  float meters_per_second = mph * 0.48037;
  float knots = mph * 0.86897;
  
  Serial.print("real is: ");
  Serial.println(mph);
  return(knots);
}

float norm_to_degrees(unsigned int norm) {
  float td;
  // Do ourselves the service of representing 000 degrees as 360.
  td = float(norm) * 360.0;
  if (td < 1.0) td += 360.0;
  return (td);
}

void loop() {
  // The interrupts will update the global counters. Report what we know.
  unsigned long t0 = millis();  
  noInterrupts();
  //debug_samples(samples);
  //compute_averages(3, &averages);
  print_debug();
  timedelta_to_real();
  interrupts();
  /*  180 er 0.32?
      ca 210 var 0.41?
      ca 160 var 0.26
      anta: 090 er 0.01? */
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
  direction_latency0 = 0;
}

/*
 * After the rotation interrupt happened, the rotating section continues
 * into the next round. Somewhere within that rotation we get a different
 * interrupt when a certain point of the wind wane/direction indicator is
 * passed. By counting how far into the rotation (assumed to be == last rotation)
 * we can compute the angle.
 *
 * Alters global state:
 *   samples[sample_index].direction_latency
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
