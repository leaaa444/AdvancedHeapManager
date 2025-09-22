#include "Heap.h"
#include <iostream>

Heap::Heap() {
    InitializeCriticalSection(&m_lock);

    // Postavljamo pokazivače na null da znamo da heap još nije inicijalizovan
    m_start_ptr = nullptr;
    m_total_size = 0;
    m_used_size = 0;
    m_free_list_head = nullptr;
}

Heap::~Heap() {
    if (m_start_ptr != nullptr) {
        delete[] static_cast<char*>(m_start_ptr);
    }
    DeleteCriticalSection(&m_lock);
}

void Heap::init(size_t size) {
    // Alociraj jedan veliki blok memorije
    m_start_ptr = new char[size];
    m_total_size = size;
    m_used_size = 0;

    // Postavi prvi header (MemoryBlock) na početak alocirane memorije
    // Ovaj prvi blok predstavlja ceo heap kao jedan slobodan komad
    m_free_list_head = static_cast<MemoryBlock*>(m_start_ptr);
    m_free_list_head->size = size;
    m_free_list_head->is_free = true;
    m_free_list_head->next_free = nullptr; // Nema drugih slobodnih blokova
    m_free_list_head->prev_free = nullptr;
}


void* Heap::allocate(size_t size, size_t heap_index) {
    // Zaključaj heap
    EnterCriticalSection(&m_lock);

    // Veličina koju stvarno treba da alociramo
    size_t total_required_size = size + sizeof(MemoryBlock);

    // Krećemo od početka liste slobodnih blokova i tražimo prvi koji je dovoljno velik (First-Fit algoritam)
    MemoryBlock* current_block = m_free_list_head;
    while (current_block != nullptr) {
        if (current_block->size >= total_required_size) {
            break;
        }
        current_block = current_block->next_free;
    }

    // Ako nismo pronašli odgovarajući slobodan blok, otključavamo i vraćamo nullptr
    if (current_block == nullptr) {
        LeaveCriticalSection(&m_lock);
        return nullptr;
    }

    // Ako smo pronašli blok, proveravamo da li je dovoljno velik da ga podelimo
    // Ostavljamo prostora za još jedan header i minimalnu veličinu bloka
    size_t remaining_size = current_block->size - total_required_size;
    if (remaining_size > sizeof(MemoryBlock) + MIN_BLOCK_ALLOCATION_SIZE) {

        // Pravimo novi, manji slobodan blok od ostatka memorije
        MemoryBlock* new_free_block = (MemoryBlock*)((char*)current_block + total_required_size);
        new_free_block->size = remaining_size;
        new_free_block->is_free = true;

        // Ubacujemo novi slobodan blok u listu slobodnih blokova
        new_free_block->next_free = current_block->next_free;
        new_free_block->prev_free = current_block->prev_free;

        if (current_block->prev_free != nullptr) {
            current_block->prev_free->next_free = new_free_block;
        }
        else {
            // Ako je `current_block` bio prvi u listi, sada je to `new_free_block`
            m_free_list_head = new_free_block;
        }

        if (current_block->next_free != nullptr) {
            current_block->next_free->prev_free = new_free_block;
        }

        // Ažuriramo veličinu bloka koji ćemo vratiti korisniku
        current_block->size = total_required_size;

    }
    else {
        // Ako ostatak nije dovoljno velik za novi blok, dajemo korisniku ceo `current_block` i uklanjamo ga iz liste slobodnih
        if (current_block->prev_free != nullptr) {
            current_block->prev_free->next_free = current_block->next_free;
        }
        else {
            m_free_list_head = current_block->next_free;
        }
        if (current_block->next_free != nullptr) {
            current_block->next_free->prev_free = current_block->prev_free;
        }
    }

    // Označavamo blok kao zauzet i čistimo njegove pokazivače za listu slobodnih
    current_block->is_free = false;
    current_block->next_free = nullptr;
    current_block->prev_free = nullptr;
    m_used_size += current_block->size;
    current_block->heap_index = heap_index;

    LeaveCriticalSection(&m_lock);

    // Vraćamo pokazivač na memoriju nakon headera jer korisnik ne treba da vidi naš header
    return (void*)((char*)current_block + sizeof(MemoryBlock));
}


