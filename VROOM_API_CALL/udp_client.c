#include "udp_client.h"
#include <stdio.h>
#include <string.h>

#ifdef ESP32
WiFiUDP udp;
const char* ssid = "ESP32-CAN-API";
const char* password = "12345678";
#else
int sock;
struct sockaddr_in server_addr;
#endif

void setup_network() {
#ifdef ESP32
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");
    udp.begin(12345); // port local
#else
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return;
    }
#endif

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
#endif
}

int send_udp_request(const char* msg, char* response, int response_size) {
#ifdef ESP32
    udp.beginPacket(SERVER_IP, SERVER_PORT);
    udp.write((uint8_t*)msg, strlen(msg));
    udp.endPacket();

    delay(100);
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read((uint8_t*)response, response_size - 1);
        response[len] = '\0';
        return len;
    } else {
        response[0] = '\0';
        return -1;
    }
#else
    if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("sendto failed");
        response[0] = '\0';
        return -1;
    }

    socklen_t addr_len = sizeof(server_addr);
    int recv_len = recvfrom(sock, response, response_size - 1, 0, (struct sockaddr*)&server_addr, &addr_len);
    if (recv_len < 0) {
        perror("recvfrom failed");
        response[0] = '\0';
        return -1;
    } else {
        response[recv_len] = '\0';
        return recv_len;
    }
#endif
}