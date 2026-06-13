#include <ETH.h>
#include <WiFiUdp.h>

// konfigurasi ip address
IPAddress local_ip(192, 168, 1, 10);    // yg ini
IPAddress receiver_ip(255, 255, 255, 255); // ini penerima
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned int localPort = 8888;
WiFiUDP udp;
unsigned long packet_id = 0;

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
    String payload = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX12345";; // minimal 18 byte | 6.65 us, 6.65 us
    //packet_id++;
    Serial.println(payload);
    // untuk tiap test case, ambil data 4 kali, lalu rata-ratakan. SAMA AJA, gk harus 4 kali
    // test case:
    // abcdefg --> 7 byte | 6.65 us, 6.65 us
    // abcdefghij --> 10 byte | 6.65 us us, 6.65 us
    // abcdefghijklm --> 13 byte | 6.65 us, 6.65 us
    // abcdefghijklmnop --> 16 byte |  us
    // abcdefghijklmnopqr --> 18 byte | 6.65 us, 6.65 us

    // abcdefghijklmnopqrstu --> 21 byte | 6.9 us, 6.9 us
    // abcdefghijklmnopqrstuvwx --> 24 byte | 
    // abcdefghijklmnopqrstuvwxyzA --> 27 byte |
    // abcdefghijklmnopqrstuvwxyzABC --> 29 byte |
    // abcdefghijklmnopqrstuvwxyzABCDEF --> 32 byte | 7.75 us, 7.75 us
    // abcdefghijklmnopqrstuvwxyzABCDEFGHI --> 35 byte |
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKL --> 38 byte |
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNO --> 41 byte |
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQR --> 44 byte | 8.74 us, 8.74 us
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTU --> 47 byte |
    // abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWX --> 50 byte | 9.24 us, 9.24 us
    // --> 100 byte | 13.2 us, 13.2 us
    // --> 200 byte | 21.2 us. 21.2 us
    // --> 255 byte | 25.6 us, 25.6 us
    
    // Kirim Paket UDP
    udp.beginPacket(receiver_ip, localPort);
    udp.print(payload);
    udp.endPacket();
    if (packet_id >= 4294967295) {
      packet_id = 0;
    }
    
  } else {
    Serial.println("Kabel LAN belum terhubung...");
  }
  
  // delay ms
  delay(500); 
}