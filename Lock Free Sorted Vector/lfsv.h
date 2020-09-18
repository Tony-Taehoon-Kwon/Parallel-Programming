#include <iostream>       // std::cout
#include <atomic>         // std::atomic
#include <thread>         // std::thread
#include <vector>         // std::vector
#include <deque>          // std::deque
#include <mutex>          // std::mutex

class MemoryBank {
    std::deque<std::vector<int>*> slots;
    std::mutex m;

public:
    MemoryBank() : slots(6000) {
        for (int i = 0; i < 6000; ++i)
            slots[i] = reinterpret_cast<std::vector<int>*>(new char[sizeof(std::vector<int>)]);
    }
    std::vector<int>* Get() {
        std::lock_guard<std::mutex> lock(m);
        std::vector<int>* p = slots[0];
        slots.pop_front();
        return p;
    }
    void Store(std::vector<int>* p) {
        std::lock_guard<std::mutex> lock(m);
        slots.push_back(p);
    }
    ~MemoryBank() {
        for (auto& el : slots) 
            delete[] reinterpret_cast<char*>(el);
    }
};

struct Pair {
    std::vector<int>* pointer;
    long              ref_count;
}; // __attribute__((aligned(16),packed));
// for some compilers alignment needed to stop std::atomic<Pair>::load to segfault

class LFSV {
    MemoryBank mb;
    std::atomic<Pair> pdata;

public:
    LFSV() : mb(), pdata(Pair{new (mb.Get()) std::vector<int>(), 1}) {
//        std::cout << "Is lockfree " << pdata.is_lock_free() << std::endl;
    }   

    ~LFSV() { 
        pdata.load().pointer->~vector();
        mb.Store(pdata.load().pointer);
    }

    // Update
    void Insert( int const & v ) {
        Pair pdata_new, pdata_old;
        pdata_new.pointer = nullptr;
        //std::vector<int>* last = nullptr; // variable to keep the pdata_old.pointer for optimization

        do {
            pdata_old = pdata.load();
            //if (last == pdata_old.pointer) // optimization purpose
            //    continue;

            /* release the vector and insert the vector pointer to the end of memory bank slot.
               this pointer would be deleted when the memory bank destructor are called */
            if (pdata_new.pointer != nullptr)
            {
                pdata_new.pointer->~vector();
                mb.Store(pdata_new.pointer);
            }

            /* set the reference counter of pdata_old as 1
               to compare with pdata in while loop condition check
               to prevent writer modifying data while reader is still reading.
               counter 1 means data is not referencing in any other thread */
            pdata_old.ref_count = 1;

            Pair pdata_new_memory, pdata_old_memory;
            do {
                pdata_old_memory = pdata.load();
                pdata_new_memory = pdata_old_memory;
                ++pdata_new_memory.ref_count;
            } while (!(this->pdata).compare_exchange_weak(pdata_old_memory, pdata_new_memory));
            /* set pdata_new.pointer with pre-allocated slot in memory bank
               and copy the value of the vector *pdata_old.pointer */
            pdata_new.pointer = new (mb.Get()) std::vector<int>(*pdata_old_memory.pointer);
            do {
                pdata_old_memory = pdata.load();
                pdata_new_memory = pdata_old_memory;
                --pdata_new_memory.ref_count;
            } while (!(this->pdata).compare_exchange_weak(pdata_old_memory, pdata_new_memory));

            pdata_new.ref_count = 1; // set the reference counter of pdata_new as 1

            // working on a local copy
            std::vector<int>::iterator b = pdata_new.pointer->begin();
            std::vector<int>::iterator e = pdata_new.pointer->end();
            if ( b==e || v>=pdata_new.pointer->back() ) { pdata_new.pointer->push_back( v ); } //first in empty or last element
            else {
                for ( ; b!=e; ++b ) {
                    if ( *b >= v ) {
                        pdata_new.pointer->insert( b, v );
                        break;
                    }
                }
            }
            /* optimization purpose */
            //last = pdata_old.pointer;
        } while (!(this->pdata).compare_exchange_weak(pdata_old, pdata_new)); // CAS

        /* release the vector and store the vector pointer */
        pdata_old.pointer->~vector();
        mb.Store(pdata_old.pointer);
    }

    // Lookup
    int operator[] ( int pos ) { // not a const method anymore
        Pair pdata_new, pdata_old;
        do {
            pdata_old = pdata.load();
            pdata_new = pdata_old;
            ++pdata_new.ref_count; // increase counter to indicate pdata will be used to prevent writer modifying data
        } while (!(this->pdata).compare_exchange_weak(pdata_old, pdata_new)); // CAS

        int ret_val = (*pdata_new.pointer) [pos];

        do {
            pdata_old = pdata.load();
            pdata_new = pdata_old;
            --pdata_new.ref_count; // decrease counter to indicate finished using pdata
        } while (!(this->pdata).compare_exchange_weak(pdata_old, pdata_new)); // CAS

        return ret_val;
    }
};
