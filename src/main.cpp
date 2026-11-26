
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Direccion I2C habitual: 0x27 (si no muestra nada, prueba 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int PIN_HUMEDAD = A0;

// Valores obtenidos de nuestra calibracion
const int ADC_SECO = 1023;
const int ADC_HUMEDO = 559;

const unsigned long INTERVALO_MS = 1000;

// ===== 2. INICIALIZACION =====

void setup() {
  Serial.begin(9600);
  Serial.println("PlantCare - P1");
  Serial.println("Monitoreo de humedad");

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("PlantCare - P1");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  delay(1500);
  lcd.clear();
}

// ===== 3. PROGRAMA PRINCIPAL =====

void loop() {
  // Leer la senal analogica del sensor
  int lectura = analogRead(PIN_HUMEDAD);

  // Convertir la lectura a porcentaje
  int humedad = map(
    lectura,
    ADC_SECO,
    ADC_HUMEDO,
    0,
    100
  );

  // Mantener el resultado entre 0 y 100
  humedad = constrain(humedad, 0, 100);

  // Evaluar las condiciones del suelo
  const char* estadoLCD;
  const char* estadoSerial;

  if (humedad < 30) {
    estadoLCD = "SUELO SECO";
    estadoSerial = "Estado: SUELO SECO";
  }
  else if (humedad < 70) {
    estadoLCD = "HUMEDAD MEDIA";
    estadoSerial = "Estado: HUMEDAD MEDIA";
  }
  else {
    estadoLCD = "SUELO HUMEDO";
    estadoSerial = "Estado: SUELO HUMEDO";
  }

  // Mostrar las mediciones en Monitor Serial
  Serial.print("ADC: ");
  Serial.print(lectura);
  Serial.print(" | Humedad: ");
  Serial.print(humedad);
  Serial.println("%");
  Serial.println(estadoSerial);
  Serial.println("--------------------");

  // Fila 0 del LCD: Porcentaje y valor ADC (ej. "Hum: 64% A:712  ")
  lcd.setCursor(0, 0);
  lcd.print("Hum:");
  if (humedad < 10) lcd.print("  ");
  else if (humedad < 100) lcd.print(" ");
  lcd.print(humedad);
  lcd.print("% A:");
  lcd.print(lectura);
  lcd.print("   ");

  // Fila 1 del LCD: Estado del suelo (ej. "HUMEDAD MEDIA   ")
  lcd.setCursor(0, 1);
  lcd.print(estadoLCD);
  lcd.print("    ");

  delay(INTERVALO_MS);
}
