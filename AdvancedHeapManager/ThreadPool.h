// U ThreadPool.h

#pragma once
#include <windows.h>

class ThreadPool {
public:
    // Konstruktor: kreira određen broj radničkih niti
    ThreadPool(int num_threads);
    // Destruktor: zaustavlja i čisti niti
    ~ThreadPool();

    // Funkcija za dodavanje novog zadatka u red
    // Zadatak je u našem slučaju pokazivač na klijentski soket
    void submit_task(void* task);

private:
    // Statička funkcija koju svaka nit u pool-u izvršava
    static DWORD WINAPI worker_thread_start(LPVOID param);

    // --- Resursi Thread Pool-a ---
    int m_num_threads;
    HANDLE* m_threads; // Niz handle-ova za radničke niti

    // --- Kružni bafer za zadatke ---
    void** m_task_queue;
    int m_queue_capacity;
    int m_head;
    int m_tail;

    // --- Sinhronizacioni mehanizmi ---
    CRITICAL_SECTION m_queue_lock; // Štiti pristup kružnom baferu
    HANDLE m_tasks_available;      // Semafor koji signalizira da ima novih zadataka
    HANDLE m_space_available;      // Semafor koji signalizira da ima mesta u baferu

    bool m_stop_threads; // Flag za zaustavljanje niti
};