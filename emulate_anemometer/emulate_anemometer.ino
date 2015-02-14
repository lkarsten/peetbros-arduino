/*
Use a second arduino to generate test signals
for the unit, so development can happen all cozy at
home instead of on board.

Two adjustables:
* time between pulldowns, indicating one rotation.
* delta_t after a rotation pulldown, which is which apparent angle the wind has.

Back of envelope calculations to get the sense of sizes:
* the radius of the rotating section is about 15cm.
* Specs say max 71 m/s.
* At max windspeed we should see one full rotation every 7ms, and
as such (with the direction signal) an event every 3.5ms.
* At 5m/s we should see an rotation every 100ms, event ever 50ms.

Based on empirical knowledge I think these rotational numbers are a bit on
the high side. (10 rounds per second in 5m/s?)

How quickly the Atmel chip can read is still open, must review specs. (at
least for the core, there should be plenty of instructions available
at 16MHz.)

Author: Lasse Karstensen <lasse.karstensen@gmail.com>, February 2015.
*/


void setup() {
  Serial.begin(57600);
  // put your setup code here, to run once:
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);  
  pinMode(A0, INPUT);
  pinMode(A2, INPUT);
}

void pulse(int port) {
  // volatile unsigned long t0 = millis();
  digitalWrite(port, HIGH);
  /* I have no sense of how long such a pulse should be.
  To reliably detect a pulse every 3.5ms, it should be
  2ms at the minimum. It is of course a strictly linear
  function of the rotation speed.
  */
  delay(2);
  digitalWrite(port, LOW);
  // volatile unsigned long t1 = millis();
  // Serial.println(t0 - t1);
// period = 100; // milliseconds
}

void loop() {
  int period = analogRead(A2);
  // ms. happens every period, but 180 degrees phase shifted. 
  double direction_delay = analogRead(A1) / 1024;
  
  // potmeter-specific adjustments.
 //  period = period - 780;
  // period *= 4;
  
  if (period < 6) period = 6;
  // if (period > 2000) period = 10000;
  
  // if (direction_delay < 6) direction_delay = 6;
  // if (direction_delay > 360) direction_delay = 
  // if (direction_delay >= period) direction_delay = period;

  Serial.print("period="); Serial.print(period);
  Serial.print(" direction="); Serial.println(direction_delay*period);
  
  pulse(2);
  delay(direction_delay*period);
  pulse(3);
  
  delay(max(period - (period*direction_delay), 0));
}
