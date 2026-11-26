#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

// PlantCare P10 - Arduino UNO. Rele activo LOW, contactos COM/NO.
const uint8_t PIN_SUELO=A0, PIN_DHT=3, PIN_ANT=2, PIN_SIG=4;
const uint8_t PIN_OK=5, PIN_LED=6, PIN_RELE=7;
const int ADC_SECO=1023, ADC_HUMEDO=559;
const unsigned long PULSO_MS=2000, ABSORCION_MS=30000;
const uint8_t MAX_PULSOS=3;
const unsigned long MUESTREO_MS=2000;

DHT dht(PIN_DHT,DHT11);
LiquidCrystal_I2C lcd(0x27,16,2);
BH1750 bh1750;
ThreeWire conexion(10,11,9); // DAT, CLK, RST
RtcDS1302<ThreeWire> rtc(conexion);

// Umbrales iniciales de prototipo, no equivalen a humedad volumetrica.
struct Perfil { const char* nombre; uint8_t seco, objetivo, tMin,tMax,hMin,hMax; uint16_t luzMin; };
const Perfil perfiles[] = {
  {"Albahaca",30,60,15,32,40,80,100},
  {"Petunia",30,55,15,32,40,80,100},
  {"Menta",35,65,15,30,40,80,100},
  {"Tomate",35,60,15,32,40,80,100}
};
const uint8_t NPERFILES=sizeof(perfiles)/sizeof(perfiles[0]);
uint8_t perfil=0, candidato=0, pagina=0;
bool eligiendo=false, lcdOK=false, luzDisponible=false, rtcDisponible=false;
bool rtcOK=false, diaManual=true, esDia=true;
bool sueloOK=false, dhtOK=false, luzOK=false;
int adc=0, suelo=0;
float temp=0, aire=0, lux=0;
RtcDateTime hora(2000,1,1,0,0,0);
unsigned long ultimaMuestra=0, ultimaPantalla=0, ultimaAlerta=0;
uint8_t alertas=0, alertaVista=0;
// Bits: temperatura, humedad ambiental, luz baja de dia.
const uint8_t ALERTA_TEMP=1, ALERTA_AIRE=2, ALERTA_LUZ=4;

enum EstadoRiego { REPOSO, PULSO, ABSORCION, BLOQUEO };
EstadoRiego estado=REPOSO;
bool habilitado=false; // habilitacion explicita por seguridad tras cada reinicio
uint8_t pulsos=0;
unsigned long inicioEstado=0;

struct Boton { uint8_t pin; bool crudo, estable; unsigned long cambio; };
Boton anterior={PIN_ANT,HIGH,HIGH,0};
Boton siguiente={PIN_SIG,HIGH,HIGH,0};
Boton aceptar={PIN_OK,HIGH,HIGH,0};

bool pulsado(Boton &b) {
  bool v=digitalRead(b.pin);
  unsigned long ahora=millis();
  if(v!=b.crudo){b.crudo=v;b.cambio=ahora;}
  if(v!=b.estable && ahora-b.cambio>=35){b.estable=v;return v==LOW;}
  return false;
}

void apagarBomba(){digitalWrite(PIN_RELE,HIGH);}
void pausarRiego(){
  apagarBomba(); habilitado=false;
  // No borrar pulsos ni bloqueo al pausar: evita eludir el limite.
  if(estado==PULSO){estado=ABSORCION;inicioEstado=millis();}
}
void reiniciarRiego(){
  apagarBomba(); habilitado=false; estado=REPOSO; pulsos=0;
  Serial.println(F("Riego reiniciado y pausado. Habilitar desde menu."));
}

bool probarRTC(){
  rtc.Begin();
  rtc.SetIsWriteProtected(false);
  uint8_t original=rtc.GetMemory((uint8_t)0);
  rtc.SetMemory((uint8_t)0,(uint8_t)85);
  uint8_t a=rtc.GetMemory((uint8_t)0);
  rtc.SetMemory((uint8_t)0,(uint8_t)170);
  uint8_t b=rtc.GetMemory((uint8_t)0);
  if(a!=85 || b!=170) return false;
  rtc.SetMemory((uint8_t)0,original);
  if(!rtc.IsDateTimeValid()) rtc.SetDateTime(RtcDateTime(__DATE__,__TIME__));
  if(!rtc.GetIsRunning()) rtc.SetIsRunning(true);
  if(!rtc.IsDateTimeValid() || !rtc.GetIsRunning()) return false;
  RtcDateTime aHora=rtc.GetDateTime();
  delay(1200);
  RtcDateTime bHora=rtc.GetDateTime();
  return rtc.IsDateTimeValid() && rtc.GetIsRunning() && aHora.Second()!=bHora.Second();
}

