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

using namespace cimg_library;
using namespace ff;

FF_Worker **workerObject;
typedef std::pair<std::string, CImg<unsigned char> *> task;
typedef std::pair<task *, FF_Worker *> workerTask;

double forkThread = 0, timeToSetupEnv = 0, timeToReadImg = 0, timeToCreateCopy = 0, scatterTimeToPushAll = 0, timeToTerminate = 0;
int height = 0, width = 0, count = 0;
auto tParallelStart = std::chrono::high_resolution_clock::now();
auto tCompletionStart = std::chrono::high_resolution_clock::now();
int interarrivalTime = 10;
std::vector<task> toScatter;
std::atomic<long> completionTime(0);

struct tt_scatter : ff_node_t<char, workerTask>
{
	int imgSend;
	int nw;
	std::vector<task> toScatter;
	FF_Worker **toWorkerObject;
	tt_scatter(int nw, std::vector<task> toScatter, FF_Worker **toWorkerObject) : nw(nw), toScatter(toScatter), toWorkerObject(toWorkerObject) {}

	workerTask *svc(char *)
	{
		int imgSend = 0;
		/**************************Start time for the parallel execution. Ending time on line 195***************************/
		tParallelStart = std::chrono::high_resolution_clock::now();
		for (auto &temp : toScatter)
		{
			if (imgSend == 0)
				tCompletionStart = std::chrono::high_resolution_clock::now();

			for (int i = 0; i < nw; i++)
			{
				workerTask *ptrPair = new workerTask(&temp, toWorkerObject[i]);
				ff_send_out(ptrPair);
			}
			active_delay(interarrivalTime);
			imgSend++;
		}
		auto tEnd = std::chrono::high_resolution_clock::now();
		scatterTimeToPushAll = std::chrono::duration<double, std::milli>(tEnd - tParallelStart).count();
		/********************************************************************************************************************/
		return EOS;
	}
};

struct tt_worker : ff_node_t<workerTask, task>
{
	CImg<unsigned char> *mark;
	unsigned char *markPtr;
	unsigned char *imgPtr;
	int partitionReceived;
	double idealService;
	FF_Worker *ds;
	task *workerPair;
	int id;
	int h;
	int s;

	tt_worker(int id, CImg<unsigned char> *mark) : id(id), mark(mark)
	{
		/********************** Load a copy of the mark in each worker only once in each worker *****************************/
		markPtr = mark->data();
		partitionReceived = 0;
		idealService = 0;
	}
	task *svc(workerTask *toWorkerTask)
	{
		if (toWorkerTask)
		{
			workerPair = toWorkerTask->first;
			ds = toWorkerTask->second;
			h = ds->end - ds->start;
			s = ds->start;
			/************************************ Time to procees a single partition of an image  ****************************/
			auto tStart = std::chrono::high_resolution_clock::now();

			imgPtr = workerPair->second->data();

			for (int i = 0; i < (width * h); i++)
			{
				if ((int)markPtr[i + s * width] < 50)
				{
					imgPtr[i + s * width] = ((int)markPtr[i + s * width] + ((int)imgPtr[i + s * width] * 0.3)) / 2;
					imgPtr[i + s * width + (height * width)] = ((int)markPtr[i + s * width] + ((int)imgPtr[i + s * width + (height * width)] * 0.59)) / 2;
					imgPtr[i + s * width + 2 * (height * width)] = ((int)markPtr[i + s * width] + ((int)imgPtr[i + s * width + 2 * (height * width)] * 0.11)) / 2;
				}
			}
			if (partitionReceived == 0)
			{
				auto tEnd = std::chrono::high_resolution_clock::now();
				idealService = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			}
			partitionReceived++;
			return workerPair;
		}
		else
		{
			return EOS;
		}
	}

	void svc_end()
	{
		ff_shared_print(id, idealService, partitionReceived, 0);
		return;
	}
};

struct tt_gather : ff_minode_t<task, char>
{
	int imgPartition;
	std::vector<std::string> buffer;
	std::string destPath;
	task toDestDataPair;
	std::string destFolder;
	int nw;
	tt_gather(std::string destFolder, int nw) : destFolder(destFolder), nw(nw)
	{
		imgPartition = 0;
	}

