#include <ETH.h>
#include <WiFiUdp.h>

// konfigurasi ip address
IPAddress local_ip(192, 168, 1, 10);    // yg ini
IPAddress receiver_ip(192, 168, 1, 20); // ini penerima
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned int localPort = 8888;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  
  // 2. Inisialisasi Ethernet
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, -1, ETH_CLOCK_GPIO0_IN);
  ETH.config(local_ip, gateway, subnet);
  udp.begin(localPort);
  
  Serial.println("System Sender Siap. Menunggu Link Ethernet...");
  delay(2000);
}

void loop() {
  // Jika LAN terhubung, kirim data
  if (ETH.linkUp()) {
    // Format data: "derajatRotasi,panjangCmTranslasi" (Contoh: "120.5,45.2")
    // esp32 dan cpp defaultnya adalah utf-8
    // Serial.print(payload, BIN); // untuk debug payload dalam format biner
    // ukur di TX_EN aja, karena konversi ke sinyal diferensial perlu proses.
    // TX_EN high saat MAC esp32 sedang mengirim data ke chip LAN8720
    // CRS_DV high saat chip LAN8720 menerima data yang masuk
    // CH1 osiloskop --> TX_EN, tunggu saat rising edge
    // CH2 osiloskop --> CRS_DV, tunggu di falling edge
    // ukur waktu antara rising edge dan falling edge
    String payload = "0.0,0.0"; // minimal 18 byte
    // untuk tiap test case, ambil data 4 kali, lalu rata-ratakan.
    // test case:
    // abcdefg --> 7 byte
    // abcdefghij --> 10 byte
    // abcdefghijklm --> 13 byte
    // abcdefghijklmnop --> 16 byte
    // abcdefghijklmnopqr --> 18 byte

    // abcdefghijklmnopqrstu --> 21 byte
    // abcdefghijklmnopqrstuvwx --> 24 byte
    // abcdefghijklmnopqrstuvwxyzA --> 27 byte
    // abcdefghijklmnopqrstuvwxyzABC --> 29 byte
    // abcdefghijklmnopqrstuvwxyzABCDEF --> 32 byte
    // abcdefghijklmnopqrstuvwxyzABCDEFGHI --> 35 byte
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL --> 38 byte
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNO --> 41 byte
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR --> 44 byte
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU --> 47 byte
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX --> 50 byte
    
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
  
  // delay ms
  delay(1000); 
}