void leerSensores(){
  // Promedio de ocho lecturas. Un ADC de 1023 puede ser suelo seco o circuito abierto.
  long suma=0;
  for(uint8_t i=0;i<8;i++) suma+=analogRead(PIN_SUELO);
  adc=suma/8;
  suelo=constrain(map(adc,ADC_SECO,ADC_HUMEDO,0,100),0,100);
  sueloOK=(adc>0); // solo detecta extremo bajo; no detecta todos los fallos de sonda
  temp=dht.readTemperature(); aire=dht.readHumidity();
  dhtOK=!isnan(temp)&&!isnan(aire)&&aire>=0&&aire<=100;
  luzOK=false;
  if(luzDisponible){
    Wire.clearWireTimeoutFlag();
    lux=bh1750.readLightLevel();
    luzOK=!Wire.getWireTimeoutFlag()&&!isnan(lux)&&lux>=0;
    Wire.clearWireTimeoutFlag();
  }
  rtcOK=false;
  if(rtcDisponible && rtc.IsDateTimeValid() && rtc.GetIsRunning()){
    hora=rtc.GetDateTime(); rtcOK=true;
  }
  esDia=rtcOK ? (hora.Hour()>=6 && hora.Hour()<18) : diaManual;
  ultimaMuestra=millis();
}

void actualizarAlertas(){
  const Perfil &p=perfiles[perfil];
  alertas=0;
  if(dhtOK && (temp<p.tMin || temp>p.tMax)) alertas|=ALERTA_TEMP;
  if(dhtOK && (aire<p.hMin || aire>p.hMax)) alertas|=ALERTA_AIRE;
  if(esDia && luzOK && lux<p.luzMin) alertas|=ALERTA_LUZ;
  digitalWrite(PIN_LED,alertas?HIGH:LOW);
}
uint8_t contarAlertas(){return !!(alertas&1)+!!(alertas&2)+!!(alertas&4);}
const char* textoAlerta(uint8_t n){
  if(alertas&ALERTA_TEMP){if(n==0)return "Temperat. fuera";n--;}
  if(alertas&ALERTA_AIRE){if(n==0)return "Humedad aire";n--;}
  if(alertas&ALERTA_LUZ){if(n==0)return "Luz insuficiente";}
  return "Sin alertas";
}
const char* textoRiego(){
  if(estado==BLOQUEO)return "Bloqueado";
  if(!habilitado)return "Pausado";
  if(estado==PULSO)return "Regando";
  if(estado==ABSORCION)return "Absorbiendo";
  return "Automatico";
}
void filaLCD(uint8_t fila,const char* texto){
  lcd.setCursor(0,fila);
  uint8_t n=0;
  while(n<16 && texto[n]){lcd.write((uint8_t)texto[n]);n++;}
  while(n++<16)lcd.write((uint8_t)' ');
}
void mostrarLCD(){
  if(!lcdOK)return;
  char l1[17]={0},l2[17]={0};
  if(eligiendo){
    snprintf(l1,sizeof(l1),"Elegir planta");
    snprintf(l2,sizeof(l2),">%s",perfiles[candidato].nombre);
  }else switch(pagina){
    case 0:
      if(sueloOK)snprintf(l1,sizeof(l1),"Suelo:%d%%",suelo);
      else snprintf(l1,sizeof(l1),"Suelo: ERROR");
      if(luzOK)snprintf(l2,sizeof(l2),"Luz:%lu lx",(unsigned long)lux);
      else snprintf(l2,sizeof(l2),"Luz: ERROR");
      break;
    case 1:
      if(dhtOK){
        snprintf(l1,sizeof(l1),"Temp:%d C",(int)temp);
        snprintf(l2,sizeof(l2),"Aire:%d%%",(int)aire);
      }else{snprintf(l1,sizeof(l1),"Ambiente");snprintf(l2,sizeof(l2),"Error DHT11");}
      break;
    case 2:
      snprintf(l1,sizeof(l1),"Riego:%s",textoRiego());
      if(estado==BLOQUEO)snprintf(l2,sizeof(l2),"OK: reiniciar");
      else if(estado==PULSO || estado==ABSORCION)snprintf(l2,sizeof(l2),"Pulso %u/%u",pulsos,MAX_PULSOS);
      else snprintf(l2,sizeof(l2),"OK:%s",habilitado?"pausar":"activar");
      break;
    case 3:
      snprintf(l1,sizeof(l1),"Planta:%s",perfiles[perfil].nombre);
      snprintf(l2,sizeof(l2),"OK: cambiar");
      break;
    case 4:
      snprintf(l1,sizeof(l1),"Alertas:%u",contarAlertas());
      snprintf(l2,sizeof(l2),"%s",textoAlerta(alertaVista));
      break;
    case 5:
      if(rtcOK){
        snprintf(l1,sizeof(l1),"Hora %02u:%02u:%02u",hora.Hour(),hora.Minute(),hora.Second());
        snprintf(l2,sizeof(l2),"%s (RTC)",esDia?"Dia":"Noche");
      }else{
        snprintf(l1,sizeof(l1),"RTC error/manual");
        snprintf(l2,sizeof(l2),"%s OK:cambiar",diaManual?"Dia":"Noche");
      }
      break;
  }
  Wire.clearWireTimeoutFlag();
  filaLCD(0,l1);
  if(!Wire.getWireTimeoutFlag())filaLCD(1,l2);
  if(Wire.getWireTimeoutFlag()){
    lcdOK=false;
    Serial.println(F("LCD timeout: control sigue por serial."));
  }
  Wire.clearWireTimeoutFlag();
  ultimaPantalla=millis();
}

