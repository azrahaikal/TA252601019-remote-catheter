#include <ETH.h>
#include <WiFiUdp.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// nema 23
#define STEP_PIN_23 12
#define DIR_PIN_23 14
const int JEDA_23 = 1200;
const long PPR_23 = 400;
const float RASIO_23 = 5.0;

// nema 17 tmc2209
#define STEP_PIN_17 32
#define DIR_PIN_17 33
#define EN_PIN_17 16
const int JEDA_17 = 800;
const long FULL_STEP_17 = 200;
const int MICROSTEP_17 = 8;
const long PPR_17 = FULL_STEP_17 * MICROSTEP_17; // 1600
const float RASIO_17 = 1.0;

// global variables
volatile float targetRotasi = 0.0;
volatile float targetTranslasi = 0.0;

float posisiRotasi = 0.0;
float posisiTranslasi = 0.0;
float sisaStepTranslasi = 0.0;

const float TOLERANSI_NOISE = 0.5;
SemaphoreHandle_t dataMutex;

// lan
IPAddress local_ip(192, 168, 1, 20);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
unsigned int localPort = 8888;
WiFiUDP udp;
char packetBuffer[255];

// core 0: menerima data dari LAN
void taskLAN(void *pvParameters) {
  for (;;) {
    if (ETH.linkUp()) {
      int packetSize = udp.parsePacket();
      if (packetSize) {
        int len = udp.read(packetBuffer, 255);
        if (len > 0) packetBuffer[len] = 0;
        
        String dataMasuk = String(packetBuffer);
        dataMasuk.trim();
        int commaIndex = dataMasuk.indexOf(',');
        
        if (commaIndex > 0) {
          float dataS1 = dataMasuk.substring(0, commaIndex).toFloat();
          float dataS2 = dataMasuk.substring(commaIndex + 1).toFloat();
          
          if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // print
            if (targetRotasi != dataS1 || targetTranslasi != dataS2) {
               Serial.print("Data diterima -> Rotasi: "); Serial.print(dataS1);
               Serial.print(" | Translasi: "); Serial.println(dataS2);
            }
            targetRotasi = dataS1;
            targetTranslasi = dataS2;
            xSemaphoreGive(dataMutex);
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Jeda 10ms
  }
}

// --- core 1: Nema 23
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
        
        if (i % 100 == 0) vTaskDelay(pdMS_TO_TICKS(1)); 
      }
      posisiRotasi = localTarget;
      Serial.println("[NEMA 23] Tiba di tujuan.");
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// --- core 1: Nema 17
void taskTranslasi(void *pvParameters) {
  for (;;) {
    float localTarget = posisiTranslasi;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      localTarget = targetTranslasi;
      xSemaphoreGive(dataMutex);
    }

    float selisih = localTarget - posisiTranslasi;
    
    // Opsional: Pembatasan sudut 360 derajat. Jika Nema 17 digunakan untuk 
    // sistem translasi linear (rel maju-mundur), logika ini sebenarnya TIDAK diperlukan 
    // karena jarak linear bisa akumulatif melebihi 360 derajat.
    if (selisih > 180.0) selisih -= 360.0;
    else if (selisih < -180.0) selisih += 360.0;

    if (abs(selisih) > TOLERANSI_NOISE) { 
      Serial.print("[NEMA 17] Bergerak mengejar delta: "); Serial.println(selisih);
      
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
        
        // Jeda sangat tipis agar tidak bentrok dengan Nema 23 di Core 1
        if (i % 100 == 0) vTaskDelay(pdMS_TO_TICKS(1)); 
      }
      posisiTranslasi = localTarget;
      Serial.println("[NEMA 17] Tiba di tujuan.");
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // pin motor
  pinMode(STEP_PIN_23, OUTPUT);
  pinMode(DIR_PIN_23, OUTPUT);
  pinMode(STEP_PIN_17, OUTPUT);
  pinMode(DIR_PIN_17, OUTPUT);
  pinMode(EN_PIN_17, OUTPUT);
  
  // inisialisasi state awal motor
  digitalWrite(STEP_PIN_23, HIGH);
  digitalWrite(DIR_PIN_23, HIGH);
  digitalWrite(EN_PIN_17, LOW); 
  digitalWrite(STEP_PIN_17, LOW);
  digitalWrite(DIR_PIN_17, LOW);
  
  dataMutex = xSemaphoreCreateMutex();
  
  // setup lan
  Serial.println("Memulai Ethernet...");
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);

  // buat thread
  xTaskCreatePinnedToCore(taskLAN, "TaskLAN", 4096, NULL, 1, NULL, 0);       
  xTaskCreatePinnedToCore(taskRotasi, "TaskRotasi", 4096, NULL, 1, NULL, 1); 
  xTaskCreatePinnedToCore(taskTranslasi, "TaskTranslasi", 4096, NULL, 1, NULL, 1); 
  
  Serial.println("receiver siap...");
}

void loop() {
  // kosong
}