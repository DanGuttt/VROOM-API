#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#ifdef ESP32
#include <WiFi.h>
#include <WiFiUdp.h>
#else
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#endif

#define SERVER_IP "192.168.4.1"
#define SERVER_PORT 8888

#ifdef ESP32
extern WiFiUDP udp;
extern const char* ssid;
extern const char* password;
#else
extern int sock;
extern struct sockaddr_in server_addr;
#endif

// Initialise le réseau (WiFi ou sockets)
void setup_network();

// Envoie une requête UDP et lit la réponse
int send_udp_request(const char* msg, char* response, int response_size);

#endif // UDP_CLIENT_H
