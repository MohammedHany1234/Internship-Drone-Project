#include <WiFi.h>
#include <WiFiUdp.h>

const char* ap_ssid     = "ESP32_Network";
const char* ap_password = "12345678";   // must be 8+ characters

WiFiUDP udp;
const int udpPort = 4210;
char incomingPacket[255];

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ap_ssid, ap_password);
  IPAddress apIP = WiFi.softAPIP();

  Serial.println("Access Point started.");
  Serial.print("SSID: ");
  Serial.println(ap_ssid);
  Serial.print("ESP32 AP IP address: ");
  Serial.println(apIP);   // should print 192.168.4.1

  udp.begin(udpPort);
  Serial.printf("Listening for UDP packets on port %d\n", udpPort);
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(incomingPacket, 254);
    if (len > 0) incomingPacket[len] = 0;
    Serial.printf("Received: %s\n", incomingPacket);
  }
}