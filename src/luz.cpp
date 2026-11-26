
#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>

// Objeto del sensor de luz
BH1750 sensorLuz;

void setup() {
    Serial.begin(9600);
    Wire.begin();

    Serial.println("PlantCare - P5");

    // Inicializar sensor
    if (sensorLuz.begin()) {
        Serial.println("Sensor conectado correctamente");
    } else {
        Serial.println("ERROR: Sensor no detectado");

        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    // Medir luminosidad
    float lux = sensorLuz.readLightLevel();

    if (lux >= 0) {
        Serial.print("Luminosidad: ");
        Serial.print(lux);
        Serial.println(" lx");
    } else {
        Serial.println("Error de lectura");
    }

    delay(1000);
}
