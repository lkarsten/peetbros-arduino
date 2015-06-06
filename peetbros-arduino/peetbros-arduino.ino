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

#define SAMPLE_WINDOW 8
#define REPORT_PERIOD 1000

// All time deltas are in milliseconds.
struct sample {
  unsigned int rotation_took;
  unsigned int direction_latency;
  struct sample *prev;
};

struct sample samples[SAMPLE_WINDOW];
volatile unsigned long last_rotation_at = millis();
volatile unsigned int sample_index = 0;
unsigned long last_report = millis();

void setup() {
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  attachInterrupt(0, isr_rotated, RISING); // pin2 is int0.
  attachInterrupt(1, isr_direction, RISING); // pin3 is int1.

  // Null everything out initially.
  for (int i = 0; i < SAMPLE_WINDOW; i++) {
    samples[i].rotation_took = 0;
    samples[i].direction_latency = 0;
    if (i == 0)
      samples[i].prev = &samples[SAMPLE_WINDOW];
    else
      samples[i].prev = &samples[i - 1];
  }
  
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

void debug_samples(struct sample samples[]) {
  char foo[200];
  int i;
  for (i = 0; i < SAMPLE_WINDOW; i++) {
    snprintf((char *)&foo, 200, "%i %i %i %s", i,
             samples[i].rotation_took,
             samples[i].direction_latency,
             sample_index == i ? "current" : "");
    Serial.println(foo);
  }
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


void compute_averages(int depth, struct sample *avg) {
  // Remember to disable interrupts before calling.
  struct sample *curr;
  // = &samples[sample_index];

  int i = 0;
  for (i = 0; i < depth; i++) {
    curr = &samples[i];

    avg->rotation_took += curr->rotation_took;
    avg->direction_latency += curr->direction_latency;

    curr = curr->prev;
    if (curr == NULL) break;
  }
  avg->rotation_took /= i;
  avg->direction_latency /= i;
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
  struct sample averages = {0, 0, NULL};
  
  noInterrupts();
  //debug_samples(samples);
  //compute_averages(3, &averages);
  print_debug(&samples[sample_index]);
  interrupts();
   
  //Serial.println("direction=100.2;speed=5.00;");
    // magic constants everywhere. this is in knots.
  /* int current_windspeed = 6000; // divide by 1000 to get real value in knots.
  int normalised_direction = 800; // parts per thousand.
  current_windspeed = trunc(1000 * 1000 * (1.0 / averages.rotation_took));
    // float normalised_direction = float(averages.direction_latency) / float(averages.rotation_took); // XXX
  /*  180 er 0.32?
      ca 210 var 0.41?
      ca 160 var 0.26
      anta: 090 er 0.01? */
  //output_nmea(current_windspeed, normalised_direction);
  //norm_to_degrees(normalised_direction));
 
  delay(REPORT_PERIOD - (millis() - t0));
}

/*
 * Interrupt called when the rotating part of the wind instrument
 * has completed one rotation.
 * 
 * We can find the wind speed by calculating how long the complete
 * rotation took.
 *
 * Alters global variable:
 *   last_rotation_at
 *   sample_index
 *   samples[sample_index]
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

  if (sample_index + 1 < SAMPLE_WINDOW)
    sample_index += 1;
  else
    sample_index = 0;

  samples[sample_index].rotation_took = last_rotation_took;
  samples[sample_index].direction_latency = 0; // Clean out old value.
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

  samples[sample_index].direction_latency = direction_latency;
}
