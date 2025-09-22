// U ThreadPool.cpp

#include "ThreadPool.h"
#include <process.h>

// Deklaracija naše funkcije za obradu klijenta koja se nalazi u Server.cpp
unsigned int __stdcall ClientHandlerThread(void* p_client_socket);

ThreadPool::ThreadPool(int num_threads) {
    m_num_threads = num_threads;
    m_threads = new HANDLE[m_num_threads];
    m_stop_threads = false;

    // Inicijalizacija kružnog bafera
    m_queue_capacity = 256; // Npr. može da primi 256 zadataka pre nego što blokira
    m_task_queue = new void* [m_queue_capacity];
    m_head = 0;
    m_tail = 0;

    // Inicijalizacija sinhronizacionih mehanizama
    InitializeCriticalSection(&m_queue_lock);
    // Na početku, ima 0 zadataka, a kapacitet slobodnog mesta
    m_tasks_available = CreateSemaphore(NULL, 0, m_queue_capacity, NULL);
    m_space_available = CreateSemaphore(NULL, m_queue_capacity, m_queue_capacity, NULL);

    // Kreiranje i pokretanje radničkih niti
    for (int i = 0; i < m_num_threads; ++i) {
m_threads[i] = (HANDLE)_beginthreadex(NULL, 0, (_beginthreadex_proc_type)&ThreadPool::worker_thread_start, this, 0, NULL);
    }
}

ThreadPool::~ThreadPool() {
    m_stop_threads = true;

    // "Probudi" sve niti da bi mogle da pročitaju `m_stop_threads` i da se ugase
    for (int i = 0; i < m_num_threads; ++i) {
        ReleaseSemaphore(m_tasks_available, 1, NULL);
    }

    WaitForMultipleObjects(m_num_threads, m_threads, TRUE, INFINITE);

    for (int i = 0; i < m_num_threads; ++i) {
        CloseHandle(m_threads[i]);
    }

    delete[] m_threads;
    delete[] m_task_queue;
    CloseHandle(m_tasks_available);
    CloseHandle(m_space_available);
    DeleteCriticalSection(&m_queue_lock);
}

void ThreadPool::submit_task(void* task) {
    // Čekamo da se oslobodi mesto u redu
    WaitForSingleObject(m_space_available, INFINITE);

    // Zaključavamo red i dodajemo zadatak
    EnterCriticalSection(&m_queue_lock);
    m_task_queue[m_tail] = task;
    m_tail = (m_tail + 1) % m_queue_capacity;
    LeaveCriticalSection(&m_queue_lock);

    // Signaliziramo jednoj od radničkih niti da je novi zadatak dostupan
    ReleaseSemaphore(m_tasks_available, 1, NULL);
}

DWORD WINAPI ThreadPool::worker_thread_start(LPVOID param) {
    ThreadPool* pool = (ThreadPool*)param;

    while (true) {
        // Nit spava dok ne stigne novi zadatak
        WaitForSingleObject(pool->m_tasks_available, INFINITE);

        if (pool->m_stop_threads) {
            break; // Izlazimo iz petlje ako je stigao signal za gašenje
        }

        // Preuzimamo zadatak iz reda
        EnterCriticalSection(&pool->m_queue_lock);
        void* task = pool->m_task_queue[pool->m_head];
        pool->m_head = (pool->m_head + 1) % pool->m_queue_capacity;
        LeaveCriticalSection(&pool->m_queue_lock);

        // Signaliziramo da sada ima jedno slobodno mesto više u redu
        ReleaseSemaphore(pool->m_space_available, 1, NULL);

        // --- IZVRŠAVANJE ZADATKA ---
        // Pozivamo našu staru funkciju za obradu klijenta sa preuzetim zadatkom
        ClientHandlerThread(task);
    }
    return 0;
}