#include <Wire.h>
#include <AS5600.h>

// pin i2c
#define SDA_1 32
#define SCL_1 33

AS5600 as5600_1(&Wire);  // Sensor 1 di Bus 0

long totalPosition = 0;      
uint16_t lastRawAngle = 0;   

unsigned long lastPrintTime = 0;
const int printInterval = 1000; 

void setup() {
  Serial.begin(115200);
  
  // 1. Inisialisasi I2C
  Wire.begin(SDA_1, SCL_1);
  
  // kecepatan i2c
  Wire.setClock(400000); 

  as5600_1.begin(); 
  Serial.print("Sensor 1 (Pin 32/33): ");
  Serial.println(as5600_1.isConnected() ? "Terhubung!" : "Gagal terhubung.");

  if (as5600_1.isConnected()) {
    // bacaan pertama 0 derajat
    lastRawAngle = as5600_1.readAngle();
    totalPosition = 0; 
  }

  delay(2000);
}

void loop() {
  // baca sudut mentah saat ini
  uint16_t currentRawAngle = as5600_1.readAngle();
  
  long diff = (long)currentRawAngle - (long)lastRawAngle;

  // deteksi wrap-around
  if (diff < -2048) {
    diff += 4096;
  } else if (diff > 2048) {
    diff -= 4096;
  }

  // tambahkan selisih ke total posisi
  totalPosition += diff;
  
  // update pembacaan terakhir
  lastRawAngle = currentRawAngle;

  // print
  if (millis() - lastPrintTime >= printInterval) {
    float continuousDegree = totalPosition * (180.0 / 4096.0);
    
    Serial.print("Sudut Kontinu: ");
    Serial.print(continuousDegree, 1);
    Serial.println("°");
    
    lastPrintTime = millis();
  }
}