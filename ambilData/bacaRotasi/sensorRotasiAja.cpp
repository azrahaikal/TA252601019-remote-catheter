#include <Wire.h>
#include <AS5600.h>

// pin i2c
#define SDA_1 32
#define SCL_1 33

AS5600 as5600_1(&Wire);  // Sensor 1 di Bus 0

// variabel untuk melacak putaran kontinu
long totalPosition = 0;      // total posisi (bisa negatif/positif besar)
uint16_t lastRawAngle = 0;   // nilai pembacaan terakhir

// variabel untuk interval print non-blocking
unsigned long lastPrintTime = 0;
const int printInterval = 1000;

void setup() {
  Serial.begin(115200);
  
  // 1. Inisialisasi I2C
  Wire.begin(SDA_1, SCL_1);
  as5600_1.begin(); 
  Serial.print("Sensor 1 (Pin 32/33): ");
  Serial.println(as5600_1.isConnected() ? "Terhubung!" : "Gagal terhubung.");

  // baca posisi awal sebagai titik referensi pertama
  if (as5600_1.isConnected()) {
    lastRawAngle = as5600_1.readAngle();
    totalPosition = lastRawAngle; // Set posisi awal
  }

  delay(2000);
}

void loop() {
  // 1. baca sudut mentah saat ini (12bit, 0 - 4095)
  uint16_t currentRawAngle = as5600_1.readAngle();
  
  // 2. Hitung selisih dari pembacaan sebelumnya
  long diff = currentRawAngle - lastRawAngle;

  // 3. Deteksi wrap-around (melewati titik 0)
  // 2048 adalah setengah dari resolusi maksimum (4096 / 2)
  if (diff < -2048) {
    // Jika selisih negatif besar, berarti sensor berputar maju melewati 4095 ke 0
    diff += 4096;
  } else if (diff > 2048) {
    // Jika selisih positif besar, berarti sensor berputar mundur melewati 0 ke 4095
    diff -= 4096;
  }

  // 4. Tambahkan selisih ke total posisi
  totalPosition += diff;
  
  // 5. Update pembacaan terakhir
  lastRawAngle = currentRawAngle;

  // 6. Print ke Serial Monitor tanpa memblokir pembacaan sensor
  // millis() adalah unsigned long (32 bit, 4 miliar ms). overflow saat 49 hari.
  if (millis() - lastPrintTime >= printInterval) {
    // Konversi total posisi ke Derajat
    float continuousDegree = totalPosition * (360.0 / 4096.0);
    
    Serial.print("Sudut Kontinu: ");
    Serial.print(continuousDegree, 1);
    Serial.println("°");
    
    lastPrintTime = millis();
  }
}