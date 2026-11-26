
#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <LiquidCrystal_I2C.h>

// Pines
const int LED_ALERTA = 6;
const int PIN_DHT = 3;

// Sensores
DHT dht(PIN_DHT, DHT11);
BH1750 sensorLuz;

// RTC: DAT, CLK, RST
ThreeWire conexion(10, 11, 9);
RtcDS1302<ThreeWire> rtc(conexion);

// Pantalla LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Limites ambientales
const float TEMP_MIN = 15.0;
const float TEMP_MAX = 32.0;
const float HUM_MIN = 40.0;
const float HUM_MAX = 80.0;
const float LUZ_MIN = 100.0;

// Horario diurno
const int HORA_INICIO = 8;
const int HORA_FIN = 20;

// Variables de alertas
const char* alertas[8];
int cantidadAlertas = 0;
int indiceAlerta = 0;

bool luzDisponible;

// Agregar una alerta
void agregarAlerta(const char* mensaje) {
    if (cantidadAlertas < 8) {
        alertas[cantidadAlertas] = mensaje;
        cantidadAlertas++;
        Serial.println(mensaje);
    }
}

// Limpiar y escribir una fila de la LCD
void escribirFila(int fila, const char* mensaje) {
    lcd.setCursor(0, fila);
    lcd.print("                ");
    lcd.setCursor(0, fila);
    lcd.print(mensaje);
}

void setup() {
    Serial.begin(9600);

    pinMode(LED_ALERTA, OUTPUT);
    digitalWrite(LED_ALERTA, LOW);

    dht.begin();
    Wire.begin();

    luzDisponible = sensorLuz.begin(
        BH1750::CONTINUOUS_HIGH_RES_MODE
    );

    // Inicializar LCD
    lcd.init();
    lcd.backlight();

    escribirFila(0, "   PLANTCARE");
    escribirFila(1, "Iniciando...");

    // Inicializar RTC
    rtc.Begin();
    rtc.SetIsWriteProtected(false);
    rtc.SetTrickleChargeSettings(DS1302Tcr_Disabled);

    RtcDateTime compilacion(__DATE__, __TIME__);

    if (!rtc.GetIsRunning()) {
        rtc.SetIsRunning(true);
    }

    if (!rtc.IsDateTimeValid() ||
        rtc.GetDateTime() < compilacion) {
        rtc.SetDateTime(compilacion);
    }

    delay(2000);
}

void loop() {

    cantidadAlertas = 0;

    // Lecturas de sensores
    float temperatura = dht.readTemperature();
    float humedad = dht.readHumidity();

    float lux = luzDisponible
        ? sensorLuz.readLightLevel() : -1.0;

    RtcDateTime ahora = rtc.GetDateTime();

    bool rtcValido = rtc.IsDateTimeValid()
        && rtc.GetIsRunning();

    int hora = ahora.Hour();

    bool esDeDia = rtcValido &&
                   hora >= HORA_INICIO &&
                   hora < HORA_FIN;

    // Monitor serie
    Serial.println("--------------------");

    Serial.print("Hora: ");
    Serial.print(hora);
    Serial.print(":");

    if (ahora.Minute() < 10) {
        Serial.print("0");
    }

    Serial.print(ahora.Minute());
    Serial.print(":");

    if (ahora.Second() < 10) {
        Serial.print("0");
    }

    Serial.println(ahora.Second());

    Serial.print("Periodo: ");
    Serial.println(rtcValido
        ? (esDeDia ? "DIA" : "NOCHE")
        : "DESCONOCIDO");

    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" C");

    Serial.print("Humedad: ");
    Serial.print(humedad);
    Serial.println(" %");

    Serial.print("Luminosidad: ");
    Serial.print(lux);
    Serial.println(" lux");

    // Temperatura y humedad
    if (isnan(temperatura) || isnan(humedad)) {

        agregarAlerta("Error DHT11");

    } else {

        if (temperatura > TEMP_MAX) {
            agregarAlerta("Temperatura alta");
        }

        if (temperatura < TEMP_MIN) {
            agregarAlerta("Temperatura baja");
        }

        if (humedad > HUM_MAX) {
            agregarAlerta("Humedad alta");
        }

        if (humedad < HUM_MIN) {
            agregarAlerta("Humedad baja");
        }
    }

    // Comprobar RTC
    if (!rtcValido) {
        agregarAlerta("Error RTC");
    }

    // Luminosidad
    if (lux < 0 || isnan(lux)) {

        agregarAlerta("Error BH1750");

    } else if (esDeDia && lux < LUZ_MIN) {

        agregarAlerta("Falta de luz");
    }

    // Control del LED y pantalla
    if (cantidadAlertas == 0) {

        digitalWrite(LED_ALERTA, LOW);

        escribirFila(0, "PLANTCARE OK");
        escribirFila(1, "Sin alertas");

        indiceAlerta = 0;

        Serial.println("SISTEMA NORMAL");

    } else {

        digitalWrite(LED_ALERTA, HIGH);

        // Encabezado de la alerta
        if (cantidadAlertas == 1) {

            escribirFila(0, "! ALERTA !");

        } else {

            char encabezado[17];

            snprintf(encabezado, sizeof(encabezado),
                     "! ALERTA ! (%d)", cantidadAlertas);

            escribirFila(0, encabezado);
        }

        // Alternar motivos de alerta
        if (indiceAlerta >= cantidadAlertas) {
            indiceAlerta = 0;
        }

        escribirFila(1, alertas[indiceAlerta]);

        indiceAlerta++;

        Serial.print("Total de alertas: ");
        Serial.println(cantidadAlertas);
    }

    delay(2000);
}
