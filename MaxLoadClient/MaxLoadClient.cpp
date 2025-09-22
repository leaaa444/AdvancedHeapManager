#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT "27015"
#define MAX_ATTEMPTED_ALLOCATIONS 100000 // Pokušaće da napravi ovoliko alokacija

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
        printf("KLIJENT (MaxLoad): Ne mogu da se povezem na server.\n");
        WSACleanup();
        system("pause");
        return 1;
    }
    freeaddrinfo(result);

    printf("KLIJENT (MaxLoad): Povezan. Zapocinjem test maksimalnog opterecenja...\n");
    srand(static_cast<unsigned int>(time(NULL)) ^ GetCurrentThreadId());

    std::vector<void*> allocated_pointers;
    int successful_allocations = 0;
    int failed_allocations = 0;

    // --- FAZA ALOKACIJE: Samo alociraj dok god možeš ---
    for (int i = 0; i < MAX_ATTEMPTED_ALLOCATIONS; ++i) {
        char send_buffer[512], recv_buffer[512];
        int size = 16 + (rand() % 1024);
        sprintf_s(send_buffer, sizeof(send_buffer), "ALLOCATE %d", size);
        send(connect_socket, send_buffer, (int)strlen(send_buffer), 0);

        int recv_len = recv(connect_socket, recv_buffer, sizeof(recv_buffer), 0);
        if (recv_len > 0) {
            recv_buffer[recv_len] = '\0';
            void* ptr;
            sscanf_s(recv_buffer, "%p", &ptr);
            if (ptr != nullptr) {
                allocated_pointers.push_back(ptr);
                successful_allocations++;
            }
            else {
                failed_allocations++;
                // Ako 100 puta zaredom ne uspe alokacija, verovatno je sve puno
                if (failed_allocations > 100 && successful_allocations > 0) {
                    printf("KLIJENT (MaxLoad): Server je verovatno pun, prekidam alokaciju.\n");
                    break;
                }
            }
        }
    }

    printf("KLIJENT (MaxLoad): Faza alokacije gotova. Ceka se 5 sekundi...\n");
    Sleep(5000);

    // --- FAZA OSLOBAĐANJA ---
    printf("KLIJENT (MaxLoad): Ciscenje %zu alokacija...\n", allocated_pointers.size());
    for (void* ptr : allocated_pointers) {
        char send_buffer[512], recv_buffer[512];
        sprintf_s(send_buffer, sizeof(send_buffer), "FREE %p", ptr);
        send(connect_socket, send_buffer, (int)strlen(send_buffer), 0);
        recv(connect_socket, recv_buffer, sizeof(recv_buffer), 0);
    }

    // --- ISPIS STATISTIKE ---
    printf("====================================================\n");
    printf("KLIJENT (MaxLoad): Test gotov.\n");
    printf("Uspesne alokacije: %d\n", successful_allocations);
    printf("Neuspesne alokacije: %d\n", failed_allocations);
    printf("====================================================\n");

    closesocket(connect_socket);
    WSACleanup();
    system("pause");
    return 0;
}