#include <Arduino.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// Pines del RTC: DAT, CLK, RST
ThreeWire conexion(10, 11, 9);
RtcDS1302<ThreeWire> rtc(conexion);

void setup() {
    Serial.begin(9600);
    rtc.Begin();

    rtc.SetIsWriteProtected(false);
    rtc.SetTrickleChargeSettings(DS1302Tcr_Disabled);

    // Configurar fecha y hora de compilacion
    rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
    rtc.SetIsRunning(true);

    Serial.println("PlantCare - P6 RTC");
}

void loop() {
    RtcDateTime ahora = rtc.GetDateTime();
    
int hora = ahora.Hour();

if (hora >= 6 && hora < 18) {
    Serial.println("Modo DIA");
} else {
    Serial.println("Modo NOCHE");
}

    Serial.print(ahora.Day());
    Serial.print("/");
    Serial.print(ahora.Month());
    Serial.print("/");
    Serial.print(ahora.Year());

    Serial.print(" | ");

    Serial.print(ahora.Hour());
    Serial.print(":");
    Serial.print(ahora.Minute());
    Serial.print(":");
    Serial.println(ahora.Second());

    delay(1000);
}
