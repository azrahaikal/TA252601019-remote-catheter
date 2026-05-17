#include <ETH.h>
#include <WiFiUdp.h>

// konfigurasi ip address
IPAddress local_ip(192, 168, 1, 20);    // yg ini
IPAddress gateway(192, 168, 1, 1);      // yg sana
IPAddress subnet(255, 255, 255, 0);

unsigned int localPort = 8888;
WiFiUDP udp;
char packetBuffer[255]; 

void setup() {
  Serial.begin(115200);
  
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);
  
  Serial.println("System Receiver Siap. Menunggu Data Sensor...");
}

void loop() {
  if (ETH.linkUp()) {
    int packetSize = udp.parsePacket();
    
    if (packetSize) {
      int len = udp.read(packetBuffer, 255);
      if (len > 0) {
        packetBuffer[len] = 0; // Null terminate string
      }
      
      // Mengubah array karakter menjadi String Arduino
      String dataMasuk = String(packetBuffer);
      Serial.println(dataMasuk);

    }
  } else {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 2000) {
        Serial.println("Menunggu koneksi LAN...");
        lastCheck = millis();
    }
  }
}