#include <ETH.h>
#include <WiFiUdp.h>

// --- KONFIGURASI IP (PENERIMA) ---
IPAddress local_ip(192, 168, 1, 20);    // IP ESP32 ini
IPAddress gateway(192, 168, 1, 1);
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
      
      // Mencari posisi tanda koma
      int commaIndex = dataMasuk.indexOf(',');
      
      if (commaIndex > 0) {
        // Memecah string dan mengubahnya menjadi angka float (desimal)
        float dataSensor1 = dataMasuk.substring(0, commaIndex).toFloat();
        float dataSensor2 = dataMasuk.substring(commaIndex + 1).toFloat();
        
        // Print hasil ekstraksi (Sekarang data berupa angka, siap digunakan)
        Serial.print("Data Diekstrak -> S1: ");
        Serial.print(dataSensor1, 1);
        Serial.print("° \t|\t S2: ");
        Serial.print(dataSensor2, 1);
        Serial.println("°");
      }
    }
  } else {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 2000) {
        Serial.println("Menunggu koneksi LAN...");
        lastCheck = millis();
    }
  }
}