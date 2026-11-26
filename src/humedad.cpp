#include <Arduino.h>

#include <DHT.h>

#define PIN_DHT 2
#define TIPO_DHT DHT11

DHT dht(PIN_DHT, TIPO_DHT);

void setup() {
  Serial.begin(9600);
  dht.begin();
}

void loop() {
  delay(2000); // el DHT11 necesita al menos 1 segundo entre lecturas

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Error: no se pudo leer el sensor");
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(temp);
  Serial.print(" C  |  Humedad: ");
  Serial.print(hum);
  Serial.println(" %");
}