void iniciarPulso(){
  if(!habilitado || !sueloOK || pulsos>=MAX_PULSOS)return;
  estado=PULSO; pulsos++;
  // Actualiza pantalla ANTES de encender bomba: evita retardar su apagado por I2C.
  mostrarLCD();
  Serial.print(F("Riego pulso "));Serial.println(pulsos);
  inicioEstado=millis();
  digitalWrite(PIN_RELE,LOW);
}
void controlarRiego(){
  unsigned long ahora=millis();
  if(estado==PULSO){
    if(!habilitado || ahora-inicioEstado>=PULSO_MS){
      apagarBomba();estado=ABSORCION;inicioEstado=millis();
    }
    return;
  }
  if(estado==BLOQUEO)return;
  if(!sueloOK){pausarRiego();return;}
  if(!habilitado)return;
  const Perfil &p=perfiles[perfil];
  if(estado==REPOSO){
    if(suelo<p.seco){pulsos=0;iniciarPulso();}
  }else if(estado==ABSORCION && ahora-inicioEstado>=ABSORCION_MS){
    // Requiere una lectura tomada tras la espera.
    if(ultimaMuestra-inicioEstado<ABSORCION_MS || ahora-ultimaMuestra>MUESTREO_MS+500)return;
    if(suelo>=p.objetivo){estado=REPOSO;pulsos=0;}
    else if(pulsos>=MAX_PULSOS){
      estado=BLOQUEO;habilitado=false;apagarBomba();
      Serial.println(F("BLOQUEO: maximo 3 pulsos. Revisar deposito y suelo."));
    }else iniciarPulso();
  }
}

