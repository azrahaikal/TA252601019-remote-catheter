#include <ETH.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <AS5600.h>

// --- KONFIGURASI I2C & SENSOR ---
#define SDA_1 32
#define SCL_1 33
#define SDA_2 13
#define SCL_2 14

AS5600 as5600_1(&Wire);  // Sensor 1 di Bus 0
AS5600 as5600_2(&Wire1); // Sensor 2 di Bus 1

// --- KONFIGURASI IP (PENGIRIM) ---
IPAddress local_ip(192, 168, 1, 10);    // IP ESP32 ini
IPAddress receiver_ip(192, 168, 1, 20); // IP Tujuan (Penerima)
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned int localPort = 8888;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  
  // 1. Inisialisasi I2C
  Wire.begin(SDA_1, SCL_1);
  Wire1.begin(SDA_2, SCL_2);

  as5600_1.begin(); 
  as5600_2.begin(); 

  Serial.println("Cek Koneksi Sensor...");
  Serial.print("Sensor 1 (Pin 32/33): ");
  Serial.println(as5600_1.isConnected() ? "Terhubung!" : "Gagal terhubung.");
  Serial.print("Sensor 2 (Pin 13/14): ");
  Serial.println(as5600_2.isConnected() ? "Terhubung!" : "Gagal terhubung.");
  
  // 2. Inisialisasi Ethernet
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);
  
  Serial.println("System Sender Siap. Menunggu Link Ethernet...");
  delay(2000);
}

void loop() {
  // Membaca sudut mentah dan konversi ke Derajat
  uint16_t rawAngle1 = as5600_1.readAngle();
  uint16_t rawAngle2 = as5600_2.readAngle();

  float degree1 = rawAngle1 * (360.0 / 4096.0);
  float degree2 = rawAngle2 * (360.0 / 4096.0);

  // Jika LAN terhubung, kirim data
  if (ETH.linkUp()) {
    // Format data: "Derajat1,Derajat2" (Contoh: "120.5,45.2")
    String payload = String(degree1, 1) + "," + String(degree2, 1);
    
    // Kirim Paket UDP
    udp.beginPacket(receiver_ip, localPort);
    udp.print(payload);
    udp.endPacket();
    
    // Print ke Serial Monitor
    Serial.print("Mengirim via LAN -> S1: ");
    Serial.print(degree1, 1);
    Serial.print("° | S2: ");
    Serial.print(degree2, 1);
    Serial.println("°");
  } else {
    Serial.println("Kabel LAN belum terhubung...");
  }
  
  // Waktu tunggu pengiriman. Anda bisa mengubahnya (misal: 10ms - 50ms) 
  // sesuai kebutuhan responsivitas sistem Anda.
  delay(500); 
}