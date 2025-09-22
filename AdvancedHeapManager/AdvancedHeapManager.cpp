#include "AdvancedHeapManager.h"
#include <iostream>

AdvancedHeapManager::AdvancedHeapManager(size_t heap_count, size_t heap_size) {
    m_heap_count = heap_count;
    m_current_heap_idx = 0; 

    m_heaps = new Heap[m_heap_count];

    // inicijalizujemo svaki heap u nizu
    for (size_t i = 0; i < m_heap_count; ++i) {
        m_heaps[i].init(heap_size);
    }

    InitializeCriticalSection(&m_lock);
}

AdvancedHeapManager::~AdvancedHeapManager() {
    delete[] m_heaps;
    DeleteCriticalSection(&m_lock);
}

// Allocate funkcija sa Round-Robin logikom
void* AdvancedHeapManager::allocate(size_t size, int thread_id) {
    int heap_to_use;

    EnterCriticalSection(&m_lock);
    heap_to_use = m_current_heap_idx;
    m_current_heap_idx = (m_current_heap_idx + 1) % m_heap_count;

    //printf("Nit %d -> AHM je dodelio HEAP[%d]\n", thread_id, heap_to_use);


    LeaveCriticalSection(&m_lock);

    // Pozivamo allocate na izabranom heapu, on će zaključati sebe
    return m_heaps[heap_to_use].allocate(size, heap_to_use);
}

// Free funkcija koja pronalazi pravi heap
void AdvancedHeapManager::free(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    size_t heap_index = get_heap_index(ptr);

    if (heap_index < m_heap_count) {
        m_heaps[heap_index].free(ptr);
    }

    // Ako smo došli dovde, adresa nije ni u jednom našem heapu.
    // U realnom sistemu, ovde bi trebalo prijaviti grešku.
}

void AdvancedHeapManager::get_heap_stats(size_t heap_index, size_t& used_size, size_t& total_size) const {
    if (heap_index < m_heap_count) {
        used_size = m_heaps[heap_index].get_used_size();
        total_size = m_heaps[heap_index].get_total_size();
    }
}

size_t AdvancedHeapManager::get_heap_index(void* ptr) const {
    if (ptr == nullptr) {
        // Vraćamo nevalidan indeks
        return (size_t)-1;
    }

    // Računamo adresu MemoryBlock headera
    const MemoryBlock* block_header = (const MemoryBlock*)((const char*)ptr - sizeof(MemoryBlock));

    // Vraćamo sačuvani indeks
    return block_header->heap_index;
}

Heap* AdvancedHeapManager::get_heap(size_t index) {
    // Proveravamo da li je traženi indeks u ispravnom opsegu
    if (index < m_heap_count) {
        return &m_heaps[index];
    }
    // Ako nije, vraćamo null pokazivač
    return nullptr;
}