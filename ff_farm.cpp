#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>
#include <thread>
#include <ff/farm.hpp>
#define cimg_use_jpeg 1
#include "CImg.h"
#include <atomic>
#include "util.h"
#include <utility>
#include <atomic>
#include <stdio.h>
#include <dirent.h>
#define PIXEL 50

using namespace cimg_library;
using namespace ff;

typedef std::pair<std::string, CImg<unsigned char> *> task;

double forkThread = 0, timeToReadImg = 0, timeToCreateCopy = 0, emitterTimeToPushAll = 0, collectorInterdeparture = 0, timeToTerminate = 0;
int height = 0, width = 0, count = 0;
auto tParallelStart = std::chrono::high_resolution_clock::now();
auto tCompletionStart = std::chrono::high_resolution_clock::now();
auto tStartCollector = std::chrono::high_resolution_clock::now();
int interarrivalTime = 10;
std::vector<task> toEmitter;
std::atomic<long> completionTime(0);

struct tt_emitter : ff_node_t<char, task>
{
	int imgSend;
	int nw;
	std::vector<task> toEmitter;
	tt_emitter(int nw, std::vector<task> toEmitter) : nw(nw), toEmitter(toEmitter) {}

	task *svc(char *)
	{
		int imgSend = 0;
		/**************************Start time for the parallel execution. Ending time on line 195***************************/
		tParallelStart = std::chrono::high_resolution_clock::now();

		for (auto &temp : toEmitter)
		{
			if (imgSend == 0)
				tCompletionStart = std::chrono::high_resolution_clock::now();

			ff_send_out(&temp);
			active_delay(interarrivalTime);
			imgSend++;
		}
		auto tEnd = std::chrono::high_resolution_clock::now();
		emitterTimeToPushAll = std::chrono::duration<double, std::milli>(tEnd - tParallelStart).count();
		/********************************************************************************************************************/
		return EOS;
	}
};

struct tt_worker : ff_node_t<task, task>
{
	CImg<unsigned char> *mark;
	unsigned char *markptr;
	unsigned char *imgptr;
	int imgReceived;
	double idealService;
	int id;

	tt_worker(int id, CImg<unsigned char> *mark) : id(id), mark(mark)
	{
		/********************** Load a copy of the mark in each worker only once in each worker *****************************/
		markptr = mark->data();
		imgReceived = 0;
		idealService = 0;
	}
	task *svc(task *tuple)
	{
		if (tuple)
		{
			/************************************ Time to procees a single image  *******************************************/
			auto tStart = std::chrono::high_resolution_clock::now();
			imgptr = tuple->second->data();
			for (int i = 0; i < (width * height); i++)
			{
				if ((int)markptr[i] < PIXEL)
				{
					imgptr[i] = ((int)markptr[i] + ((int)imgptr[i] * 0.3)) / 2;
					imgptr[i + width * height] = ((int)markptr[i] + ((int)imgptr[i + width * height] * 0.59)) / 2;
					imgptr[i + 2 * width * height] = ((int)markptr[i] + ((int)imgptr[i + 2 * width * height] * 0.11)) / 2;
				}
			}
			if (imgReceived == 0)
			{
				auto tEnd = std::chrono::high_resolution_clock::now();
				idealService = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			}
			imgReceived++;
			return tuple;
		}
		else
		{
			return EOS;
		}
	}

	void svc_end()
	{
		ff_shared_print(id, idealService, imgReceived, 1);
		return;
	}
};

struct tt_collector : ff_minode_t<task, char>
{
	tt_collector(std::string destFolder, int nw) : destFolder(destFolder), nw(nw) {}
	char *svc(task *toDestDataPair)
	{
		if (toDestDataPair)
		{
			auto tStart = std::chrono::high_resolution_clock::now();

			/************************** The last incoming image will set the time ******************************************/
			completionTime = std::chrono::duration<double, std::milli>(tStart - tCompletionStart).count();
			/***************************** Uncomment if you want to save the images to the disk  ***************************/
			//std::string path = destFolder + '/' + toDestDataPair->first.c_str();
			//toDestDataPair->second->save(path.c_str());
		}
		return GO_ON;
	}

