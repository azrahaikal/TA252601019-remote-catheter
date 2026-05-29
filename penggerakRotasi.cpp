#include <ETH.h>
#include <WiFiUdp.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// --- KONFIGURASI MOTOR (NEMA 23) ---
#define STEP_PIN 12
#define DIR_PIN 14
const int JEDA_KECEPATAN = 1200;
const long PPR = 400;
const float RASIO_PULLEY = 5.0;

// --- VARIABEL GLOBAL & MUTEX ---
volatile float targetRotasi = 0.0;
float posisiSekarang = 0.0;
SemaphoreHandle_t dataMutex;

// --- KONFIGURASI LAN ---
IPAddress local_ip(192, 168, 1, 20);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
unsigned int localPort = 8888;
WiFiUDP udp;
char packetBuffer[255];

// --- TASK 1: MENERIMA DATA LAN (CORE 0) ---
void taskLAN(void *pvParameters) {
  for (;;) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      int len = udp.read(packetBuffer, 255);
      if (len > 0) packetBuffer[len] = 0;
      
      String dataMasuk = String(packetBuffer);
      int commaIndex = dataMasuk.indexOf(',');
      
      if (commaIndex > 0) {
        float dataS1 = dataMasuk.substring(0, commaIndex).toFloat();
        
        // Simpan ke variabel global dengan proteksi Mutex
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          targetRotasi = dataS1;
          xSemaphoreGive(dataMutex);
        }
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Istirahat sejenak agar Core 0 tidak hang
  }
}

// --- TASK 2: MENGGERAKKAN MOTOR (CORE 1) ---
void taskMotor(void *pvParameters) {
  for (;;) {
    float localTarget;
    
    // Ambil data terbaru dengan aman
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      localTarget = targetRotasi;
      xSemaphoreGive(dataMutex);
    }

    float selisih = localTarget - posisiSekarang;
    
    if (abs(selisih) > 0.1) { // Toleransi error 0.1 derajat
      digitalWrite(DIR_PIN, (selisih > 0) ? HIGH : LOW);
      
      long totalStep = (abs(selisih) / 360.0) * PPR * RASIO_PULLEY;
      
      for (long i = 0; i < totalStep; i++) {
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(JEDA_KECEPATAN);
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(JEDA_KECEPATAN);
        if (i % 100 == 0) yield(); // Mencegah Watchdog Timer reset
      }
      posisiSekarang = localTarget;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  
  dataMutex = xSemaphoreCreateMutex();
  
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);

  // Jalankan Task di Core yang berbeda
  xTaskCreatePinnedToCore(taskLAN, "TaskLAN", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskMotor, "TaskMotor", 4096, NULL, 1, NULL, 1);
}

void loop() {
  // Biarkan kosong, semua kerja dilakukan oleh Task
}