void manejarBotones(){
  bool a=pulsado(anterior),s=pulsado(siguiente),ok=pulsado(aceptar);
  if(!a&&!s&&!ok)return;
  if(eligiendo){
    if(a)candidato=(candidato+NPERFILES-1)%NPERFILES;
    if(s)candidato=(candidato+1)%NPERFILES;
    if(ok){
      if(candidato!=perfil){pausarRiego();perfil=candidato;}
      eligiendo=false;
      Serial.print(F("Perfil: "));Serial.println(perfiles[perfil].nombre);
    }
  }else{
    if(a)pagina=(pagina+5)%6;
    if(s)pagina=(pagina+1)%6;
    if(ok){
      if(pagina==2){
        if(estado==BLOQUEO)reiniciarRiego();
        else if(habilitado)pausarRiego();
        else if(sueloOK){habilitado=true;Serial.println(F("Riego habilitado"));}
      }else if(pagina==3){candidato=perfil;eligiendo=true;}
      else if(pagina==5 && !rtcOK){diaManual=!diaManual;esDia=diaManual;}
    }
  }
  if(estado!=PULSO){actualizarAlertas();mostrarLCD();}
}
void informeSerial(){
  Serial.print(F("Planta: "));Serial.print(perfiles[perfil].nombre);
  Serial.print(F(" | ADC: "));Serial.print(adc);
  Serial.print(F(" | Suelo: "));if(sueloOK)Serial.print(suelo);else Serial.print(F("ERR"));
  Serial.print(F("% | Temp: "));if(dhtOK)Serial.print(temp);else Serial.print(F("ERR"));
  Serial.print(F(" | Aire: "));if(dhtOK)Serial.print(aire);else Serial.print(F("ERR"));
  Serial.print(F(" | Lux: "));if(luzOK)Serial.print(lux);else Serial.print(F("ERR"));
  Serial.print(F(" | "));Serial.print(esDia?F("Dia"):F("Noche"));
  if(!rtcOK)Serial.print(F(" manual (RTC error)"));
  else{Serial.print(F(" RTC "));Serial.print(hora.Hour());Serial.print(':');Serial.print(hora.Minute());Serial.print(':');Serial.print(hora.Second());}
  Serial.print(F(" | Alertas: "));Serial.print(contarAlertas());
  Serial.print(F(" | Riego: "));Serial.print(textoRiego());
  Serial.print(F(" | Pulsos: "));Serial.println(pulsos);
}
void setup(){
  digitalWrite(PIN_RELE,HIGH);pinMode(PIN_RELE,OUTPUT);apagarBomba();
  pinMode(PIN_LED,OUTPUT);digitalWrite(PIN_LED,LOW);
  pinMode(PIN_ANT,INPUT_PULLUP);pinMode(PIN_SIG,INPUT_PULLUP);pinMode(PIN_OK,INPUT_PULLUP);
  Serial.begin(9600);delay(200);
  Serial.println(F("PlantCare P10 - FINAL"));
  Wire.begin();Wire.setWireTimeout(25000,true);
  dht.begin();
  Wire.beginTransmission(0x27);lcdOK=(Wire.endTransmission()==0);
  if(lcdOK){
    Wire.clearWireTimeoutFlag();lcd.init();lcd.backlight();
    if(Wire.getWireTimeoutFlag())lcdOK=false;
    Wire.clearWireTimeoutFlag();
  }
  Wire.clearWireTimeoutFlag();
  luzDisponible=bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE,0x23);
  if(Wire.getWireTimeoutFlag())luzDisponible=false;
  Wire.clearWireTimeoutFlag();
  rtcDisponible=probarRTC();
  Serial.print(F("LCD: "));Serial.println(lcdOK?F("OK"):F("ERROR"));
  Serial.print(F("BH1750: "));Serial.println(luzDisponible?F("OK"):F("ERROR"));
  Serial.print(F("RTC: "));Serial.println(rtcDisponible?F("OK"):F("ERROR - dia/noche manual"));
  Serial.println(F("Bomba pausada. Menu Riego + OK para habilitar. R: reiniciar y pausar; X: pausar."));
  delay(2000); // estabiliza DHT y primera lectura; bomba apagada
  leerSensores();actualizarAlertas();mostrarLCD();informeSerial();
}
void loop(){
  // Mientras la bomba esta activa no ejecutar lecturas I2C, LCD ni DHT.
  if(estado==PULSO){
    controlarRiego();
    if(estado==PULSO){
      // Permitir parada desde el boton OK en la pagina de riego o serial X/R.
      manejarBotones();
      if(Serial.available()){
        char c=Serial.read();
        if(c=='x'||c=='X')pausarRiego();
        if(c=='r'||c=='R')reiniciarRiego();
      }
      return;
    }
  }
  if(Serial.available()){
    char c=Serial.read();
    if(c=='r'||c=='R')reiniciarRiego();
    if(c=='x'||c=='X')pausarRiego();
  }
  manejarBotones();
  unsigned long ahora=millis();
  if(ahora-ultimaMuestra>=MUESTREO_MS){leerSensores();actualizarAlertas();informeSerial();}
  ahora=millis();
  if(ahora-ultimaAlerta>=2000){
    ultimaAlerta=ahora;uint8_t n=contarAlertas();alertaVista=n?(alertaVista+1)%n:0;
  }
  if(alertaVista>=contarAlertas())alertaVista=0;
  controlarRiego();
  if(estado!=PULSO && millis()-ultimaPantalla>=500)mostrarLCD();
}
