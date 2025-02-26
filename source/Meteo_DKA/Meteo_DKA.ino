/*
* Example code for AirBoard and SEN5x
* The measuread values are send to 
* Serial Monitor and to TMEP.cz (TMEP.eu)
* If you want to use deep sleep function, short RST with GPIO16 (16) on header of AirBoard.
*
* Libraries:
* https://github.com/Sensirion/arduino-i2c-sen5x
*
* laskakit 2023 + úpravy PMA 2025
*/

#include <ESP8266WiFi.h>
#include <SensirionI2CSen5x.h>
#include <SensirionI2CScd4x.h>
#include <Adafruit_BME280.h>
#include <Wire.h>
#include <GxEPD.h>
#include <GxGDEH0154D67/GxGDEH0154D67.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeMonoBold9pt7b.h>

//GxIO_Class io(SPI, /*CS=*/ 0, /*DC=*/ 2, /*RST=*/ 15); // arbitrary selection of 8, 9 selected for default of GxEPD_Class
//GxEPD_Class display(io, /*RST=*/ 15, /*BUSY=*/ 4); // default selection of (9), 7

#define BOOST_EN_PIN 12
#define BATTERY_PIN 0             // ADC (battery) pin on LaskaKit AIR board
#define deviderRatio 4.1154657  // Voltage devider ratio on ADC pin

unsigned int waitTime = 600000;

// FONTs definition =================================================
const char* name9 = "FreeMonoBold9pt7b";
const GFXfont* f9 = &FreeMonoBold9pt7b;

// SEN5x, SCD41, BME280 instance senzorů ============================
SensirionI2CSen5x sen5x;
SensirionI2CScd4x scd4x;
Adafruit_BME280 bme280;

uint16_t error;
char errorMessage[256];
float bat_voltage;
// SEN5x proměnné ==================================================== 
float massConcentrationPm1p0;
float massConcentrationPm2p5;
float massConcentrationPm4p0;
float massConcentrationPm10p0;
float ambientHumidity;
float ambientTemperature;
float vocIndex;
float noxIndex;
// SCD41 proměnné ====================================================
uint16_t co2;
float tmp;
float hum;
// BME280 proměnné ===================================================
float temp;
float humid;
float atm;

// definice pro WiFi a TMEP ==========================================
const char* ssid     = "xxxxxxxxx"; // SSID WiFi
const char* password = "xxxxxxxxx"; // heslo k WiFi
const char* host1 = "xxxxxxxxxx.tmep.cz"; // Doména č. 1 pro zasílání hodnot 
const char* host2 = "xxxxxxxxxx.tmep.cz"; // Doména č. 2 pro zasílání hodnot