	char *svc(task *toDestDataPair)
	{
		if (toDestDataPair)
		{
			auto tStartGather = std::chrono::high_resolution_clock::now();
			/**********  Buffer the image and wait for other processed partitions of the same image   **************/
			if (toDestDataPair->first != "")
			{
				buffer.push_back(toDestDataPair->first);
			}
			if (std::find(buffer.begin(), buffer.end(), toDestDataPair->first) != buffer.end())
			{
				imgPartition++;
			}
			/********************************************* Got complete image  ***************************************/
			if (imgPartition == nw)
			{
				tStartGather = std::chrono::high_resolution_clock::now();
				/************************** The last incoming image will set the time ********************************/
				completionTime = std::chrono::duration<double, std::milli>(tStartGather - tCompletionStart).count();
				buffer.erase(std::remove(buffer.begin(), buffer.end(), toDestDataPair->first), buffer.end());
				/***************************** Uncomment if you want to save the images to the disk  *****************/
				// std::string path = destFolder + '/' + toDestDataPair->first.c_str();
				// toDestDataPair->second->save(path.c_str());
				imgPartition = 0;
			}
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
		printf("\n\n\t\t******************************* FAST FLOW MAP MODEL **************************************\n\n");
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
		int delta{height / nw};

		/******************************* Time to create copies *****************************************************/
		tStart = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < totalImages; i++)
		{
			CImg<unsigned char> *imgPointer = new CImg<unsigned char>(*img);
			std::string name = "img";
			name.append(std::to_string(i) + ".jpg");
			toScatter.push_back(std::make_pair(name, imgPointer));
		}
		tEnd = std::chrono::high_resolution_clock::now();
		timeToCreateCopy = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

		/******************************* Time to setup environment **************************************/
		tStart = std::chrono::high_resolution_clock::now();
		workerObject = new FF_Worker *[nw];
		for (int i = 0; i < nw; i++)
			workerObject[i] = new FF_Worker();
		/********************** Create the chunks and set it inside the  workers  ***********************/
		for (int i = 0; i < nw; i++)
		{
			workerObject[i]->start = (i != 0 ? i * delta : 0);
			workerObject[i]->end = (i != (nw - 1) ? (i + 1) * delta : height);
		}
		/************************************************************************************************/
		tEnd = std::chrono::high_resolution_clock::now();
		timeToSetupEnv = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

		auto tmap = std::chrono::high_resolution_clock::now();

		/*********************************** scatter node ***********************************************/
		tt_scatter scatter(nw, toScatter, workerObject);

		/*********************************** gather node ***********************************************/
		tt_gather gather(destFolder, nw);

		/*********************************** worker nodes **********************************************/
		std::vector<ff_node *> W;

		for (int i = 0; i < nw; ++i)
		{
			W.push_back(new tt_worker(i, mark));
		}

		/*********************************** MAP creation ***********************************************/
		ff_farm<> map;

		/*********************************** add nodes to it ********************************************/
		map.add_emitter(&scatter);

		map.add_collector(&gather);

		map.add_workers(W);
		/************************************ use ondemand scheduling  ***********************************/
		map.set_scheduling_ondemand();

		/*********************************** start the map ***********************************************/
		if (map.run_and_wait_end() < 0)
			std::cout << "ERROR";

		tEnd = std::chrono::high_resolution_clock::now();
		/******************************  Time to join threads ********************************************/
		forkThread = std::chrono::duration<double, std::milli>(tEnd - tmap).count();

		/*************************************** Terminate Execution **************************************/
		auto tParallelEnd = std::chrono::high_resolution_clock::now();
		timeToTerminate = std::chrono::duration<double, std::milli>(tParallelEnd - tParallelStart).count();

		std::cerr << "\t\tMain thread initializes worker objects : " << timeToSetupEnv << " msecs\n"
				  << "\t\tMain reads the image " << width << " X " << height << " : " << timeToReadImg << " msecs\n"
				  << "\t\tMain thread creates : " << totalImages << " copies of " << srcImg << " : " << timeToCreateCopy << " msecs\n"
				  << "\t\tInterarrival time for scatter-worker queue : " << interarrivalTime << " µsecs\n"
				  << "\t\tScatter sends all images to the workers : " << scatterTimeToPushAll << " msecs\n"
				  << "\t\tData-Parallel time (Tseq/nw) : " << timeToTerminate << "  msecs with : " << nw << " workers  <========\n"
				  << "\t\tTmax(Ta,Tid,Td) : " << std::max((double)interarrivalTime, timeToTerminate) << " msecs\n"
				  << "\t\tFork and join threads : " << forkThread << " msecs\n"
				  << "\t\tCompletion time : " << completionTime << " msecs\n\n"
				  << "\t\t**********************************************************************************\n"
				  << std::endl;
	}
	else
		std::cout << "Few parameters included (HD output logo.jpg NW)" << std::endl;

	return 0;
}