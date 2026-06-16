#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <mcp2515.h>
#include <Arduino.h>

// Structure pour stocker un message CAN 
struct can_frame canMsg;

// Objet MCP2515 : contrôleur CAN connecté via SPI sur le pin CS 10
MCP2515 mcp2515(10);

// Identifiants du point d'accès Wi-Fi créé par l'ESP32
const char* ssid     = "VROOM-API";
const char* password = "12345678";

WiFiUDP Udp;
unsigned int localUdpPort = 8888;

// -------------------------------------------------------
// Lit un message CAN correspondant à un identifiant donné
// et extrait une valeur multi-octets à partir d'une liste
// d'indices de bytes dans le payload.
//
// Paramètres :
//   targetId  – identifiant CAN attendu (ex: 0xC9)
//   bytesList – tableau des indices de bytes à lire dans data[]
//   count     – nombre d'indices dans bytesList
//
// Retourne :
//   La valeur reconstituée (big-endian),
//   ou 0xFFFFFFFF en cas d'indice hors limites.
// -------------------------------------------------------
uint32_t readCAN(uint32_t targetId, uint8_t* bytesList, uint8_t count);

uint32_t readCAN(uint32_t targetId, uint8_t* bytesList, uint8_t count) {
    while (true) {
        if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
            // Ignore les messages dont l'ID ne correspond pas
            if (canMsg.can_id != targetId) continue;

            // Vérifie que tous les indices demandés sont dans les limites du payload
            for (uint8_t i = 0; i < count; i++) {
                if (bytesList[i] >= canMsg.can_dlc) return 0xFFFFFFFF;
            }

            // Reconstruit la valeur en concaténant les bytes (ordre big-endian)
            uint32_t result = 0;
            for (uint8_t i = 0; i < count; i++) {
                result = (result << 8) | canMsg.data[bytesList[i]];
            }
            return result;
        }
    }
}

// -------------------------------------------------------
// Lit le régime moteur (RPM) depuis le bus CAN.
//
// Protocole attendu :
//   ID 0xC9, bytes 1 et 2 → valeur brute sur 16 bits
//   RPM réel = valeur brute / 4  (résolution 0,25 tr/min)
//
// Retourne le RPM, ou -1 en cas d'erreur.
// -------------------------------------------------------
int getRPM() {
    uint8_t rpmBytes[] = {1, 2};
    uint32_t rpm = readCAN(0xC9, rpmBytes, 2);
    if (rpm != 0xFFFFFFFF) return rpm / 4;
    return -1;
}

int getDoor() {
    uint8_t Bytes[] = {1};
    uint32_t door = readCAN(0x12A, Bytes, 1);
    if(door == 0)return 0;
    if(door == 2)return 1;
}

int getTurnSignal() {
    uint8_t Bytes[] = {0};
    uint32_t turn = readCAN(0x361, Bytes, 1);
    if(turn == 1)return 0;

    if(turn > 63 && turn < 67) return 1;
    if(turn > 127 && turn < 131) return 2;

    if(turn > 31 && turn < 35) return -1;
    if(turn > 95 && turn < 99) return -2;
}

int getLightsCall() {
    uint8_t Bytes[] = {4};
    uint32_t var = readCAN(0x361, Bytes, 1);

    if(var > 30 && var < 34) return 0;
    if(var > 158 && var < 162) return 1;
}

int getLightsType() {
    uint8_t Bytes[] = {0};
    uint32_t var = readCAN(0x361, Bytes, 1);

    if(var == 1) return 1;
    if(var == 9) return 2;
    else return 0;
}

int getAutoLightsState(){
    uint8_t Bytes[] = {2};
    uint32_t var = readCAN(0x361, Bytes, 1);

    if(var == 0) return 0;
    if(var == 4) return 1;
  
}

int getWindShield(){
    uint8_t Bytes[] = {6};
    uint32_t var = readCAN(0x361, Bytes, 1);

    if(var == 0)return 0;

    if(var > 158 && var < 162) return 1;
    if(var > 174 && var < 178) return 2;

    if(var > 2006 && var < 210) return 3;
    
  
}

