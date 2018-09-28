#define cimg_use_jpeg 1
#include "CImg.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "util.h"
#include <utility>
#include <atomic>
#include <stdio.h>
#include <dirent.h>

using namespace cimg_library;

/***** Farm Components ****/
void tt_emitter(int nw);
void tt_worker(int id);
void tt_collector(std::string destFolder, int nw);
/**********************/

queue<std::pair<std::string, CImg<unsigned char> *>> toWorker, toCollector, toDrain;
double forkThread = 0, timeToReadImg = 0, timeToCreateCopy = 0, emitterTimeToPushAll = 0, collectorInterdeparture = 0, timeToTerminate = 0;
int height = 0, width = 0, count = 0;
auto tParallelStart = std::chrono::high_resolution_clock::now();
auto tCompletionStart = std::chrono::high_resolution_clock::now();
int interarrivalTime = 10;
std::vector<std::pair<std::string, CImg<unsigned char> *>> toEmitter;
std::atomic<long> completionTime(0);
std::vector<CImg<unsigned char> *> markVec;

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		std::cout << "Usage is: " << argv[0] << " image.jpg output logo.jpg copies NW  (optional delay)" << std::endl;
		return (0);
	}

	if (argc == 7)
		interarrivalTime = atoi(argv[6]);

	if (argc == 6 || argc == 7)
	{
		std::string srcImg = argv[1];
		std::string destFolder = argv[2];
		std::string stamp = argv[3];
		int totalImages = atoi(argv[4]);
		int nw = atoi(argv[5]);
		printf("\n\n\t\t*******************************  C++ FARM MODEL ************************************\n\n");
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

		for (int i = 0; i < nw; i++)
		{
			CImg<unsigned char> *markPointer = new CImg<unsigned char>(*mark);
			markVec.push_back(markPointer);
		}

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

		tStart = std::chrono::high_resolution_clock::now();
		std::vector<std::thread> threads;
		/* Emitter */
		threads.push_back(std::thread(tt_emitter, nw));
		/* Workers */
		for (int id = 0; id < nw; id++)
			threads.push_back(std::thread(tt_worker, id));
		/* Collector */
		threads.push_back(std::thread(tt_collector, destFolder, nw));
		for (auto &th : threads)
			th.join();

		tEnd = std::chrono::high_resolution_clock::now();
		/******************************  Time to join threads *********************************************************/
		forkThread = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

		std::cerr
			<< "\t\tMain reads the image " << width << " X " << height << " : " << timeToReadImg << " msecs\n"
			<< "\t\tMain thread creates : " << totalImages << " copies of " << srcImg << " : " << timeToCreateCopy << " msecs\n"
			<< "\t\tInterarrival time for emitter-worker queue : " << interarrivalTime << " µsecs\n"
			<< "\t\tEmitter sends all images to the workers : " << emitterTimeToPushAll << " msecs\n"
			<< "\t\tInterdeparture time for worker-collector queue : " << collectorInterdeparture << " msecs\n"
			<< "\t\tStream-Parallel time (Tseq/nw) : " << timeToTerminate << "  msecs with : " << nw << " workers  <========\n"
			<< "\t\tTmax(Ta,Tid,Td) : " << std::max(std::max((double)interarrivalTime, collectorInterdeparture), timeToTerminate) << " msecs\n"
			<< "\t\tFork and join threads : " << forkThread << " msecs\n"
			<< "\t\tCompletion time : " << completionTime << " msecs\n\n"
			<< "\t\t**********************************************************************************\n"
			<< std::endl;
	}
	else
		std::cout << "Few/More parameters included (image.jpg output logo.jpg copies NW)" << std::endl;

	return 0;
}

/*********************************************  EMITTER ****************************************************************/
/***********************************************************************************************************************/
void tt_emitter(int nw)
{
	int imgSend = 0;
	/**************************Start time for the parallel execution. Ending time on line 195***************************/
	tParallelStart = std::chrono::high_resolution_clock::now();

	for (const auto &temp : toEmitter)
	{
		if (imgSend == 0)
			tCompletionStart = std::chrono::high_resolution_clock::now();

		toWorker.push(temp);
		active_delay(interarrivalTime);
		imgSend++;
	}
	auto tEnd = std::chrono::high_resolution_clock::now();
	emitterTimeToPushAll = std::chrono::duration<double, std::milli>(tEnd - tParallelStart).count();
	/********************************************************************************************************************/
	for (int i = 0; i < nw; i++)
	{
		toWorker.push(std::make_pair("", nullptr));
	}
	return;
}

