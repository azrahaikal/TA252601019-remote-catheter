#include <ETH.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <AS5600.h>

#define SDA_1 32 // rotasi
#define SCL_1 33 // translasi
#define SDA_2 13
#define SCL_2 14

AS5600 as5600_1(&Wire);  // rotasi
AS5600 as5600_2(&Wire1); // translasi

uint16_t rawSebelumnya1 = 0;
uint16_t rawSebelumnya2 = 0;

long totalRaw1 = 0; // 32 bit
long totalRaw2 = 0;

// ip
IPAddress local_ip(192, 168, 1, 10);    // yg ini
IPAddress receiver_ip(192, 168, 1, 20); // yg tujuan
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned int localPort = 8888;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  pinMode(17, OUTPUT);
  digitalWrite(17, HIGH);
  
  //init i2c
  Wire.begin(SDA_1, SCL_1);
  Wire1.begin(SDA_2, SCL_2);

  as5600_1.begin(); 
  as5600_2.begin(); 

  Serial.println("Cek Koneksi Sensor...");
  Serial.print("Sensor 1 (Pin 32/33): ");
  Serial.println(as5600_1.isConnected() ? "Terhubung!" : "Gagal terhubung.");
  Serial.print("Sensor 2 (Pin 13/14): ");
  Serial.println(as5600_2.isConnected() ? "Terhubung!" : "Gagal terhubung.");
  
  // zeroing
  rawSebelumnya1 = as5600_1.readAngle();
  rawSebelumnya2 = as5600_2.readAngle();
  totalRaw1 = 0; 
  totalRaw2 = 0;
  Serial.println("Kalibrasi Titik 0 Berhasil!");

  // init eth
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);
  
  Serial.println("System Sender Siap. Menunggu Link Ethernet...");
  delay(2000);
}

void loop() {
  uint16_t rawSekarang1 = as5600_1.readAngle();
  uint16_t rawSekarang2 = as5600_2.readAngle();

  long delta1 = rawSekarang1 - rawSebelumnya1;
  long delta2 = rawSekarang2 - rawSebelumnya2;

  if (delta1 < -2048) {
    delta1 += 4096;
  } else if (delta1 > 2048) {
    delta1 -= 4096;
  }

  if (delta2 < -2048) {
    delta2 += 4096;
  } else if (delta2 > 2048) {
    delta2 -= 4096;
  }


  totalRaw1 += delta1;
  totalRaw2 += delta2;

  rawSebelumnya1 = rawSekarang1;
  rawSebelumnya2 = rawSekarang2;

  // ini derajat
  float degree1 = totalRaw1 * (360.0 / 4096.0);
  
  // ubah rotasi ke translasi
  float translation2 = (totalRaw2 / 4096.0) * (PI * 25.0);

  // kirim data
  if (ETH.linkUp()) {
    // derajat, milimeter
    String payload = String(degree1, 2) + "," + String(translation2, 2); 
    
    udp.beginPacket(receiver_ip, localPort);
    udp.print(payload);
    udp.endPacket();
    
    Serial.print("Kirim LAN -> S1 (Rotasi): ");
    Serial.print(degree1, 2);
    Serial.print("° | S2 (Translasi): ");
    Serial.print(translation2, 2);
    Serial.println(" mm");
  } else {
    Serial.println("Kabel LAN belum terhubung...");
  }
  
  delay(100); 
}