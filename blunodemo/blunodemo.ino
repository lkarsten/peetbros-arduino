void setup() {
  Serial.begin(115200);
}

void loop()
{
  int sent;
  if (Serial.available()) {
      while (Serial.available() > 0) Serial.read();
      Serial.println("direction=100.2;speed=5.00;");
    }
}
