#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "Ayman.Elbadawy";
const char* password = "13909964";

IPAddress pcIP(192, 168, 1, 4);

const uint16_t udpPortESP = 8888;
const uint16_t udpPortPC  = 9999;

WiFiUDP udp;

// 1. The Raw UDP structure coming from ROS2 (16 bytes)
// __attribute__((packed)) guarantees the compiler won't add hidden padding
struct __attribute__((packed)) RosCommandPacket
{
    float roll;
    float pitch;
    float yaw;
    float throttle;
};

// 2. The Framed UART structure going to the STM32 (19 bytes)
struct __attribute__((packed)) CommandPacket
{
    uint16_t header;
    float roll;
    float pitch;
    float yaw;
    float throttle;
    uint8_t checksum;
};

struct __attribute__((packed)) HeartbeatPacket
{
    uint32_t counter;
};

RosCommandPacket incomingRosCommand;
CommandPacket stmCommand;

bool rosConnected = false;
uint32_t heartbeatCounter = 0;

unsigned long lastCommandTime = 0;
const uint32_t commandTimeout = 500;

unsigned long lastHeartbeat = 0;
const uint32_t heartbeatPeriod = 100;

// Simple XOR Checksum calculator
uint8_t calculate_checksum(CommandPacket* packet) {
    uint8_t* ptr = (uint8_t*)packet;
    uint8_t crc = 0;
    // XOR everything except the last byte (which is the checksum itself)
    for (int i = 0; i < sizeof(CommandPacket) - 1; i++) {
        crc ^= ptr[i];
    }
    return crc;
}

void setup()
{
    Serial.begin(115200);

    // TX = 17, RX = 16 (We only transmit to STM32, but initialize both)
    Serial2.begin(115200, SERIAL_8N1, 16, 17);

    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("ESP32 IP address: ");
    Serial.println(WiFi.localIP());

    udp.begin(udpPortESP);

    Serial.print("UDP server listening on port ");
    Serial.println(udpPortESP);

    // Lock in the fixed hardware sync header
    stmCommand.header = 0xA55A;
}

void loop()
{
    int size = udp.parsePacket();

    // Check if the incoming UDP packet is exactly the 16 bytes we expect from the PC
    if (size == sizeof(RosCommandPacket))
    {
        // 1. Read the raw floats
        udp.read((uint8_t*)&incomingRosCommand, sizeof(RosCommandPacket));

        // 2. Transfer data into the framed STM32 struct
        stmCommand.roll = incomingRosCommand.roll;
        stmCommand.pitch = incomingRosCommand.pitch;
        stmCommand.yaw = incomingRosCommand.yaw;
        stmCommand.throttle = incomingRosCommand.throttle;

        // 3. Calculate the checksum of the newly framed packet
        stmCommand.checksum = calculate_checksum(&stmCommand);

        // 4. Blast the fully synchronized 19-byte packet to the STM32 DMA
        Serial2.write((uint8_t*)&stmCommand, sizeof(CommandPacket));

        rosConnected = true;
        lastCommandTime = millis();
    }

    // Update connection status based on timeout
    if (millis() - lastCommandTime > commandTimeout)
    {
        rosConnected = false;
        // The STM32 now has its own 500ms failsafe timer handling motor disarms,
        // so we don't need to manually send a zero-throttle packet here unless preferred.
    }

    // Fire off the heartbeat back to the PC
    if (millis() - lastHeartbeat > heartbeatPeriod)
    {
        lastHeartbeat = millis();

        HeartbeatPacket hb;
        hb.counter = heartbeatCounter++;

        udp.beginPacket(pcIP, udpPortPC);
        udp.write((uint8_t*)&hb, sizeof(hb));
        udp.endPacket();
    }
}