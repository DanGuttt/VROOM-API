#include "udp_client.h"
#include <stdio.h>
#include <string.h>
int main() {
    setup_network();

    char response[256];
    int len = send_udp_request("GET_RPM", response, sizeof(response));
    printf("%s",response);

    #ifdef _WIN32
        closesocket(sock);
        WSACleanup();
    #else
        close(sock);
    #endif

    return 0;
}