int getGlassWash(){
    uint8_t Bytes[] = {5};
    uint32_t var = readCAN(0x361, Bytes, 1);

    return var;  
}

int getSteeringWheel(){
    uint8_t Bytes[] = {1,2,4};
    uint32_t var = readCAN(0x1E5, Bytes, 3);
    int res = var/4096;
    int deg;
    if (res > 3000) deg = 4096 - res;
    else deg = (-1) * res;
    return deg;  
}

int getClutch(){
    uint8_t Bytes[] = {1};
    uint32_t var = readCAN(0x1F5, Bytes, 1);
    if (var < 66 && var > 62) return 1;
    else return 0;
}

int getBrakes(){
    uint8_t Bytes[] = {5};
    uint32_t var = readCAN(0x0C9, Bytes, 1);
    return var;
}

int getAccelerator(){
    uint8_t Bytes[] = {2,3};
    uint32_t var = readCAN(0x1BC, Bytes, 2);
    return var;
}

int getGear(){
    uint8_t Bytes[] = {3};
    uint32_t var = readCAN(0x1F5, Bytes, 1);
    if(var > 13 && var < 17) return 1;
    if(var > 0 && var < 4) return 2;
}


// Initialisation : port série, Wi-Fi AP, UDP, bus CAN
void setup() {
    Serial.begin(115200);

    // Crée un point d'accès Wi-Fi pour les clients
    WiFi.softAP(ssid, password);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    // Démarre le serveur UDP
    Udp.begin(localUdpPort);
    Serial.println("UDP server started on port 8888");

    // Initialise le contrôleur CAN MCP2515
    SPI.begin();
    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();
    Serial.println("CAN ready");
}

// Boucle principale : écoute les commandes UDP et répond
void loop() {
    int packetSize = Udp.parsePacket();
    if (!packetSize) return;

    // Lecture de la commande entrante
    char incoming[255];
    int len = Udp.read(incoming, 254);
    incoming[len] = '\0';

    // Supprime le \n final s'il est présent
    if (len > 0 && incoming[len - 1] == '\n') incoming[--len] = '\0';

    Serial.printf("Reçu: '%s'\n", incoming);

    char resp[32];

    if (strcmp(incoming, "GET_RPM") == 0) {
        int rpm = getRPM();
        if (rpm >= 0) snprintf(resp, sizeof(resp), "%d\n", rpm);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_DOOR") == 0) {
        int door = getDoor();
        if (door >= 0) snprintf(resp, sizeof(resp), "%d\n", door);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_TURN_SIGNAL") == 0) {
        int turn = getTurnSignal();
        if (turn >= 0) snprintf(resp, sizeof(resp), "%d\n", turn);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_LIGHTS_CALL") == 0) {
        int var = getLightsCall();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_LIGHTS_TYPE") == 0) {
        int var = getLightsType();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_AUTOLIGHTS_STATE") == 0) {
        int var = getAutoLightsState();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_FRONT_WINDSHIELD") == 0) {
        int var = getWindShield();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_FRONT_GLASS_WASH") == 0) {
        int var = getGlassWash();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_STEERING_WHEEL") == 0) {
        int var = getSteeringWheel();
        snprintf(resp, sizeof(resp), "%d\n", var);
    }
    else if (strcmp(incoming, "GET_CLUTCH") == 0) {
        int var = getClutch();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_BRAKES") == 0) {
        int var = getBrakes();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_ACCELERATOR") == 0) {
        int var = getAccelerator();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else if (strcmp(incoming, "GET_GEAR") == 0) {
        int var = getGear();
        if (var >= 0) snprintf(resp, sizeof(resp), "%d\n", var);
        else          snprintf(resp, sizeof(resp), "ERROR\n");
    }
    else {
        snprintf(resp, sizeof(resp), "UNKNOWN_CMD\n");
    }

    // Envoi de la réponse à l'expéditeur
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write((uint8_t*)resp, strlen(resp));
    Udp.endPacket();

    Serial.printf("Envoyé: '%s'\n", resp);
}