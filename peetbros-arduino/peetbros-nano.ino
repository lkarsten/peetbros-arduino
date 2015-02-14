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

Author: Lasse Karstensen <lasse.karstensen@gmail.com>, February 2015.
*/

#include <limits.h>
#include <assert.h>

#define SAMPLE_WINDOW 10

struct sample {
  float rotation_took;
  float direction_latency;
};
typedef struct sample sample_t;

volatile unsigned long last_rotation_at = millis();
volatile unsigned sample_index = 0;
sample_t samples[SAMPLE_WINDOW];

void setup() {
  Serial.begin(57600);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(0, isr_rotated, RISING); // normally closed.
  attachInterrupt(1, isr_direction, RISING); // normally open.
}

/*
void print_debug() {
  Serial.print(" since last rotation: ");
  Serial.print(millis() - last_rotation_at);
  Serial.print("ms; last duration: ");
  Serial.print(last_rotation_took);
  Serial.print("ms; last dir_latency: ");
  Serial.print(direction_latency);
  Serial.print("ms; ");
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

// All report periods in milliseconds.
int report_period_min = 500;
int report_period_max = 5000; // ms
unsigned long last_report = millis();

void loop() {
  // The interrupts will update the global counters. Report what we know when there is something new.
  unsigned long t0 = millis();

  // If nothing new has happened, and we're still under the
  if ((t0 - report_period_min) < last_report) {
    Serial.println("Nothing new to report, sleeping one period");
    delay(200);
  } else {

    /*
    if less than report_period_min since last output:
      * store the new sample to aggregate numbers.
      * return.
    else:
        * if report_period_max has passed since last output, output the last known numbers.
    */
    sample_t latest_sample;
    noInterrupts();
    latest_sample = samples[sample_index];
    interrupts();
    
    // magic constants everywhere. this is in knots.
    float current_windspeed = 1000.0 * (1.0 / latest_sample.rotation_took);
    
    float normalised_direction = latest_sample.direction_latency / latest_sample.rotation_took;
 
    /*
      if (last_rotation_at >= last_report)) {
        // Outdated sample.
      } else {
        print_debug();
        /*
        180 er 0.32?
        ca 210 var 0.41?
        ca 160 var 0.26
        anta: 090 er 0.01?
        */
    //}

    output_nmea(norm_to_degrees(normalised_direction), current_windspeed);

    // sleep until the end of the second.
    delay(report_period_min - (millis() - t0)); // XXX
  }
}

void isr_rotated() {
  unsigned long now = millis();
  unsigned int last_rotation_took;

  // Handle wrapping.
  if (now < last_rotation_at)
    last_rotation_took = now + (ULONG_MAX - last_rotation_at);
  else
    last_rotation_took = now - last_rotation_at;
  // I'd love to log this somewhere, but ISR.
  if (last_rotation_took < 0.0)
      last_rotation_took = 0.0;
      
  last_rotation_at = now;

  if (sample_index + 1 < SAMPLE_WINDOW)
    sample_index += 1;
  else
    sample_index = 0;

  samples[sample_index] = {last_rotation_took, NULL};
}

void isr_direction() {
  // direction is a function of time since last rotation interrupt.
  unsigned long now = millis();
  unsigned int direction_latency;

  if (now < last_rotation_at)
    direction_latency = now + (ULONG_MAX - last_rotation_at);
  else
    direction_latency = now - last_rotation_at;
    
  samples[sample_index] = { .rotation_took = samples[sample_index].rotation_took, 
                            .direction_latency = direction_latency };
}