/**********************************************WORKER*********************************************************************/
/*************************************************************************************************************************/
void tt_worker(int id)
{
	CImg<unsigned char> *mark = new CImg<unsigned char>();
	unsigned char *imgPtr;
	int imgReceived = 0;
	double idealService = 0, workerTimeToPush = 0, workerTimeToPop = 0;
	std::pair<std::string, CImg<unsigned char> *> toCollectorDataPair;

	auto tStart = std::chrono::high_resolution_clock::now();
	/********************** Load a copy of the mark in each worker only once in each worker *****************************/
	unsigned char *markPtr = markVec[id]->data();
	auto tEnd = std::chrono::high_resolution_clock::now();

	/********************************************************************************************************************/
	while (true)
	{
		toCollectorDataPair = toWorker.pop();

		if (toCollectorDataPair.second == nullptr)
		{
			toCollector.push(std::make_pair("", nullptr));
			shared_print(id, idealService, workerTimeToPush, imgReceived, 1);
			return;
		}
		imgPtr = toCollectorDataPair.second->data();

		/************************************ Time to procees a single image  *******************************************/
		tStart = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < (width * height); i++)
		{
			if ((int)markPtr[i] < 50)
			{
				imgPtr[i] = ((int)markPtr[i] + ((int)imgPtr[i] * 0.3)) / 2;
				imgPtr[i + width * height] = ((int)markPtr[i] + ((int)imgPtr[i + width * height] * 0.59)) / 2;
				imgPtr[i + 2 * width * height] = ((int)markPtr[i] + ((int)imgPtr[i + 2 * width * height] * 0.11)) / 2;
			}
		}
		if (imgReceived == 0)
		{
			tEnd = std::chrono::high_resolution_clock::now();
			idealService = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		}
		/*****************************************************************************************************************/

		tStart = std::chrono::high_resolution_clock::now();
		toCollector.push(std::make_pair(toCollectorDataPair.first, toCollectorDataPair.second));

		if (imgReceived == 0)
		{
			/******************************* Time to push a processed image into worker-collector queue********************/
			tEnd = std::chrono::high_resolution_clock::now();
			workerTimeToPush = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			/**************************************************************************************************************/
		}
		imgReceived++;
	}
	return;
}

/***********************************************COLLECTOR*******************************************************************/
/***************************************************************************************************************************/
void tt_collector(std::string destFolder, int nw)
{
	int nullPointer = 0;
	int imgReceived = 0;
	std::string destPath;
	std::pair<std::string, CImg<unsigned char> *> toDestDataPair;

	auto tStart = std::chrono::high_resolution_clock::now();
	while (true)
	{
		tStart = std::chrono::high_resolution_clock::now();
		toDestDataPair = toCollector.pop();
		if (imgReceived == 1)
		{
			/*************************  Time to pop a processed image from worker-collector queue *************************/
			auto tEnd = std::chrono::high_resolution_clock::now();
			collectorInterdeparture = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			/**************************************************************************************************************/
		}
		/*************************************** count the null pointers recieved *****************************************/
		if (toDestDataPair.second == nullptr)
		{
			nullPointer++;
		}
		if (nullPointer == nw)
		{
			/*************************************** Terminate Execution ***************************************************/
			auto tParallelEnd = std::chrono::high_resolution_clock::now();
			timeToTerminate = std::chrono::duration<double, std::milli>(tParallelEnd - tParallelStart).count();
			return;
		}
		if (toDestDataPair.second != nullptr)
		{
			tStart = std::chrono::high_resolution_clock::now();
			/************************** The last incoming image will set the time ******************************************/
			completionTime = std::chrono::duration<double, std::milli>(tStart - tCompletionStart).count();
			/***************************** Uncomment if you want to save the images to the disk  ***************************/
			// std::string path = destFolder + '/' + toDestDataPair.first.c_str();
			// toDestDataPair.second->save(path.c_str());
		}
		imgReceived++;
	}
	return;
}