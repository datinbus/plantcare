#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Perfiles: horas de luz y mm de agua por semana (referencias).
// maxLuz = 0 significa "o mas". No son ordenes de riego.
struct Perfil {
    const char* nombre;
    byte minLuz, maxLuz;
    float aguaMin, aguaMax;
};
const Perfil plantas[] = {
    {"Albahaca", 6, 8, 25.4, 38.1},
    {"Petunia",  6, 0, 25.4, 50.8},
    {"Menta",    4, 6, 25.4, 50.8},
    {"Tomate",   6, 8, 25.4, 50.8}
};
const byte N_PLANTAS = sizeof(plantas) / sizeof(plantas[0]);
const float DIAMETRO_CM = 20.0; // Cambiar por el diametro interior real.
const byte FIRMA = 0xA7;
byte plantaActiva = 1; // Petunia por defecto.
byte candidata = 1;

const char* menu[] = {
    "Estado general", "Humedad suelo", "Ambiente", "Luz diaria",
    "Agua diaria", "Agua semanal", "Estado riego", "Elegir planta"
};
const byte N_MENU = sizeof(menu) / sizeof(menu[0]);
byte opcion = 0;
enum Pantalla { MENU, DATOS, SELECCION, CONFIRMACION };
Pantalla pantalla = MENU;

// Datos de prueba. No proceden de sensores reales.
int humedadSuelo = 42;
int temperatura = 24;
int humedadAire = 58;
float luzHoy = 3.2;
unsigned int aguaHoy = 80;
unsigned int aguaSemana = 420;
bool bombaActiva = false;

struct Boton {
    byte pin;
    bool lecturaAnterior, estable;
    unsigned long ultimoCambio;
};
Boton anterior = {2, HIGH, HIGH, 0};
Boton siguiente = {4, HIGH, HIGH, 0};
Boton aceptar = {5, HIGH, HIGH, 0};

bool pulsado(Boton &b) {
    bool lectura = digitalRead(b.pin);
    if (lectura != b.lecturaAnterior) {
        b.lecturaAnterior = lectura;
        b.ultimoCambio = millis();
    }
    if (millis() - b.ultimoCambio >= 30 && lectura != b.estable) {
        b.estable = lectura;
        return lectura == LOW;
    }
    return false;
}

// Convertir mm de agua a mL sobre una superficie circular.
unsigned int estimarML(float mm) {
    float radioM = DIAMETRO_CM / 200.0;
    return (unsigned int)(mm * 3.14159265 * radioM * radioM * 1000.0 + 0.5);
}

void mostrarAgua(bool diario) {
    const Perfil &p = plantas[plantaActiva];
    float divisor = diario ? 7.0 : 1.0;
    lcd.print(diario ? F("Hoy:") : F("Sem:"));
    lcd.print(diario ? aguaHoy : aguaSemana);
    lcd.print(F("mL DEMO"));
    lcd.setCursor(0, 1);
    lcd.print(F("Est:"));
    lcd.print(estimarML(p.aguaMin / divisor));
    lcd.print('-');
    lcd.print(estimarML(p.aguaMax / divisor));
    lcd.print(F("mL"));
}

void mostrar() {
    lcd.clear();
    lcd.setCursor(0, 0);
    const Perfil &p = plantas[plantaActiva];

    if (pantalla == MENU) {
        lcd.print(F("MENU "));
        lcd.print(opcion + 1);
        lcd.print('/');
        lcd.print(N_MENU);
         lcd.setCursor(0, 1);
        lcd.print('>');
        lcd.print(menu[opcion]);
    } else if (pantalla == SELECCION) {
        lcd.print(F("ELEGIR PLANTA"));
        lcd.setCursor(0, 1);
        lcd.print(F("< "));
        lcd.print(plantas[candidata].nombre);
        lcd.print(F(" >"));
    } else if (pantalla == CONFIRMACION) {
        lcd.print(F("PLANTA GUARDADA"));
        lcd.setCursor(0, 1);
        lcd.print(p.nombre);
    } else {
        switch (opcion) {
            case 0:
                lcd.print(F("PLANTCARE DEMO"));
                lcd.setCursor(0, 1);
                lcd.print(p.nombre);
                break;
            case 1:
                lcd.print(F("HUMEDAD SUELO"));
                lcd.setCursor(0, 1);
                lcd.print(humedadSuelo);
                lcd.print(F("% DEMO"));
                break;
            case 2:
                lcd.print(F("T:"));
                lcd.print(temperatura);
                lcd.print(F("C DEMO"));
                lcd.setCursor(0, 1);
                lcd.print(F("Aire:"));
                lcd.print(humedadAire);
                lcd.print(F("% DEMO"));
                break;
            case 3:
                lcd.print(F("Hoy:"));
                lcd.print(luzHoy, 1);
                lcd.print(F("h DEMO"));
                lcd.setCursor(0, 1);
                lcd.print(F("Ref:"));
                lcd.print(p.minLuz);
                if (p.maxLuz == 0) {
                    lcd.print(F("h o mas"));
                } else {
                    lcd.print('-');
                    lcd.print(p.maxLuz);
                    lcd.print('h');
                    if (plantaActiva == 3) lcd.print('+');
                }
                break;
            case 4:
                mostrarAgua(true);
                break;
            case 5:
                mostrarAgua(false);
                break;
            case 6:
                lcd.print(F("ESTADO RIEGO"));
                lcd.setCursor(0, 1);
                lcd.print(bombaActiva ? F("Bomba: ON DEMO") : F("Bomba: OFF DEMO"));
                break;
        }
    }
}

void setup() {
    Serial.begin(9600);
    lcd.init();
    lcd.backlight();
    pinMode(anterior.pin, INPUT_PULLUP);
    pinMode(siguiente.pin, INPUT_PULLUP);
    pinMode(aceptar.pin, INPUT_PULLUP);

    byte guardada = EEPROM.read(1);
    if (EEPROM.read(0) == FIRMA && guardada < N_PLANTAS) {
        plantaActiva = guardada;
    }
    mostrar();
}

void loop() {
    bool izq = pulsado(anterior);
    bool der = pulsado(siguiente);
    bool ok = pulsado(aceptar);
    if (!izq && !der && !ok) return;

    switch (pantalla) {
        case MENU:
            if (ok) {
                if (opcion == N_MENU - 1) {
                    candidata = plantaActiva;
                    pantalla = SELECCION;
                } else {
                    pantalla = DATOS;
                }
            } else if (der) {
                opcion = (opcion + 1) % N_MENU;
            } else if (izq) {
                opcion = (opcion + N_MENU - 1) % N_MENU;
            }
            break;
        case SELECCION:
            if (ok) {
                plantaActiva = candidata;
                EEPROM.update(1, plantaActiva);
                EEPROM.update(0, FIRMA);
                Serial.print(F("Planta activa: "));
                Serial.println(plantas[plantaActiva].nombre);
                pantalla = CONFIRMACION;
            } else if (der) {
                candidata = (candidata + 1) % N_PLANTAS;
            } else if (izq) {
                candidata = (candidata + N_PLANTAS - 1) % N_PLANTAS;
            }
            break;
        case DATOS:
        case CONFIRMACION:
            if (ok) pantalla = MENU;
            break;
    }
    mostrar();
}
