#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include <iomanip>
#include <process.h>
#include <windows.h>
#include "AdvancedHeapManager.h"
#include "ThreadPool.h" 

#pragma comment(lib, "Ws2_32.lib")

// ======================= GLAVNI PREKIDAČ ZA TESTOVE =======================
#define TEST_STRESS_KLIJENTI      1 // Test sa više spoljnih klijenata
#define TEST_DEMO_MULTI_THREAD    2 // Mali demo sa više niti i više heapova

// Promeni vrednost ispod da izabereš koji test želiš da pokreneš
#define AKTIVAN_TEST   TEST_STRESS_KLIJENTI
// ========================================================================

// Globalna promenljiva za kontrolu rada niti u nekim testovima
bool g_keep_running = true;


#if AKTIVAN_TEST == TEST_STRESS_KLIJENTI
// =========================================================================================
// === TEST 1: KOD ZA TEST SA VIŠE KLIJENATA (STRES TEST) ===================================
// =========================================================================================

#define DEFAULT_PORT "27015"
#define NUM_WORKER_THREADS 16 // Broj unapred kreiranih niti u pool-u

const int HEAP_COUNT = 4;
const size_t HEAP_SIZE_MB = 4;
AdvancedHeapManager g_ahm(HEAP_COUNT, HEAP_SIZE_MB * 1024 * 1024);

ThreadPool g_thread_pool(NUM_WORKER_THREADS);

unsigned int __stdcall ClientHandlerThread(void* p_client_socket) {
    SOCKET client_socket = (SOCKET)(uintptr_t)p_client_socket;
    char recv_buffer[512];
    int recv_len;
    while ((recv_len = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0)) > 0) {
        recv_buffer[recv_len] = '\0';
        int size = 0;
        void* address_to_free = nullptr;
        if (sscanf_s(recv_buffer, "ALLOCATE %d", &size) == 1) {
            void* allocated_mem = g_ahm.allocate(size, (int)client_socket);
            char response_buffer[50];
            sprintf_s(response_buffer, sizeof(response_buffer), "%p", allocated_mem);
            send(client_socket, response_buffer, (int)strlen(response_buffer), 0);
        }
        else if (sscanf_s(recv_buffer, "FREE %p", &address_to_free) == 1) {
            g_ahm.free(address_to_free);
            const char* response = "OK";
            send(client_socket, response, (int)strlen(response), 0);
        }
    }
    closesocket(client_socket);
    return 0;
}

void DrawStatusBar(size_t used, size_t total) {
    const int bar_width = 50;
    if (total == 0) return;
    double percentage = (double)used / total;
    int chars_to_draw = (int)(percentage * bar_width);
    printf("[");
    for (int i = 0; i < bar_width; ++i) printf(i < chars_to_draw ? "|" : " ");
    printf("] %6.2f%% Used (%8zu / %8zu bytes)\n", percentage * 100.0, used, total);
}

DWORD WINAPI MonitorThread(LPVOID lpParam) {
    while (g_keep_running) {
        time_t raw_time = time(NULL);
        struct tm time_info;
        char time_buffer[9];
        localtime_s(&time_info, &raw_time);
        strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &time_info);
        printf("\n--- AHM Server Status (STRES TEST) [%s] ---\n", time_buffer);
        for (int i = 0; i < HEAP_COUNT; ++i) {
            size_t used = 0, total = 0;
            g_ahm.get_heap_stats(i, used, total);
            printf("Heap %d: ", i);
            DrawStatusBar(used, total);
        }
        printf("-----------------------------------------------\n");
        printf("Server aktivan, ceka klijente na portu %s...\n", DEFAULT_PORT);
        Sleep(1000);
    }
    return 0;
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    addrinfo* result = NULL;
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    SOCKET listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    freeaddrinfo(result);
    listen(listen_socket, SOMAXCONN);

    HANDLE h_monitor_thread = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)MonitorThread, NULL, 0, NULL);

    printf("Server sa Thread Pool-om (%d niti) slusa na portu %s...\n", NUM_WORKER_THREADS, DEFAULT_PORT);

    while (true) {
        SOCKET client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) { continue; }
        g_thread_pool.submit_task((void*)(uintptr_t)client_socket);
    }
    closesocket(listen_socket);
    WSACleanup();
    return 0;
}


#elif AKTIVAN_TEST == TEST_DEMO_MULTI_THREAD
// =========================================================================================
// === TEST 2: KOD ZA DETALJNU MULTI-THREAD DEMONSTRACIJU ==================================
// =========================================================================================
const int HEAP_COUNT_DEMO_MT = 5;
const size_t HEAP_SIZE_BYTES_DEMO_MT = 1024;
const int NUM_DEMO_THREADS = 3;
const int ALLOCATIONS_PER_DEMO_THREAD = 4;
AdvancedHeapManager g_ahm_demo_mt(HEAP_COUNT_DEMO_MT, HEAP_SIZE_BYTES_DEMO_MT);

DWORD WINAPI DemoMonitorThread(LPVOID lpParam) {
    while (g_keep_running) {
        printf("--- Detaljni prikaz stanja za %d heap-ova ---\n", HEAP_COUNT_DEMO_MT);
        for (int i = 0; i < HEAP_COUNT_DEMO_MT; ++i) {
            Heap* h = g_ahm_demo_mt.get_heap(i);
            if (h != nullptr) h->print_layout();
        }
        Sleep(2000);
    }
    return 0;
}

DWORD WINAPI DemoWorkerThread(LPVOID lpParam) {
    size_t thread_id = (size_t)(uintptr_t)lpParam;
    for (int i = 0; i < ALLOCATIONS_PER_DEMO_THREAD; ++i) {
        Sleep(500 + (rand() % 1000));
        size_t size_to_alloc = 50 + (rand() % 100);
        printf("\n>>> Nit %zu trazi %zu bajtova...\n", thread_id, size_to_alloc);
        g_ahm_demo_mt.allocate(size_to_alloc, (int)thread_id);
    }
    printf("\n<<< Nit %zu je zavrsila sa alokacijama.\n", thread_id);
    return 0;
}

int main() {
    srand(static_cast<unsigned int>(time(NULL)));
    HANDLE demo_workers[NUM_DEMO_THREADS];
    HANDLE h_monitor_thread = CreateThread(NULL, 0, DemoMonitorThread, NULL, 0, NULL);
    for (size_t i = 0; i < NUM_DEMO_THREADS; ++i) {
        demo_workers[i] = CreateThread(NULL, 0, DemoWorkerThread, (LPVOID)i, 0, NULL);
    }
    WaitForMultipleObjects(NUM_DEMO_THREADS, demo_workers, TRUE, INFINITE);
    printf("\nSve radnicke niti su zavrsile. Pritisni taster za finalni prikaz stanja.\n");
    g_keep_running = false;
    WaitForSingleObject(h_monitor_thread, INFINITE);
    printf("--- FINALNO STANJE HEAP-OVA ---\n");
    for (int i = 0; i < HEAP_COUNT_DEMO_MT; ++i) {
        Heap* h = g_ahm_demo_mt.get_heap(i);
        if (h != nullptr) h->print_layout();
    }
    for (size_t i = 0; i < NUM_DEMO_THREADS; ++i) CloseHandle(demo_workers[i]);
    CloseHandle(h_monitor_thread);
    system("pause");
    return 0;
}


#endif