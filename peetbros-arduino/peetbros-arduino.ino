/*
Peet Bros. anemometer interface.

Read the wind speed and direction from two input pins, do some math, and
output this over Serial as NMEA0183.

The anemometer itself is designed so that the two inputs does not signal
at the same time. We can use simple non-interruptable interrupts and still
be sure not to lose any events.

Useful resources:
    http://arduino.cc/en/Tutorial/DigitalPins
    http://learn.parallax.com/reed-switch-arduino-demo
    http://www.agrolan.co.il/UploadProductFiles/AWVPRO.pdf

Remaining:
* power saving, it uses ~40mA on an Uno now.

Author: Lasse Karstensen <lasse.karstensen@gmail.com>, February 2015.
*/

#include <limits.h>
#include <assert.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define SAMPLE_WINDOW 30

struct sample {
  float rotation_took;
  float direction_latency;
  struct sample *prev;
} samples[SAMPLE_WINDOW];

volatile unsigned long last_rotation_at = millis();
volatile unsigned sample_index = 0;

void setup() {
  // Null everything out initially.
  for (int i = 0; i < SAMPLE_WINDOW; i++) {
    samples[i].rotation_took = 0.0;
    samples[i].direction_latency = 0.0;
    if (i == 0)
      samples[i].prev = &samples[SAMPLE_WINDOW];
    else
      samples[i].prev = &samples[i - 1];
  }
  Serial.begin(57600);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(0, isr_rotated, FALLING); // normally closed.
  attachInterrupt(1, isr_direction, RISING); // normally open.
}

void do_sleep() {
  // Enable power saving. Without any modifications is uses 0.255W/0.204W.
  set_sleep_mode(SLEEP_MODE_IDLE);
  power_adc_disable();
  power_spi_disable();
  power_twi_disable();
  sleep_enable(); // just a flag.
  sleep_mode();
  // zZz here until an interrupt wakes us.
  sleep_disable();
}

void print_debug(struct sample *s) {
  Serial.print(" since last rotation: ");
  Serial.print(millis() - last_rotation_at);
  Serial.print("ms; ");
  Serial.print("sampleindex: "); Serial.print(sample_index);
  Serial.print(" last duration: ");
  Serial.print(s->rotation_took);
  Serial.print("ms; last dir_latency: ");
  Serial.print(s->direction_latency);
  Serial.print("ms; ");
  Serial.println();
}
/*
   Serial.println();
   Serial.print(" speed=");
   Serial.print(current_windspeed); // Implicit truncation to %.2f.
   Serial.print(" direction=");
   Serial.print(normalised_direction, 4);
   */
//Serial.print("norm. dir: ");
//Serial.println(float(direction_latency) / float(last_rotation_took));
// 1166 av 1879ms er ca. 350 grader app.
// 370 grader er
//}

void output_nmea(float wspeed, float wdirection) {
  Serial.print("$$MWV,");
  Serial.print(wdirection);
  Serial.print(",R,");
  Serial.print(wspeed);
  Serial.println(",K,A");
  //  Serial.println("*hh");
}

int norm_to_degrees(float norm) {
  float td;
  // Do ourselves the service of representing 000 degrees as 360.
  td = norm * 360.0;
  if (td < 1.0) td += 360.0;
  return (td);
}

void compute_averages(byte depth, struct sample *avg) {
  struct sample *curr = &samples[sample_index];

  for (int i = 0; i++; i < depth) {
    avg->rotation_took += curr->rotation_took;
    avg->direction_latency += curr->direction_latency;
    curr = curr->prev;
  }
  avg->rotation_took /= float(depth);
  avg->direction_latency /= float(depth);
}

// All report periods in milliseconds.
int report_period_min = 500;
int report_period_max = 5000; // ms
unsigned long last_report = millis();

void loop() {
  // The interrupts will update the global counters. Report what we know when there is something new.
  unsigned long t0 = millis();
  struct sample averages;

  //noInterrupts();
  //compute_averages(3, &averages);
  
  //interrupts();

  // magic constants everywhere. this is in knots.
  float current_windspeed = 1000.0 * (1.0 / averages.rotation_took);
  float normalised_direction = averages.direction_latency / averages.rotation_took;
  /*
      180 er 0.32?
      ca 210 var 0.41?
      ca 160 var 0.26
      anta: 090 er 0.01?
      */
  //}
  //print_debug(&averages);
  //print_debug(&samples[sample_index]);
  
  output_nmea(norm_to_degrees(normalised_direction), current_windspeed);
  delay(500);
  // do_sleep();
}


void isr_rotated() {
  unsigned long now = millis();
  unsigned int last_rotation_took;

  // Handle wrapping.
  if (now < last_rotation_at)
    last_rotation_took = now + (ULONG_MAX - last_rotation_at);
  else
    last_rotation_took = now - last_rotation_at;
  // I'd love to log this somewhere, but ISR...
  if (last_rotation_took < 0.0)
    last_rotation_took = 0.0;

  last_rotation_at = now;

  if (sample_index + 1 < SAMPLE_WINDOW)
    sample_index += 1;
  else
    sample_index = 0;

  samples[sample_index].rotation_took = last_rotation_took;
  samples[sample_index].direction_latency = 0.0;
}

void isr_direction() {
  // direction is a function of time since last rotation interrupt.
  unsigned long now = millis();
  unsigned int direction_latency;

  if (now < last_rotation_at)
    direction_latency = now + (ULONG_MAX - last_rotation_at);
  else
    direction_latency = now - last_rotation_at;

  samples[sample_index],direction_latency = direction_latency;
}

