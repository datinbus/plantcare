
#include <Arduino.h>

// Pines
const int sensor = A0;
const int rele = 7;

// Calibracion
const int seco = 1023;
const int humedo = 559;
const int umbral = 30;

// Tiempos
const unsigned long tiempoRiego = 2000;
const unsigned long tiempoAbsorcion = 30000;
const unsigned long intervaloLectura = 1000;

// Seguridad
const int maxPulsos = 3;
int contadorPulsos = 0;

// Control del tiempo
unsigned long inicioEstado = 0;
unsigned long ultimaLectura = 0;

// Estados
enum Estado {
    MONITOREO,
    REGANDO,
    ABSORCION,
    BLOQUEADO
};

Estado estado = MONITOREO;

void setup() {
    digitalWrite(rele, HIGH);
    pinMode(rele, OUTPUT);
    Serial.begin(9600);

    ultimaLectura = millis() - intervaloLectura;

    Serial.println("PlantCare P8.6 iniciado");
}

void loop() {
    unsigned long ahora = millis();

    // Reinicio manual mediante monitor serie
    if (Serial.available() > 0) {
        char comando = Serial.read();

        if ((comando == 'R' || comando == 'r')
            && estado == BLOQUEADO) {

            contadorPulsos = 0;
            estado = MONITOREO;

            Serial.println("Bloqueo REINICIADO");
        }
    }

    // Finalizar pulso de riego
    if (estado == REGANDO &&
        ahora - inicioEstado >= tiempoRiego) {

        digitalWrite(rele, HIGH);

        estado = ABSORCION;
        inicioEstado = ahora;

        Serial.println("Riego FINALIZADO");
        Serial.println("Iniciando absorcion");
    }

    // Finalizar absorcion
    if (estado == ABSORCION &&
        ahora - inicioEstado >= tiempoAbsorcion) {

        estado = MONITOREO;

        Serial.println("Absorcion FINALIZADA");
    }

    // Medicion cada segundo
    if (ahora - ultimaLectura >= intervaloLectura) {

        ultimaLectura = ahora;

        int lectura = analogRead(sensor);

        int humedad = map(
            lectura, seco, humedo, 0, 100
        );

        humedad = constrain(humedad, 0, 100);

        Serial.print("Lectura: ");
        Serial.print(lectura);

        Serial.print(" | Humedad: ");
        Serial.print(humedad);

        Serial.print("% | Pulsos: ");
        Serial.print(contadorPulsos);
        Serial.print("/");
        Serial.println(maxPulsos);

        // Tomar decisiones
        if (estado == MONITOREO) {

            if (humedad >= umbral) {

                contadorPulsos = 0;
                digitalWrite(rele, HIGH);

            } else if (contadorPulsos >= maxPulsos) {

                digitalWrite(rele, HIGH);
                estado = BLOQUEADO;

                Serial.println("ALERTA: RIEGO BLOQUEADO");
                Serial.println("Revisar sensor y deposito");
                Serial.println("Enviar R para reiniciar");

            } else {

                contadorPulsos++;

                digitalWrite(rele, LOW);

                estado = REGANDO;
                inicioEstado = ahora;

                Serial.println("Riego ACTIVADO");
            }
        }

        // Mantener bloqueo de seguridad
        if (estado == BLOQUEADO) {
            digitalWrite(rele, HIGH);
            Serial.println("Sistema BLOQUEADO");
        }
    }
}
