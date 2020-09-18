#include "semaphore.h"

Semaphore::Semaphore(int thread_num)
	: i(thread_num)
{
}

void Semaphore::wait()
{
	std::unique_lock<std::mutex> lock(cv_m);
	while(!i)
		cv.wait(lock);
	--i;
}

void Semaphore::signal()
{
	std::lock_guard<std::mutex> lock(cv_m);
	++i;
	cv.notify_one();
}