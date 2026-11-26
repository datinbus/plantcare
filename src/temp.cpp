
#include <Arduino.h>
#include <DHT.h>
#include <math.h>

// ===== 1. CONFIGURACION =====

const int PIN_DHT = 3;
const unsigned long INTERVALO_MS = 2000;

// Crear el objeto del sensor
DHT dht(PIN_DHT, DHT11);

// ===== 2. INICIALIZACION =====

void setup() {
  Serial.begin(9600);
  dht.begin();

  Serial.println("PlantCare - P2 iniciado");
}

// ===== 3. PROGRAMA PRINCIPAL =====

void loop() {

  delay(INTERVALO_MS);

  // Obtener las mediciones
  float temperatura = dht.readTemperature();
  float humedadAire = dht.readHumidity();

  // Detectar fallos del sensor
  if (isnan(temperatura) || isnan(humedadAire)) {
    Serial.println("ERROR: lectura del DHT11");
    return;
  }

  // Mostrar temperatura
  Serial.print("Temperatura: ");
  Serial.print(temperatura, 1);
  Serial.print(" C");

  // Mostrar humedad ambiental
  Serial.print(" | Humedad: ");
  Serial.print(humedadAire, 1);
  Serial.println(" %");
}
