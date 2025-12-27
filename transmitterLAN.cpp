#include <ETH.h>
#include <WiFiUdp.h>

// --- KONFIGURASI IP (PENGIRIM) ---
IPAddress local_ip(192, 168, 1, 10);    // IP ESP32 ini
IPAddress receiver_ip(192, 168, 1, 20); // IP Tujuan (Penerima)
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Port UDP
unsigned int localPort = 8888;

// Objek UDP
WiFiUDP udp;

// Variabel data dummy
int counter = 0;

void setup() {
  Serial.begin(115200);
  
  // 1. Mulai Ethernet (Sesuai perbaikan sebelumnya)
  // Urutan: Type, Address, MDC, MDIO, Power, Clock
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  
  // 2. Set Static IP
  ETH.config(local_ip, gateway, subnet);
  
  // 3. Mulai UDP
  udp.begin(localPort);
  
  Serial.println("System Sender Siap. Menunggu Link Ethernet...");
}

void loop() {
  // Cek apakah kabel LAN terhubung
  if (ETH.linkUp()) {
    
    // Siapkan data string
    String message = "Data ke-" + String(counter);
    
    // Kirim Paket UDP
    udp.beginPacket(receiver_ip, localPort);
    udp.print(message);
    udp.endPacket();
    
    Serial.print("Mengirim: ");
    Serial.println(message);
    
    counter++;
  } else {
    Serial.println("Kabel LAN belum terhubung...");
  }
  
  delay(1000); // Kirim setiap 1 detik
}