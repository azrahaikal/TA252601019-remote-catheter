#include <ETH.h>
#include <WiFiUdp.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// nema 23
#define STEP_PIN_23 12
#define DIR_PIN_23 14
const long PPR_23 = 400;
const float RASIO_23 = 5.0;

// nema 17 tmc2209
#define STEP_PIN_17 32
#define DIR_PIN_17 33
#define EN_PIN_17 16
const long FULL_STEP_17 = 200;
const int MICROSTEP_17 = 8;
const long PPR_17 = FULL_STEP_17 * MICROSTEP_17; // 1600
const float RASIO_17 = 1.0;

// parameter PID
float Kp_23 = 2.0, Ki_23 = 0.0, Kd_23 = 0.5;
float Kp_17 = 3.0, Ki_17 = 0.0, Kd_17 = 0.2;

// Batas Kecepatan (Jeda Mikrodetik antar Step)
const long MIN_JEDA = 400;   // Kecepatan Maksimal (Jangan terlalu kecil agar tidak stall)
const long MAX_JEDA = 10000; // Kecepatan Minimal (Saat mendekati target)
const float CONST_SPEED = 20000.0; // Konstanta pemetaan Output PID ke Jeda Waktu

// variabel global
volatile float targetRotasi = 0.0;
volatile float targetTranslasi = 0.0;

// Variabel Pelacakan Posisi (Menggunakan jumlah langkah absolut untuk mencegah akumulasi error desimal)
volatile long stepSaatIni_23 = 0;
volatile long stepSaatIni_17 = 0;

SemaphoreHandle_t dataMutex;

// LAN
IPAddress local_ip(192, 168, 1, 20);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
unsigned int localPort = 8888;
WiFiUDP udp;
char packetBuffer[255];


