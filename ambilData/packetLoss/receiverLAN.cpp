#include <ETH.h>
#include <WiFiUdp.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// --- KONFIGURASI MOTOR ---
// nema 23 (Rotasi)
#define STEP_PIN_23 12
#define DIR_PIN_23 14
const int JEDA_23 = 1200;
const long PPR_23 = 400;
const float RASIO_23 = 5.0;

// nema 17 tmc2209 (Translasi/Linear)
#define STEP_PIN_17 32
#define DIR_PIN_17 33
#define EN_PIN_17 16
const int JEDA_17 = 800;
const long FULL_STEP_17 = 200;
const int MICROSTEP_17 = 8;
const long PPR_17 = FULL_STEP_17 * MICROSTEP_17; // 1600
const float RASIO_17 = 1.0;

// --- VARIABEL GLOBAL ---
volatile float targetRotasi = 0.0;
volatile float targetTranslasi = 0.0;

float posisiRotasi = 0.0;
float posisiTranslasi = 0.0;
float sisaStepTranslasi = 0.0;

const float TOLERANSI_NOISE = 0.5;
SemaphoreHandle_t dataMutex;

// --- KONFIGURASI JARINGAN ---
IPAddress local_ip(192, 168, 1, 20);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
unsigned int localPort = 8888;
WiFiUDP udp;
char packetBuffer[255];


// ==========================================
// CORE 0: TASK MENERIMA DATA LAN (UDP)
// ==========================================
void taskLAN(void *pvParameters) {
  for (;;) {
    if (ETH.linkUp()) {
      int packetSize = udp.parsePacket();
      if (packetSize > 0) {
        
        // Baca seluruh buffer yang masuk
        int len = udp.read(packetBuffer, 255);
        if (len > 0) packetBuffer[len] = 0;
        
        String dataMasuk = String(packetBuffer);
        dataMasuk.trim();
        
        // Print full payload untuk memastikan data tidak terpotong
        Serial.print("Paket masuk: ");
        Serial.println(dataMasuk);

        // Parsing ID dan Data (menggunakan koma pertama sebagai pemisah)
        int commaIndex = dataMasuk.indexOf(',');
        
        if (commaIndex > 0) {
          String idStr = dataMasuk.substring(0, commaIndex);
          String sisaData = dataMasuk.substring(commaIndex + 1);
          
          // Parsing isi data "rotasi,translasi"
          int secondComma = sisaData.indexOf(',');
          if (secondComma > 0) {
            float dataS1 = sisaData.substring(0, secondComma).toFloat();
            float dataS2 = sisaData.substring(secondComma + 1).toFloat();
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
              Serial.print("ID: "); Serial.print(idStr);
              Serial.print(" | Target Rotasi: "); Serial.print(dataS1);
              Serial.print(" | Target Translasi: "); Serial.println(dataS2);
              
              targetRotasi = dataS1;
              targetTranslasi = dataS2;
              xSemaphoreGive(dataMutex);
            }
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

// ==========================================
// CORE 1: TASK KENDALI ROTASI (NEMA 23)
// ==========================================
void taskRotasi(void *pvParameters) {
  for (;;) {
    float localTarget = posisiRotasi;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localTarget = targetRotasi;
      xSemaphoreGive(dataMutex);
    }

    float selisih = localTarget - posisiRotasi;
    
    if (abs(selisih) > TOLERANSI_NOISE) { 
      Serial.print("[NEMA 23] Bergerak mengejar delta: "); Serial.println(selisih);
      
      digitalWrite(DIR_PIN_23, (selisih > 0) ? HIGH : LOW);
      long totalStep = (abs(selisih) / 360.0) * PPR_23 * RASIO_23;
      
      for (long i = 0; i < totalStep; i++) {
        digitalWrite(STEP_PIN_23, LOW);
        delayMicroseconds(JEDA_23);
        digitalWrite(STEP_PIN_23, HIGH);
        delayMicroseconds(JEDA_23);
        
        // Interval context-switch diturunkan agar kedua motor bisa berjalan bersamaan
        if (i % 10 == 0) vTaskDelay(pdMS_TO_TICKS(1)); 
      }
      posisiRotasi = localTarget;
      Serial.println("[NEMA 23] Tiba di tujuan rotasi.");
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ==========================================
// CORE 1: TASK KENDALI TRANSLASI (NEMA 17)
// ==========================================
void taskTranslasi(void *pvParameters) {
  for (;;) {
    float localTarget = posisiTranslasi;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localTarget = targetTranslasi;
      xSemaphoreGive(dataMutex);
    }

    float selisih = localTarget - posisiTranslasi;
    
    // Logika batasan derajat 360 dihapus untuk pergerakan linier/translasi absolut

    if (abs(selisih) > TOLERANSI_NOISE) { 
      Serial.print("[NEMA 17] Bergerak mengejar delta translasi: "); Serial.println(selisih);
      
      digitalWrite(DIR_PIN_17, (selisih > 0) ? HIGH : LOW);
      
      float exactStep = (abs(selisih) / 360.0) * PPR_17 * RASIO_17;
      exactStep += sisaStepTranslasi;
      long totalStep = (long)exactStep;
      sisaStepTranslasi = exactStep - totalStep;
      
      for (long i = 0; i < totalStep; i++) {
        digitalWrite(STEP_PIN_17, HIGH);
        delayMicroseconds(JEDA_17);
        digitalWrite(STEP_PIN_17, LOW);
        delayMicroseconds(JEDA_17);
        
        // Interval disesuaikan untuk mengimbangi interupsi motor NEMA 23
        if (i % 20 == 0) vTaskDelay(pdMS_TO_TICKS(1)); 
      }
      posisiTranslasi = localTarget;
      Serial.println("[NEMA 17] Tiba di tujuan translasi.");
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ==========================================
// SETUP & MAIN LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Inisialisasi pin motor
  pinMode(STEP_PIN_23, OUTPUT);
  pinMode(DIR_PIN_23, OUTPUT);
  pinMode(STEP_PIN_17, OUTPUT);
  pinMode(DIR_PIN_17, OUTPUT);
  pinMode(EN_PIN_17, OUTPUT);
  
  // State awal motor
  digitalWrite(STEP_PIN_23, HIGH);
  digitalWrite(DIR_PIN_23, HIGH);
  digitalWrite(EN_PIN_17, LOW); // LOW untuk enable (driver TMC2209)
  digitalWrite(STEP_PIN_17, LOW);
  digitalWrite(DIR_PIN_17, LOW);
  
  dataMutex = xSemaphoreCreateMutex();
  
  // Inisialisasi LAN
  Serial.println("\nMemulai Ethernet MAC/PHY...");
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  
  // Menunggu negosiasi link sebelum membuka soket UDP
  Serial.print("Mencari sinyal LAN...");
  while (!ETH.linkUp()) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nLAN Terhubung! IP dikonfigurasi.");
  
  // Buka UDP hanya setelah LAN siap
  udp.begin(localPort);
  Serial.printf("Receiver siap mendengar UDP di port %d\n", localPort);

  // Alokasi Task ke Core
  xTaskCreatePinnedToCore(taskLAN, "TaskLAN", 4096, NULL, 1, NULL, 0);       
  xTaskCreatePinnedToCore(taskRotasi, "TaskRotasi", 4096, NULL, 1, NULL, 1); 
  xTaskCreatePinnedToCore(taskTranslasi, "TaskTranslasi", 4096, NULL, 1, NULL, 1); 
}

void loop() {
  // FreeRTOS menangani semuanya di background. Loop utama dibiarkan kosong.
  vTaskDelay(pdMS_TO_TICKS(1000));
}