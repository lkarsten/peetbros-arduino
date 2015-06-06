/*
Peet Bros. anemometer interface.

Author: Lasse Karstensen <lasse.karstensen@gmail.com>, February 2015.
*/

#include <limits.h>
#include <assert.h>

volatile unsigned long last_rotation_at = millis();
volatile unsigned long last_rotation_took = 100; // 0.5; // Picked at random.
volatile unsigned long direction_latency = 50;  // Picked at random.

void setup() {
  Serial.begin(57600);
  // Not sure if this is right.
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(0, isr_rotated, RISING); // normally open.
  attachInterrupt(1, isr_direction, RISING); // normally closed.
}

void print_debug() {
  Serial.print(" since last rotation: ");
  Serial.print(millis() - last_rotation_at);
  Serial.print(" last rotation took: ");
  Serial.println(last_rotation_took);
}

void loop() {
  // The interrupts will update the global counters. Report what we know.
  unsigned long t0 = millis();

  noInterrupts();
  float current_windspeed = 500.0 * (1.0 / last_rotation_took); // magic constants everywhere. knots.
  float normalised_direction = last_rotation_took / direction_latency;
  interrupts();
  
  // assert(current_windspeed < 300)
  // assert(normalised_direction >= 0.0);
  // assert(normalised_direction <= 1.0);
  
  Serial.print(" speed=");
  Serial.print(current_windspeed); // Implicit truncation to %.2f.
  Serial.print(" direction=");
  Serial.print(normalised_direction);
  print_debug();

  // sleep until the end of the second.
  delay(1e3 - (millis() - t0));
}

void isr_rotated() {
  unsigned long now = millis();
  if (now < last_rotation_at)
    last_rotation_took = now + (ULONG_MAX - last_rotation_at);
  else
    last_rotation_took = now - last_rotation_at;
  last_rotation_at = now;
}

void isr_direction() {
  // direction is a function of time since last rotation interrupt.
  unsigned long now = millis();

  if (now < last_rotation_at)
    direction_latency = now + (ULONG_MAX - last_rotation_at);
  else
    direction_latency = now - last_rotation_at;
}

