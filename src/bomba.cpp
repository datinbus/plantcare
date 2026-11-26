
#include <Arduino.h>

const int bomba = 7;
const unsigned long duracion = 9500;

bool regando = false;
unsigned long inicio = 0;

void setup() {
    digitalWrite(bomba, HIGH);
    pinMode(bomba, OUTPUT);
    Serial.begin(9600);

    Serial.println("PLANTCARE - P8");
    Serial.println("1 = Regar");
    Serial.println("0 = Detener");
}

void loop() {
    if (Serial.available() > 0) {
        char comando = Serial.read();

        if (comando == '1' && !regando) {
            inicio = millis();
            regando = true;
            digitalWrite(bomba, LOW);

            Serial.println("Bomba encendida");
        }

        if (comando == '0') {
            digitalWrite(bomba, HIGH);
            regando = false;

            Serial.println("Parada manual");
        }
    }

    if (regando && millis() - inicio >= duracion) {
        digitalWrite(bomba, HIGH);
        regando = false;

        Serial.println("Riego terminado");
        Serial.print("Tiempo transcurrido: ");
        Serial.print(millis() - inicio);
        Serial.println(" ms");
    }
}
