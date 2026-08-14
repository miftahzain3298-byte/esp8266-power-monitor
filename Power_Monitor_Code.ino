#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Konfigurasi WiFi dan MQTT
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "broker.emqx.io"; // Or your own broker
const int mqtt_port = 1883;
const char* mqtt_topic = "power_meter/data";

// Konfigurasi pin TFT SPI
#define TFT_CS   D3
#define TFT_DC   D4
#define TFT_RST  D0

PZEM004Tv30 pzem(4, 5); // RX=D1(5), TX=D2(4)

// Inisialisasi TFT
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Variabel sensor
float Power, Energy, Voltage, Current;

// Warna untuk setiap parameter
#define BACKGROUND    ST77XX_BLACK
#define VOLTAGE_COLOR ST77XX_CYAN
#define CURRENT_COLOR ST77XX_YELLOW
#define POWER_COLOR   ST77XX_MAGENTA
#define ENERGY_COLOR  ST77XX_GREEN
#define ERROR_COLOR   ST77XX_RED

// Objek WiFi dan MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Variabel untuk menyimpan nilai sebelumnya
float prevVoltage = NAN;
float prevCurrent = NAN;
float prevPower = NAN;
float prevEnergy = NAN;

// Koordinat nilai untuk masing-masing parameter
const int valueX = 180;
const int voltageY = 50;
const int currentY = 90;
const int powerY = 130;
const int energyY = 170;

void setup() {
  Serial.begin(9600);
  
  // Inisialisasi TFT
  tft.init(240, 320, SPI_MODE2);
  tft.setRotation(3);
  tft.fillScreen(BACKGROUND);
  tft.setTextWrap(false);
  
  // Header TFT
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_ORANGE);
  tft.setCursor(20, 10);
  tft.println("ELECTRICAL POWER MONITOR");
  tft.drawLine(20, 35, 300, 35, ST77XX_WHITE);

  // Tampilkan label statis
  displayParameter(50, voltageY, "Voltage :", VOLTAGE_COLOR);
  displayParameter(50, currentY, "Current :", CURRENT_COLOR);
  displayParameter(50, powerY, "Power   :", POWER_COLOR);
  displayParameter(50, energyY, "Energy  :", ENERGY_COLOR);
  
  // Gambar garis bawah
  tft.drawLine(20, 200, 300, 200, ST77XX_WHITE);

  // Koneksi WiFi
  setup_wifi();
  
  // Konfigurasi MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Menghubungkan ke WiFi");
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi Terhubung");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan MQTT...");
    if (client.connect("PowerMeterClient")) {
      Serial.println("MQTT Terhubung");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  readSensorData();
  printSerialData();
  displayTFTData();
  sendMQTTData();
  
  delay(1000);
}

void readSensorData() {
  Voltage = pzem.voltage();
  Current = pzem.current();
  Power = pzem.power();
  Energy = pzem.energy();
}

void printSerialData() {
  Serial.println("\n--- Sensor Readings ---");
  
  if(!isnan(Voltage)) {
    Serial.print("Voltage: "); Serial.print(Voltage); Serial.println("V");
  } else Serial.println("Voltage error");
    
  if(!isnan(Current)) {
    Serial.print("Current: "); Serial.print(Current); Serial.println("A");
  } else Serial.println("Current error");
    
  if(!isnan(Power)) {
    Serial.print("Power: "); Serial.print(Power); Serial.println("kW");
  } else Serial.println("Power error");
    
  if(!isnan(Energy)) {
    Serial.print("Energy: "); Serial.print(Energy); Serial.println("kWh");
  } else Serial.println("Energy error");
  
  Serial.println("-----------------------");
}

void displayTFTData() {
  // Update nilai dengan area penghapusan yang tepat
  updateValue(valueX, voltageY, Voltage, "V", VOLTAGE_COLOR, prevVoltage);
  updateValue(valueX, currentY, Current, "A", CURRENT_COLOR, prevCurrent);
  updateValue(valueX, powerY, Power, "kW", POWER_COLOR, prevPower);
  updateValue(valueX, energyY, Energy, "kWh", ENERGY_COLOR, prevEnergy);
}

void displayParameter(int x, int y, const char* label, uint16_t color) {
  tft.setTextSize(2);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(label);
}

void updateValue(int x, int y, float value, const char* unit, uint16_t color, float &prevVal) {
  // Hanya update jika nilai berubah atau status error berubah
  if (value != prevVal || (isnan(value) != isnan(prevVal))) {
    // Hapus area spesifik dengan margin yang cukup
    tft.fillRect(x, y-2, 120, 20, BACKGROUND);
    
    tft.setTextSize(2);
    tft.setCursor(x, y);
    
    if (!isnan(value)) {
      tft.setTextColor(color);
      if (value < 10) {
        tft.print(value, 2);
      } else if (value < 100) {
        tft.print(value, 1);
      } else {
        tft.print(value, 0);
      }
      tft.print(" ");
      tft.print(unit);
    } else {
      tft.setTextColor(ERROR_COLOR);
      tft.print("ERROR");
    }
    prevVal = value;
  }
}

void sendMQTTData() {
  StaticJsonDocument<200> doc;

  if(isnan(Voltage)) {
    doc["voltage"] = nullptr;
  } else {
    doc["voltage"] = Voltage;
  }
  
  if(isnan(Current)) {
    doc["current"] = nullptr;
  } else {
    doc["current"] = Current;
  }
  
  if(isnan(Power)) {
    doc["power"] = nullptr;
  } else {
    doc["power"] = Power;
  }
  
  if(isnan(Energy)) {
    doc["energy"] = nullptr;
  } else {
    doc["energy"] = Energy;
  }

  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);
  
  if (client.publish(mqtt_topic, jsonBuffer)) {
    Serial.println("Data terkirim ke MQTT");
  } else {
    Serial.println("Gagal mengirim ke MQTT");
  }
}
