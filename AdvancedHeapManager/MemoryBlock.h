#pragma once
#pragma once
#ifndef MEMORY_BLOCK_H
#define MEMORY_BLOCK_H

#include <cstddef> // za size_t

// header koji se nalazi na početku svakog memorijskog bloka(i slobodnog i zauzetog).
// Služi nam da bismo znali veličinu bloka i da bismo mogli da formiramo
// listu slobodnih blokova.
struct MemoryBlock {
    size_t size;      // Veličina celog bloka (uključujući i ovaj header)
    bool is_free;     // Status bloka

    MemoryBlock* next_free; // Pokazivač na sledeći SLOBODAN blok u listi
    MemoryBlock* prev_free; // Pokazivač na prethodni SLOBODAN blok u listi

    size_t heap_index;
};

#endif // MEMORY_BLOCK_H