// core 0: LAN
void taskLAN(void *pvParameters) {
  for (;;) {
    if (ETH.linkUp()) {
      int packetSize = udp.parsePacket();
      if (packetSize > 0) {
        int len = udp.read(packetBuffer, 255);
        if (len > 0) packetBuffer[len] = 0;
        
        String dataMasuk = String(packetBuffer);
        dataMasuk.trim();
        int commaIndex = dataMasuk.indexOf(',');
        
        if (commaIndex > 0) {
          String idStr = dataMasuk.substring(0, commaIndex);
          String sisaData = dataMasuk.substring(commaIndex + 1);
          
          int secondComma = sisaData.indexOf(',');
          if (secondComma > 0) {
            float dataS1 = sisaData.substring(0, secondComma).toFloat();
            float dataS2 = sisaData.substring(secondComma + 1).toFloat();
            
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
              targetRotasi = dataS1;
              targetTranslasi = dataS2;
              xSemaphoreGive(dataMutex);
            }
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5)); // Responsivitas LAN ditingkatkan
  }
}

//  core 1: NEMA 23
void taskRotasi(void *pvParameters) {
  unsigned long waktuTerakhir = micros();
  unsigned long waktuStepTerakhir = micros();
  float integral = 0, errorSebelumnya = 0;
  
  for (;;) {
    float localTarget;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      localTarget = targetRotasi;
      xSemaphoreGive(dataMutex);
    }

    // Konversi Target Derajat ke Target Step Absolut
    long targetStep = (localTarget / 360.0) * PPR_23 * RASIO_23;
    long error = targetStep - stepSaatIni_23;

    if (abs(error) > 0) {
      unsigned long waktuSekarang = micros();
      float dt = (waktuSekarang - waktuTerakhir) / 1000000.0;
      if (dt <= 0) dt = 0.0001; 
      waktuTerakhir = waktuSekarang;

      // 1. Kalkulasi PID
      integral += error * dt;
      float derivative = (error - errorSebelumnya) / dt;
      float output = (Kp_23 * error) + (Ki_23 * integral) + (Kd_23 * derivative);
      errorSebelumnya = error;

      // 2. Tentukan Arah
      bool arahMaju = (output > 0);
      digitalWrite(DIR_PIN_23, arahMaju ? HIGH : LOW);

      // 3. Konversi Output PID ke Jeda Kecepatan
      float kecepatanTarget = abs(output);
      long jedaStep = MAX_JEDA; 
      if (kecepatanTarget > 0.1) {
          jedaStep = CONST_SPEED / kecepatanTarget;
      }
      
      // Clamp kecepatan agar tidak melampaui batas fisik motor
      if (jedaStep < MIN_JEDA) jedaStep = MIN_JEDA;
      if (jedaStep > MAX_JEDA) jedaStep = MAX_JEDA;

      // 4. Eksekusi Pulsa Non-Blocking
      if (waktuSekarang - waktuStepTerakhir >= jedaStep) {
        digitalWrite(STEP_PIN_23, HIGH);
        delayMicroseconds(2); // Pulsa pemicu sangat singkat
        digitalWrite(STEP_PIN_23, LOW);
        
        stepSaatIni_23 += (arahMaju ? 1 : -1);
        waktuStepTerakhir = micros();
      }
    } else {
      integral = 0; // Mencegah integral windup saat diam
    }
    
    // Memberikan napas CPU sangat singkat agar NEMA 17 dapat berjalan paralel
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}

// core 1: NEMA 17
void taskTranslasi(void *pvParameters) {
  unsigned long waktuTerakhir = micros();
  unsigned long waktuStepTerakhir = micros();
  float integral = 0, errorSebelumnya = 0;
  
  for (;;) {
    float localTarget;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
      localTarget = targetTranslasi;
      xSemaphoreGive(dataMutex);
    }

    long targetStep = (localTarget / 360.0) * PPR_17 * RASIO_17;
    long error = targetStep - stepSaatIni_17;

    if (abs(error) > 0) {
      unsigned long waktuSekarang = micros();
      float dt = (waktuSekarang - waktuTerakhir) / 1000000.0;
      if (dt <= 0) dt = 0.0001;
      waktuTerakhir = waktuSekarang;

      integral += error * dt;
      float derivative = (error - errorSebelumnya) / dt;
      float output = (Kp_17 * error) + (Ki_17 * integral) + (Kd_17 * derivative);
      errorSebelumnya = error;

      bool arahMaju = (output > 0);
      digitalWrite(DIR_PIN_17, arahMaju ? HIGH : LOW);

      float kecepatanTarget = abs(output);
      long jedaStep = MAX_JEDA; 
      if (kecepatanTarget > 0.1) {
          jedaStep = CONST_SPEED / kecepatanTarget;
      }
      
      if (jedaStep < MIN_JEDA) jedaStep = MIN_JEDA;
      if (jedaStep > MAX_JEDA) jedaStep = MAX_JEDA;

      if (waktuSekarang - waktuStepTerakhir >= jedaStep) {
        digitalWrite(STEP_PIN_17, HIGH);
        delayMicroseconds(2); 
        digitalWrite(STEP_PIN_17, LOW);
        
        stepSaatIni_17 += (arahMaju ? 1 : -1);
        waktuStepTerakhir = micros();
      }
    } else {
      integral = 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}

// setup
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(STEP_PIN_23, OUTPUT);
  pinMode(DIR_PIN_23, OUTPUT);
  pinMode(STEP_PIN_17, OUTPUT);
  pinMode(DIR_PIN_17, OUTPUT);
  pinMode(EN_PIN_17, OUTPUT);
  
  digitalWrite(STEP_PIN_23, LOW);
  digitalWrite(DIR_PIN_23, LOW);
  digitalWrite(EN_PIN_17, LOW); 
  digitalWrite(STEP_PIN_17, LOW);
  digitalWrite(DIR_PIN_17, LOW);
  
  dataMutex = xSemaphoreCreateMutex();
  
  Serial.println("\nMemulai Ethernet MAC/PHY...");
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  
  Serial.print("Mencari sinyal LAN...");
  while (!ETH.linkUp()) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nLAN Terhubung! IP dikonfigurasi.");
  
  udp.begin(localPort);
  Serial.printf("Receiver siap mendengar UDP di port %d\n", localPort);

  // Alokasi Task ke Core
  xTaskCreatePinnedToCore(taskLAN, "TaskLAN", 4096, NULL, 1, NULL, 0);       
  xTaskCreatePinnedToCore(taskRotasi, "TaskRotasi", 4096, NULL, 1, NULL, 1); 
  xTaskCreatePinnedToCore(taskTranslasi, "TaskTranslasi", 4096, NULL, 1, NULL, 1); 
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}