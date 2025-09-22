#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT "27015"

// Parametri za dinamički test
const int NUM_CYCLES = 5; // Koliko puta ponavljamo ciklus punjenja i pražnjenja
const int ALLOCATIONS_PER_CYCLE = 15000; // Koliko alokacija radimo u jednom ciklusu

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { return 1; }

    addrinfo* result = NULL;
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    getaddrinfo(SERVER_ADDRESS, SERVER_PORT, &hints, &result);

    SOCKET connect_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (connect(connect_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        printf("KLIJENT (Dynamic): Ne mogu da se povezem na server.\n");
        WSACleanup();
        system("pause");
        return 1;
    }
    freeaddrinfo(result);

    printf("KLIJENT (Dynamic): Povezan. Zapocinjem dinamicki test sa %d ciklusa.\n", NUM_CYCLES);
    srand(static_cast<unsigned int>(time(NULL)) ^ GetCurrentThreadId());

    std::vector<void*> allocated_pointers;
    allocated_pointers.reserve(ALLOCATIONS_PER_CYCLE); // Optimizacija: unapred rezervišemo prostor

    for (int cycle = 0; cycle < NUM_CYCLES; ++cycle) {
        printf("KLIJENT (Dynamic): Ciklus %d - Faza alokacije...\n", cycle + 1);
        // --- FAZA ALOKACIJE ---
        for (int i = 0; i < ALLOCATIONS_PER_CYCLE; ++i) {
            char send_buffer[512], recv_buffer[512];
            int size = 16 + (rand() % 512);
            sprintf_s(send_buffer, sizeof(send_buffer), "ALLOCATE %d", size);
            send(connect_socket, send_buffer, (int)strlen(send_buffer), 0);

            int recv_len = recv(connect_socket, recv_buffer, sizeof(recv_buffer), 0);
            if (recv_len > 0) {
                recv_buffer[recv_len] = '\0';
                void* ptr;
                sscanf_s(recv_buffer, "%p", &ptr);
                if (ptr != nullptr) {
                    allocated_pointers.push_back(ptr);
                }
            }
        }

        Sleep(2000); // Pauza da vidimo punu memoriju na monitoru servera

        // --- FAZA OSLOBAĐANJA ---
        printf("KLIJENT (Dynamic): Ciklus %d - Faza oslobadjanja...\n", cycle + 1);
        for (void* ptr : allocated_pointers) {
            char send_buffer[512], recv_buffer[512];
            sprintf_s(send_buffer, sizeof(send_buffer), "FREE %p", ptr);
            send(connect_socket, send_buffer, (int)strlen(send_buffer), 0);
            recv(connect_socket, recv_buffer, sizeof(recv_buffer), 0); // Čekamo "OK"
        }
        allocated_pointers.clear();

        Sleep(2000); // Pauza da vidimo praznu memoriju
    }

    printf("KLIJENT (Dynamic): Stres test zavrsen.\n");
    closesocket(connect_socket);
    WSACleanup();
    return 0;
}