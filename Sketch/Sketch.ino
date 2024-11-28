int tmpPin = A0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  float PM25;
  float PM10;
  float NO2;
  float Temp;
  
  float reading = analogRead(tmpPin);  

  // These numbers are from the manufacturer
  float B = 3977;
  float T0 = 298.15;
  
  // Temperature in kelvin based on the Steinhart-Hart equation
  Temp = 1 / (1 / T0 + 1 / B * log((1023.0 / reading) - 1));

  // Convert to celcius
  Temp = Temp - 273.15;

  // Print in the csv format: PM25,PM10,NO2,Temp
  // right now we only have a temp sensor, so we will
  // print 0 for the rest
  Serial.print(PM25);
  Serial.print(",");
  Serial.print(PM10);
  Serial.print(",");
  Serial.print(NO2);
  Serial.print(",");
  Serial.println(Temp);

  delay(1000);
}
