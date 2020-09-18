#include <condition_variable>

class Semaphore
{
public:
	Semaphore(int thread_num);
	void wait();
	void signal();
private:
	std::condition_variable cv;
	std::mutex cv_m;
	int i = 0;
};
