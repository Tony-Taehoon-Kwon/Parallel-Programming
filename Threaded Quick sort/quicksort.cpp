#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include "sort_small_arrays.h"

template< typename T>
unsigned partition( T* a, unsigned begin, unsigned end) {
	unsigned i = begin, last = end-1;
	T pivot = a[last];

	for (unsigned j=begin; j<last; ++j) {
		if ( a[j]<pivot ) {
			std::swap( a[j], a[i] );
			++i;
		}
	}
	std::swap( a[i], a[last] );
	return i;
}

template< typename T>
unsigned partition_new( T* a, unsigned begin, unsigned end) {
    if ( end-begin > 8 ) return partition_old( a, begin, end );

	unsigned i = begin, last = end-1, step = (end-begin)/4;

    T* pivots[5] = { a+begin, a+begin+step, a+begin+2*step, a+begin+3*step, a+last };
    quicksort_base_5_pointers( pivots );

	std::swap( a[last], a[begin+2*step] );
	T pivot = a[last];
    
    for (unsigned j=begin; j<last; ++j) {
		if ( a[j]<pivot /*|| a[j]==pivot*/ ) {
			std::swap( a[j], a[i] );
			++i;
		}
	}
	std::swap( a[i], a[last] );
	return i;
}

/* recursive */
template< typename T>
void quicksort_rec( T* a, unsigned begin, unsigned end )
{
    if ( end-begin<6 ) {
        switch ( end-begin ) {
            case 5: quicksort_base_5( a+begin ); break;
            case 4: quicksort_base_4( a+begin ); break;
            case 3: quicksort_base_3( a+begin ); break;
            case 2: quicksort_base_2( a+begin ); break;
        }
        return;
    }

	unsigned q = partition(a,begin,end);
 	
	quicksort_rec(a,begin,q);
	quicksort_rec(a,q,end);
}

/* iterative */
#define STACK
#define xVECTOR
#define xPRIORITY_QUEUE 

#include <utility> // std::pair

template <typename T>
using triple = typename std::pair< T*, std::pair<unsigned,unsigned>>;

template< typename T>
struct compare_triples {
    bool operator() ( triple<T> const& op1, triple<T> const& op2 ) const {
        return op1.second.first > op2.second.first;
    }
};

#ifdef STACK
#include <stack>
template< typename T>
using Container = std::stack< triple<T>>;
#define PUSH push
#define TOP  top
#define POP  pop
#endif

#ifdef VECTOR
#include <vector>
template< typename T>
using Container = std::vector< triple<T>>;
#define PUSH push_back
#define TOP  back
#define POP  pop_back
#endif

#ifdef PRIORITY_QUEUE
#include <queue>
template< typename T>
using Container = std::priority_queue< triple<T>, std::vector<triple<T>>, compare_triples<T> >;
#define PUSH push
#define TOP  top
#define POP  pop
#endif

template< typename T>
void quicksort_iterative_aux( Container<T> & ranges );

template< typename T>
void quicksort_iterative( T* a, unsigned begin, unsigned end )
{
    Container<T> ranges;
    ranges.PUSH( std::make_pair( a, std::make_pair( begin,end ) ) );
    quicksort_iterative_aux( ranges );
}

template< typename T>
void quicksort_iterative_aux( Container<T> & ranges )
{
    while ( ! ranges.empty() ) {
        triple<T> r = ranges.TOP();
        ranges.POP();
        
        T*       a = r.first;
        unsigned b = r.second.first;
        unsigned e = r.second.second;
        
        //base case
        if (e-b<6) {
            switch ( e-b ) {
                case 5: quicksort_base_5( a+b ); break;
                case 4: quicksort_base_4( a+b ); break;
                case 3: quicksort_base_3( a+b ); break;
                case 2: quicksort_base_2( a+b ); break;
            }
            continue;
        }

        unsigned q = partition(a,b,e);

        ranges.PUSH( std::make_pair( a, std::make_pair( b,q ) ) );
        ranges.PUSH( std::make_pair( a, std::make_pair( q+1,e ) ) );
    }
}

// class from assignment2
class Semaphore
{
public:
    Semaphore(int thread_num) : cv(), cv_m(), i(thread_num) {}
    void wait() {
        std::unique_lock<std::mutex> lock(cv_m);
        while (!i)
            cv.wait(lock);
        --i;
    }
    void signal() {
        std::lock_guard<std::mutex> lock(cv_m);
        ++i;
        cv.notify_one();
    }
private:
    std::condition_variable cv;
    std::mutex cv_m;
    int i = 0;
};

template< typename T>
void quicksort_aux(Container<T>& ranges, int original_size);

template< typename T>
void quicksort(T* a, unsigned begin, unsigned end, int thread_num)
{
    /* The best number of threads to sort array of 200 Ratios with the delay : 22 */

    Container<T> ranges;
    ranges.PUSH(std::make_pair(a, std::make_pair(begin, end)));

    std::vector<std::thread> threads;

    for (int i = 0; i < thread_num; ++i)
        threads.push_back(std::thread([&]() { quicksort_aux(ranges, end - begin); }));

    for (auto& th : threads)
        th.join();
}

/* global variable */
static std::atomic<int> counter(0);
static std::mutex mutex;
static Semaphore sem(1);

template< typename T>
void quicksort_aux(Container<T>& ranges, int original_size)
{
    triple<T> r;
    T* a;
    unsigned b, e;

    do {
        /* Do one partition for each thread.
           if there is no more partition left, wait */
        sem.wait();

        /* lock for reader */
        mutex.lock();
        if (ranges.empty()) {
            mutex.unlock();
            continue;
        }
        r = ranges.TOP();
        ranges.POP();
        mutex.unlock();

        a = r.first;            // array with given range
        b = r.second.first;     // begin index
        e = r.second.second;    // end index

        /* if the top of stack is dummy object, see the next stack
           if there is no more stack left, skip this loop
           and will continue inserting dummy object later */
        while (a == nullptr)
        {
            /* lock for reader */
            mutex.lock();
            if (ranges.empty()) {
                mutex.unlock();
                break;
            }
            r = ranges.TOP();
            ranges.POP();
            mutex.unlock();

            a = r.first;            // array with given range
            b = r.second.first;     // begin index
            e = r.second.second;    // end index
        }

        //base case
        if (e - b < 6) {
            switch (e - b) {
            case 5: quicksort_base_5(a + b); break;
            case 4: quicksort_base_4(a + b); break;
            case 3: quicksort_base_3(a + b); break;
            case 2: quicksort_base_2(a + b); break;
            }
            /* increment the counter by counting the number of objects in the correct position */
            counter += e - b; // range from [b,e) is already sorted, so all the elements are at the right position

            /* insert dummy object. lock for writer */
            mutex.lock();
            ranges.PUSH(std::make_pair(nullptr, std::make_pair(0, 0)));
            mutex.unlock();

            /* if this partition in this thread is sorted, signal once.
               Don't terminate thread until all elements in original array are sorted */
            sem.signal();
            continue;
        }

        unsigned q = partition(a, b, e);
        ++counter; // count pivot element, which is at the right position

        /* lock for writer */
        mutex.lock();
        ranges.PUSH(std::make_pair(a, std::make_pair(b, q)));
        ranges.PUSH(std::make_pair(a, std::make_pair(q + 1, e)));
        mutex.unlock();
        
        sem.signal(); // left partition thread
        sem.signal(); // right partition thread

    } while (counter < original_size); // when counter is equal to the size of the original array, DONE
}