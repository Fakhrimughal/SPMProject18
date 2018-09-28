#include <iostream>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <chrono>
#include <cstddef>
#include <math.h>
#include <string>
#include "CImg.h"
#include <cctype>

using namespace cimg_library;
template <typename T>
class queue
{
  private:
	std::mutex d_mutex;

	std::mutex mtx;
	std::condition_variable d_condition;
	std::deque<T> d_queue;

  public:
	queue(std::string s)
	{
		std::cout << "Created " << s << " queue " << std::endl;
	}
	queue() {}

	void push(T const &value)
	{
		{
			std::unique_lock<std::mutex> lock(this->d_mutex);
			d_queue.push_front(value);
		}
		this->d_condition.notify_one();
	}

	T pop()
	{
		std::unique_lock<std::mutex> lock(this->d_mutex);
		this->d_condition.wait(lock, [=] { return !this->d_queue.empty(); });
		T rc(std::move(this->d_queue.back()));
		this->d_queue.pop_back();
		return rc;
	}
};

#define EOS NULL

class Worker
{
  public:
	Worker();
	int start;
	int end;
	queue<std::pair<std::string, CImg<unsigned char> *>> tuple;

	~Worker();

  private:
};
Worker::Worker()
{
}

Worker::~Worker()
{
}
class FF_Worker
{
  public:
	FF_Worker();
	int start;
	int end;

	~FF_Worker();

  private:
};
FF_Worker::FF_Worker()
{
}

FF_Worker::~FF_Worker()
{
}
std::mutex mtx;
void shared_print(int id, double idealService, double workerTimeToPush, int totalImages, int flag)
{
	std::unique_lock<std::mutex> lck(mtx);
	if (flag)
		std::cerr
			<< "\t\tWorker : " << id << " Ideal service time : " << idealService << " msecs\n"
			<< "\t\tWorker takes : " << workerTimeToPush << " msecs to send a processed image\n"
			<< "\t\tRecieves total : " << totalImages << " images\n"
			<< std::endl;
	else
		std::cerr
			<< "\t\tWorker : " << id << " Ideal service time : " << idealService << " msecs\n"
			<< "\t\tWorker takes : " << workerTimeToPush << " msecs to send a processed partition\n"
			<< "\t\tRecieves total : " << totalImages << " portion of image\n"
			<< std::endl;
}
void ff_shared_print(int id, double idealService, int totalImages, int flag)
{
	std::unique_lock<std::mutex> lck(mtx);
	if (flag)
		std::cerr
			<< "\t\tWorker : " << id << " Ideal service time : " << idealService << " msecs\n"
			<< "\t\tRecieves total : " << totalImages << " images\n"
			<< std::endl;
	else
		std::cerr
			<< "\t\tWorker : " << id << " Ideal service time : " << idealService << " msecs\n"
			<< "\t\tRecieves total : " << totalImages << " portion of image\n"
			<< std::endl;
}
int findJPG(std::string check)
{
	std::string ext = ".jpg";
	if (check.length() >= ext.length())
	{
		return (0 == check.compare(check.length() - ext.length(), ext.length(), ext));
	}
	else
		return 0;
}
void active_delay(int usecs)
{
	auto start = std::chrono::high_resolution_clock::now();
	auto end = false;
	while (!end)
	{
		auto elapsed = std::chrono::high_resolution_clock::now() - start;
		auto usec = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
		if (usec > usecs)
			end = true;
	}
	return;
}