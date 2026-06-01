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

// --- VARIABEL MULTI-TURN & ZEROING ---
uint16_t rawSebelumnya1 = 0;
uint16_t rawSebelumnya2 = 0;

long totalRaw1 = 0; // Menggunakan long agar bisa menampung putaran tak terhingga
long totalRaw2 = 0;

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
  
  // ==========================================
  // PROSES ZEROING (KALIBRASI AWAL KE 0)
  // ==========================================
  // Membaca posisi fisik sensor saat ESP32 pertama kali menyala
  // Posisi ini akan dijadikan titik acuan "0"
  rawSebelumnya1 = as5600_1.readAngle();
  rawSebelumnya2 = as5600_2.readAngle();
  totalRaw1 = 0; 
  totalRaw2 = 0;
  Serial.println("Kalibrasi Titik 0 Berhasil!");

  // 2. Inisialisasi Ethernet
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);
  
  Serial.println("System Sender Siap. Menunggu Link Ethernet...");
  delay(2000);
}

void loop() {
  // 1. Baca data mentah saat ini (0-4095)
  uint16_t rawSekarang1 = as5600_1.readAngle();
  uint16_t rawSekarang2 = as5600_2.readAngle();

  // 2. Hitung selisih (delta) pergerakan
  long delta1 = rawSekarang1 - rawSebelumnya1;
  long delta2 = rawSekarang2 - rawSebelumnya2;

  // 3. Logika Penanganan Lompatan Putaran (Wrap-Around)
  // Jika sensor melewati batas 4095 ke 0, delta akan bernilai sangat negatif
  if (delta1 < -2048) {
    delta1 += 4096; // Koreksi putaran maju
  } 
  // Jika sensor melewati batas 0 ke 4095, delta akan bernilai sangat positif
  else if (delta1 > 2048) {
    delta1 -= 4096; // Koreksi putaran mundur
  }

  if (delta2 < -2048) {
    delta2 += 4096;
  } else if (delta2 > 2048) {
    delta2 -= 4096;
  }

  // 4. Tambahkan delta ke total kumulatif
  totalRaw1 += delta1;
  totalRaw2 += delta2;

  // 5. Update memori raw sebelumnya untuk putaran loop berikutnya
  rawSebelumnya1 = rawSekarang1;
  rawSebelumnya2 = rawSekarang2;

  // 6. Konversi total raw absolut menjadi derajat (bisa minus & >360)
  float degree1 = totalRaw1 * (360.0 / 4096.0);
  float degree2 = totalRaw2 * (360.0 / 4096.0);

  // --- MENGIRIM DATA ---
  if (ETH.linkUp()) {
    String payload = String(degree1, 2) + "," + String(degree2, 2); // Menggunakan 2 angka di belakang koma untuk presisi
    
    udp.beginPacket(receiver_ip, localPort);
    udp.print(payload);
    udp.endPacket();
    
    Serial.print("Kirim LAN -> S1: ");
    Serial.print(degree1, 2);
    Serial.print("° | S2: ");
    Serial.print(degree2, 2);
    Serial.println("°");
  } else {
    Serial.println("Kabel LAN belum terhubung...");

    Serial.println("Cek Koneksi Sensor...");
    Serial.print("Sensor 1 (Pin 32/33): ");
    Serial.println(as5600_1.isConnected() ? "Terhubung!" : "Gagal terhubung.");
    Serial.print("Sensor 2 (Pin 13/14): ");
    Serial.println(as5600_2.isConnected() ? "Terhubung!" : "Gagal terhubung.");
    
    // ==========================================
    // PROSES ZEROING (KALIBRASI AWAL KE 0)
    // ==========================================
    // Membaca posisi fisik sensor saat ESP32 pertama kali menyala
    // Posisi ini akan dijadikan titik acuan "0"
    rawSebelumnya1 = as5600_1.readAngle();
    rawSebelumnya2 = as5600_2.readAngle();
    totalRaw1 = 0; 
    totalRaw2 = 0;
    Serial.println("Kalibrasi Titik 0 Berhasil!");

    ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
    ETH.config(local_ip, gateway, subnet);
    udp.begin(localPort);
  }
  
  delay(100); 
}