// funkce pro kalibraci dat - SEN5x ==========================
void sen5x_init() {
  sen5x.begin(Wire);
  float tempOffset = 0.0;
  error = sen5x.setTemperatureOffsetSimple(tempOffset);
  if (error) {
    Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("Temperature Offset set to ");
    Serial.print(tempOffset);
    Serial.println(" deg. Celsius (SEN54/SEN55 only)");
  }
  // Start Measurement
  error = sen5x.startMeasurement();
  if (error) {
    Serial.print("Error trying to execute startMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
}

// funkce pro čtení dat - SEN5x ==========================
void sen5x_read() {
  error = sen5x.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

  if (error) {
    Serial.print("Error trying to execute readMeasuredValues(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    Serial.print("SEN5x - Pm1p0:");
    Serial.print(massConcentrationPm1p0);
    Serial.print("; ");
    Serial.print("Pm2p5:");
    Serial.print(massConcentrationPm2p5);
    Serial.print("; ");
    Serial.print("Pm4p0:");
    Serial.print(massConcentrationPm4p0);
    Serial.print("; ");
    Serial.print("Pm10p0:");
    Serial.print(massConcentrationPm10p0);
    Serial.print("; ");
    Serial.print("Vlhkost:");
    if (isnan(ambientHumidity)) {
      Serial.print("n/a");
    } else {
      Serial.print(round(ambientHumidity),0);
    }
    Serial.print("; ");
    Serial.print("Teplota:");
    if (isnan(ambientTemperature)) {
      Serial.print("n/a");
    } else {
      Serial.print(ambientTemperature);
    }
    Serial.print("; ");
    Serial.print("VocIndex:");
    if (isnan(vocIndex)) {
      Serial.print("n/a");
    } else {
      Serial.print(vocIndex,0);
    }
    Serial.print("; ");
    Serial.print("NoxIndex:");
    if (isnan(noxIndex)) {
      Serial.print("n/a");
    } else {
      Serial.print(noxIndex,0);
    }
    Serial.println(";");
  }
}

// funkce pro kalibraci - SCD41 ==========================
void printUint16Hex(uint16_t value) {
    Serial.print(value < 4096 ? "0" : "");
    Serial.print(value < 256 ? "0" : "");
    Serial.print(value < 16 ? "0" : "");
    Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
    Serial.print("SCD41 Sériové číslo: 0x");
    printUint16Hex(serial0);
    printUint16Hex(serial1);
    printUint16Hex(serial2);
    Serial.println();
}

void scd4x_init() {
  scd4x.begin(Wire);
  // chybné dříve zahájené měření
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
      Serial.print("SCD41 chyba dříve zahájeného měření: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  }

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
      Serial.print("SCD41 chyba při získávání sériového čísla: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  } else {
      printSerialNumber(serial0, serial1, serial2);
  }
  // Start měření hodnot
  error = scd4x.startPeriodicMeasurement();
  if (error) {
      Serial.print("SCD41 chyba dříve zahájeného měření: ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  }

  Serial.println("Čekání na první měření... (5 sekund)");
  delay(5000);
}

// funkce pro čtení dat - SCD41 ==========================
void scd41_read() {
  error = scd4x.readMeasurement(co2, tmp, hum);
  if (error) {
    Serial.print("SCD41 chyba čtení: ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else if (co2 == 0) {
    Serial.println("Byla zjištěna neplatná hodnota!");
  } else {
    // vypsání hodnot do sériového monitoru pro kontrolu
    Serial.print("SCD41 - CO2:");
    Serial.print(co2);
    Serial.print("; ");
    Serial.print("Teplota:");
    Serial.print(tmp);
    Serial.print("; ");
    Serial.print("Vlhkost:");
    Serial.print(round(hum),0);
    Serial.println(";");
  }
}

// funkce pro čtení dat - BME280 =========================
void bme280_init() {
  unsigned status;
  status = bme280.begin();
  if (!status) {
    Serial.println(F("Nemohu nalézt BME280 senzor!"));
    while (1) delay(10);
  }
}

// funkce pro čtení dat - BME280 =========================
void bme280_read() {
  temp = bme280.readTemperature();
  humid = bme280.readHumidity();
  atm = bme280.readPressure()/100.0F;

  // vypsání hodnot do sériového monitoru pro kontrolu
  Serial.print("BME280 - Teplota:");
  Serial.print(temp);
  Serial.print("; ");
  Serial.print("Atm. tlak:");
  Serial.print(atm);
  Serial.print("; ");
  Serial.print("Vlhkost:");
  Serial.print(round(humid),0);
  Serial.println(";");
}

// funkce pro čtení napětí baterie =========================
void BatVoltage_read(){
  bat_voltage = analogRead(BATTERY_PIN) * deviderRatio / 1000;
  Serial.println("Battery voltage " + String(bat_voltage) + "V");
}

// funkce pro vykreslení hodnot na E-ink display ===========
/*void drawDisplayData() {
  //display.eraseDisplay();
  //display.update();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f9);
  display.setCursor(0, 0);
  display.println();
  display.println(name9);
  display.println(" !\"#$%&'()*+,-./");
  display.println("0123456789:;<=>?");
  display.println("@ABCDEFGHIJKLMNO");
  display.println("PQRSTUVWXYZ[\\]^_");
  display.println("`abcdefghijklmno");
  display.println("pqrstuvwxyz{|}~ ");
  display.update();
}*/

void MainProgram() {
  delay(1000);
  sen5x_read();
  delay(200);
  scd41_read();
  delay(200);
  bme280_read();
  delay(200);
  // Serial print average values ​​from sensors
  Serial.println("Průměr hodnot - Teplota:" + String((ambientTemperature+tmp+temp)/3) + "; Vlhkost:" + String(round((ambientHumidity+hum+humid)/3)) + ";");
  delay(200);
  BatVoltage_read();

  // show data to E-ink display
  //drawDisplayData();

  /* -------------- Wi-Fi --------------- */
  WiFiClient client;
  const int httpPort = 80;
  // We now create a URI for the request
  String url = "/?";
  /*Serial.print("Requesting URL: ");
  Serial.println(url);*/

  // Connect to host1 and GET data on sensor 01
  if (!client.connect(host1, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  client.print(String("GET ") + url + "temp=" + String((ambientTemperature+tmp+temp)/3) + "&hum=" + String(round((ambientHumidity+hum+humid)/3)) + "&atm=" + String(atm) + "&v=" + String(bat_voltage) + " HTTP/1.1\r\n" +
            "Host: " + host1 + "\r\n" + 
            "Connection: close\r\n\r\n");

  delay(10);
  //
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println("Dokončeno odesílání hodnot Meteo DKA - part01!");
  delay(1000);
  // Connect to host2 and GET data on sensor 02
  if (!client.connect(host2, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  client.print(String("GET ") + url + "co2=" + String(co2) + "&pm2p5=" + String(massConcentrationPm2p5) + "&pm10p0=" + String(massConcentrationPm10p0) + "&v=" + String(bat_voltage) + " HTTP/1.1\r\n" +
            "Host: " + host2 + "\r\n" + 
            "Connection: close\r\n\r\n");

  delay(10);
  //
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  Serial.println("Dokončeno odesílání hodnot Meteo DKA - part02!");
  /* -------------- END Wi-Fi ---------------- */
  //if you want to use deep sleep, connect RST with GPIO16 (16) on header
  Serial.println("Uspávám se na "+String((waitTime/1000)/60)+" minut!");
  Serial.flush();
  // vypnuti napájení 5V
  digitalWrite(BOOST_EN_PIN, LOW);
  delay(200);
  // uspání ESP8266 na stanovenou dobu
  ESP.deepSleep(waitTime/1000 * 1000000, WAKE_RF_DEFAULT);
  delay(100);
}

// start programu ========================================
void setup() {
  Serial.begin(115200);
  // boost 5v ON
  pinMode(BOOST_EN_PIN, OUTPUT);
  digitalWrite(BOOST_EN_PIN, HIGH);
  //
  /*display.init();
  display.setRotation(3);
  display.eraseDisplay();
  display.update();*/
  //
  Wire.begin(0, 2);
  delay(250);
  sen5x_init();
  delay(250);
  scd4x_init();
  delay(250);
  bme280_init();
  delay(250);
  /* -------------- Wi-Fi --------------- */
  WiFi.disconnect();
  delay(200);
  Serial.println("Dodatečná kalibrace měření... (5 sekund)");
  delay(5000);
  WiFi.begin(ssid, password);
  Serial.print("Připojuji se na WiFi ");
  int numberOfConnections;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    numberOfConnections++;
    // if ESP8266 can't connect to WiFi -> go to deep.sleep
    if (numberOfConnections > 20) {
      //if you want to use deep sleep, connect RST with GPIO16 (16) on header
      ESP.deepSleep(waitTime/1000 * 1000000, WAKE_RF_DEFAULT);
      //delay(waitTime);
      return;
    }
  }
  Serial.println("");
  Serial.print("Připojeno! IP: ");
  Serial.println(WiFi.localIP());
  /* -------------- END Wi-Fi ---------------- */
  MainProgram();
}

void loop() {
  //MainProgram();
  //delay(waitTime);
}