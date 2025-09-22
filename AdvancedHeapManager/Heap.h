#pragma once
#ifndef HEAP_H
#define HEAP_H

#include <windows.h> 
#include "MemoryBlock.h"

class Heap {
private:
    void* m_start_ptr;        // Pokazivač na početak memorije kojom heap upravlja
    size_t m_total_size;      // Ukupna veličina te memorije
    size_t m_used_size;

    MemoryBlock* m_free_list_head; // Glava liste slobodnih blokova
    CRITICAL_SECTION m_lock;  // Kriticna sekcija za sinhronizaciju pristupa ovom heap-u

    static const size_t MIN_BLOCK_ALLOCATION_SIZE = 16;  // Minimalna veličina korisničkog dela memorije za novi blok
public:
    // Konstruktor postavlja početne vrednosti.
    Heap();
    ~Heap();

    // Inicijalizacija heapa - alocira memoriju i postavlja prvi slobodan blok
    void init(size_t size);

    // Funkcija za alokaciju memorije unutar ovog heapa
    void* allocate(size_t size, size_t heap_index);

    // Funkcija za dealokaciju memorije
    void free(void* ptr);

    // Pomoćna funkcija da proverimo da li adresa pripada ovom heapu
    bool is_address_in_heap(void* ptr) const;

    size_t get_used_size() const;
    size_t get_total_size() const;

    void print_layout() const;
};


#endif // HEAP_H