void Heap::free(void* ptr) {
    if (ptr == nullptr) {
        return;
    }

    EnterCriticalSection(&m_lock);

    // MemoryBlock header je ispred memorije koju je korisnik dobio
    MemoryBlock* block_to_free = (MemoryBlock*)((char*)ptr - sizeof(MemoryBlock));
    m_used_size -= block_to_free->size;

    block_to_free->is_free = true;

    // Pokušavamo da spojimo (coalesce) oslobođeni blok sa susednim slobondim blokovima
    // Provera sa blokom fizicki nakon našeg bloka
    MemoryBlock* next_block = (MemoryBlock*)((char*)block_to_free + block_to_free->size);
    // Proveravamo da li `next_block` postoji unutar granica heapa i da li je slobodan
    if ((void*)next_block < (char*)m_start_ptr + m_total_size && next_block->is_free) {
        // Uklanjamo `next_block` iz liste slobodnih jer će postati deo `block_to_free`
        if (next_block->prev_free != nullptr) {
            next_block->prev_free->next_free = next_block->next_free;
        }
        else {
            m_free_list_head = next_block->next_free;
        }
        if (next_block->next_free != nullptr) {
            next_block->next_free->prev_free = next_block->prev_free;
        }

        // Povećavamo veličinu našeg bloka za veličinu sledećeg
        block_to_free->size += next_block->size;
    }

    // Provera sa blokom FIZIČKI PRE našeg bloka
    // Ovo radimo tako što pretražujemo od početka heapa
    MemoryBlock* prev_block = nullptr;
    MemoryBlock* current = (MemoryBlock*)m_start_ptr;
    while ((void*)current < (void*)block_to_free) {
        prev_block = current;
        current = (MemoryBlock*)((char*)current + current->size);
    }

    // Ako `prev_block` postoji i slobodan je, spajamo `block_to_free` sa njim.
    if (prev_block != nullptr && prev_block->is_free) {
        // Ne moramo da diramo listu slobodnih, jer `prev_block` je već u njoj
        // Samo povećavamo njegovu veličinu
        prev_block->size += block_to_free->size;
        // `block_to_free` je sada spojen i ne treba ga dodavati u listu slobodnih
    }
    else {
        // Ako se nismo spojili sa prethodnim blokom, onda `block_to_free`
        // moramo da dodamo u listu slobodnih (na početak)
        block_to_free->next_free = m_free_list_head;
        block_to_free->prev_free = nullptr;
        if (m_free_list_head != nullptr) {
            m_free_list_head->prev_free = block_to_free;
        }
        m_free_list_head = block_to_free;
    }

    LeaveCriticalSection(&m_lock);
}


bool Heap::is_address_in_heap(void* ptr) const {
    if (m_start_ptr == nullptr) {
        return false;
    }

    // Adresa je validna ako je veća ili jednaka od početne, a manja od krajnje adrese heapa.
    return (ptr >= m_start_ptr && ptr < (void*)((char*)m_start_ptr + m_total_size));
}

size_t Heap::get_total_size() const {
    return m_total_size;
}

size_t Heap::get_used_size() const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_lock));
    size_t size = m_used_size;
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_lock));
    return size;
}

void Heap::print_layout() const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&m_lock));

    printf("--- Prikaz stanja Heap-a (Ukupno: %zu, Iskorisceno: %zu) ---\n", m_total_size, m_used_size);

    if (m_start_ptr == nullptr) {
        printf("Heap nije inicijalizovan.\n");
        LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_lock));
        return;
    }

    const char* current_pos = static_cast<const char*>(m_start_ptr);
    const char* end_pos = current_pos + m_total_size;

    // Prolazimo kroz memoriju blok po blok, fizičkim redosledom
    while (current_pos < end_pos) {
        const MemoryBlock* block = reinterpret_cast<const MemoryBlock*>(current_pos);

        // Crtamo status bloka
        printf(" | %-8s ", block->is_free ? "SLOBODAN" : "ZAUZET");

        // Crtamo vizuelni bar
        // Svaki '|' ili '.' predstavlja otprilike 1% heapa
        int bar_chars = (int)((double)block->size / m_total_size * 100.0);
        for (int i = 0; i < bar_chars; ++i) {
            printf(block->is_free ? "." : "|");
        }
        printf("\n");

        // Ispisujemo detalje
        printf(" | Adresa: %p, Velicina: %-6zu bajtova\n", (void*)block, block->size);
        printf(" +------------------------------------------------------------\n");

        current_pos += block->size;
    }

    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&m_lock));
}