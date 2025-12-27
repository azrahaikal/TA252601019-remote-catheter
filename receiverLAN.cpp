#include <ETH.h>
#include <WiFiUdp.h>

// --- KONFIGURASI IP (PENERIMA) ---
IPAddress local_ip(192, 168, 1, 20);    // IP ESP32 ini (Beda dengan Pengirim)
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Port UDP (Harus sama dengan pengirim)
unsigned int localPort = 8888;

WiFiUDP udp;
char packetBuffer[255]; // Buffer untuk menyimpan data masuk

void setup() {
  Serial.begin(115200);
  
  // 1. Mulai Ethernet
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  
  // 2. Set Static IP
  ETH.config(local_ip, gateway, subnet);
  
  // 3. Mulai UDP
  udp.begin(localPort);
  
  Serial.println("System Receiver Siap. Menunggu Data...");
}

void loop() {
  if (ETH.linkUp()) {
    // Cek apakah ada paket data masuk
    int packetSize = udp.parsePacket();
    
    if (packetSize) {
      // Baca data yang masuk
      int len = udp.read(packetBuffer, 255);
      if (len > 0) {
        packetBuffer[len] = 0; // Null terminate string
      }
      
      Serial.print("Diterima dari IP: ");
      Serial.print(udp.remoteIP());
      Serial.print(" | Pesan: ");
      Serial.println(packetBuffer);
    }
  } else {
    // Hanya print sesekali agar serial monitor tidak penuh
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 2000) {
        Serial.println("Menunggu koneksi LAN...");
        lastCheck = millis();
    }
  }
}