	void svc_end()
	{
		/*************************************** Terminate Execution ***************************************************/
		auto tParallelEnd = std::chrono::high_resolution_clock::now();
		timeToTerminate = std::chrono::duration<double, std::milli>(tParallelEnd - tParallelStart).count();
		return;
	}
	std::string destPath;
	task toDestDataPair;
	std::string destFolder;
	int nw;
};

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		std::cout << "Usage is: " << argv[0] << " image.jpg output logo.jpg copies NW  (optional delay)" << std::endl;
		return (0);
	}
	std::string srcImg = argv[1];
	std::string destFolder = argv[2];
	std::string stamp = argv[3];
	int totalImages = atoi(argv[4]);
	int nw = atoi(argv[5]);

	if (argc == 7)
		interarrivalTime = atoi(argv[6]);

	if (argc == 6 || argc == 7)
	{
		printf("\n\n\t\t************************ FF FARM MODEL **************************************\n\n");
		CImg<unsigned char> *mark = new CImg<unsigned char>();
		CImg<unsigned char> *img = new CImg<unsigned char>();
		/******************************* Time to read image from disk ********************************************/
		auto tStart = std::chrono::high_resolution_clock::now();
		img->load(srcImg.c_str());
		auto tEnd = std::chrono::high_resolution_clock::now();
		timeToReadImg = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		/**********************************************************************************************************/

		mark->load(stamp.c_str());
		height = mark->height();
		width = mark->width();

		/******************************* Time to create copies *****************************************************/
		tStart = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < totalImages; i++)
		{
			CImg<unsigned char> *imgPointer = new CImg<unsigned char>(*img);
			std::string name = "img";
			name.append(std::to_string(i) + ".jpg");
			toEmitter.push_back(std::make_pair(name, imgPointer));
		}
		tEnd = std::chrono::high_resolution_clock::now();
		timeToCreateCopy = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

		auto tfarm = std::chrono::high_resolution_clock::now();
		/* Emitter*/
		tt_emitter emitter(nw, toEmitter);

		/* Collector */
		tt_collector collector(destFolder, nw);

		/* Worker*/
		std::vector<ff_node *> W;

		for (int i = 0; i < nw; ++i)
		{
			W.push_back(new tt_worker(i, mark));
		}

		/******************************* Create the farm **********************************************************/
		ff_farm<> farm;

		/*******************************Insert nodes to the farm **************************************************/
		farm.add_emitter(&emitter);

		farm.add_collector(&collector);

		farm.add_workers(W);

		farm.set_scheduling_ondemand();

		/******************************* Initiate the farm *********************************************************/
		if (farm.run_and_wait_end() < 0)
			std::cout << "ERROR";

		tEnd = std::chrono::high_resolution_clock::now();
		/******************************  Time to join threads *******************************************************/
		forkThread = std::chrono::duration<double, std::milli>(tEnd - tfarm).count();

		/*************************************** Terminate Execution *************************************************/
		auto tParallelEnd = std::chrono::high_resolution_clock::now();
		timeToTerminate = std::chrono::duration<double, std::milli>(tParallelEnd - tParallelStart).count();
		tStart = std::chrono::high_resolution_clock::now();
		/************************** The last incoming image will set the time ******************************************/
		completionTime = std::chrono::duration<double, std::milli>(tStart - tCompletionStart).count();

		std::cerr
			<< "\t\tMain reads the image " << width << " X " << height << " : " << timeToReadImg << " msecs\n"
			<< "\t\tMain thread creates : " << totalImages << " copies of " << srcImg << " : " << timeToCreateCopy << " msecs\n"
			<< "\t\tInterarrival time for emitter-worker queue : " << interarrivalTime << " µsecs\n"
			<< "\t\tEmitter sends all images to the workers : " << emitterTimeToPushAll << " msecs\n"
			<< "\t\tStream-Parallel time (Tseq/nw) : " << timeToTerminate << "  msecs with : " << nw << " workers  <========\n"
			<< "\t\tThe maximum time among endpoints : " << std::max((double)interarrivalTime, timeToTerminate) << " msecs\n"
			<< "\t\tFork and join threads : " << forkThread << " msecs\n"
			<< "\t\tCompletion time : " << completionTime << " msecs\n\n"
			<< "\t\t******************************************************************************\n"
			<< std::endl;
	}
	else
		std::cout << "Few parameters included (HD output logo.jpg NW)" << std::endl;

	return 0;
}