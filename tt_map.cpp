#define cimg_use_jpeg 1
#include "CImg.h"
#include <iostream>
#include <vector>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "util.h"
#include <utility>
#include <stdio.h>
#include <dirent.h>

using namespace cimg_library;

/****Map Components******/
void tt_scatter(std::vector<std::pair<std::string, CImg<unsigned char> *>> toScatter, int nw);
void tt_worker(int id, std::string stamp, Worker *ds);
void tt_gather(std::string destFolder, int nw);
/***********************/

queue<std::pair<std::string, CImg<unsigned char> *>> toGather;
std::vector<std::pair<std::string, CImg<unsigned char> *>> toScatter;
double forkThread = 0, timeToSetupEnv = 0, timeToReadImg = 0, timeToCreateCopy = 0, scatterTimeToPushAll = 0, gatherInterdeparture = 0, timeToTerminate = 0;
int height = 0, width = 0, count = 0;
auto tParallelStart = std::chrono::high_resolution_clock::now();
auto tCompletionStart = std::chrono::high_resolution_clock::now();
int interarrivalTime = 10;
std::atomic<long> completionTime(0);
std::vector<CImg<unsigned char> *> markVec;

Worker **workerObject;

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        std::cout << "Usage is: " << argv[0] << " image.jpg output logo.jpg copies NW  (optional delay)" << std::endl;
        return (0);
    }

    /******************************* Add optional delay *************************************************/
    if (argc == 7)
        interarrivalTime = atoi(argv[6]);

    if (argc == 6 || argc == 7)
    {
        std::string srcImg = argv[1];
        std::string destFolder = argv[2];
        std::string stamp = argv[3];
        int totalImages = atoi(argv[4]);
        int nw = atoi(argv[5]);
        printf("\n\n\t\t******************************* C++ MAP MODEL *************************************\n\n");
        CImg<unsigned char> *mark = new CImg<unsigned char>();
        CImg<unsigned char> *img = new CImg<unsigned char>();
        /******************************* Time to read image from disk ************************************/
        auto tStart = std::chrono::high_resolution_clock::now();
        img->load(srcImg.c_str());
        auto tEnd = std::chrono::high_resolution_clock::now();
        timeToReadImg = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        /***************************************************************************************************/

        mark->load(stamp.c_str());
        height = mark->height();
        width = mark->width();
        int delta{height / nw};

        for (int i = 0; i < nw; i++)
        {
            CImg<unsigned char> *markPointer = new CImg<unsigned char>(*mark);
            markVec.push_back(markPointer);
        }

        /******************************* Time to create copies *********************************************/
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
        workerObject = new Worker *[nw];
        for (int i = 0; i < nw; i++)
            workerObject[i] = new Worker();
        /********************** Create the chunks and set it inside the  workers  ***********************/

        for (int i = 0; i < nw; i++)
        {
            workerObject[i]->start = (i != 0 ? i * delta : 0);
            workerObject[i]->end = (i != (nw - 1) ? (i + 1) * delta : height);
        }
        /************************************************************************************************/

        tEnd = std::chrono::high_resolution_clock::now();
        timeToSetupEnv = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        /******************************  Time to fork threads *******************************************/
        tStart = std::chrono::high_resolution_clock::now();
        std::vector<std::thread> threads;
        /* Scatter */
        threads.push_back(std::thread(tt_scatter, toScatter, nw));
        /* Workers */
        for (int id = 0; id < nw; id++)
            threads.push_back(std::thread(tt_worker, id, stamp, std::ref(workerObject[id])));
        /* Gather */
        threads.push_back(std::thread(tt_gather, destFolder, nw));

        for (auto &th : threads)
            th.join();
        tEnd = std::chrono::high_resolution_clock::now();
        /******************************  Time to join threads *******************************************/
        forkThread = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

        std::cerr << "\t\tMain thread initializes worker objects : " << timeToSetupEnv << " msecs\n"
                  << "\t\tMain reads the image " << width << " X " << height << " : " << timeToReadImg << " msecs\n"
                  << "\t\tMain thread creates : " << totalImages << " copies of " << srcImg << " : " << timeToCreateCopy << " msecs\n"
                  << "\t\tInterarrival time for scatter-worker queue : " << interarrivalTime << " µsecs\n"
                  << "\t\tScatter sends all images to the workers : " << scatterTimeToPushAll << " msecs\n"
                  << "\t\tInterdeparture time for worker-gather queue : " << gatherInterdeparture << " msecs\n"
                  << "\t\tData-Parallel time (Tseq/nw) : " << timeToTerminate << "  msecs with : " << nw << " workers  <===========\n"
                  << "\t\tTmax(Ta,Tid,Td) : " << std::max(std::max((double)interarrivalTime, gatherInterdeparture), timeToTerminate) << " msecs\n"
                  << "\t\tFork and join threads : " << forkThread << " msecs\n"
                  << "\t\tCompletion time : " << completionTime << " msecs\n\n"
                  << "\t\t**********************************************************************************\n"
                  << std::endl;
    }
    else
        std::cout << "Few/More parameters included (image.jpg output logo.jpg copies NW)" << std::endl;

    return 0;
}
/**********************************  SCATTER  ************************************************************/
/*********************************************************************************************************/

void tt_scatter(std::vector<std::pair<std::string, CImg<unsigned char> *>> toScatter, int nw)
{
    /********************** Start time for the parallel execution. Ending time on line 195 ****************/
    tParallelStart = std::chrono::high_resolution_clock::now();
    /*************************** Push all images in Round Robin into the workers **************************/
    for (const auto &temp : toScatter)
    {
        for (int i = 0; i < nw; i++)
        {
            /*************************** Get the time of the first image sent *****************************/
            if (i == 0)
                tCompletionStart = std::chrono::high_resolution_clock::now();
            workerObject[i]->tuple.push(temp);
        }
        active_delay(interarrivalTime);
    }
    auto tEnd = std::chrono::high_resolution_clock::now();
    scatterTimeToPushAll = std::chrono::duration<double, std::milli>(tEnd - tParallelStart).count();
    /*******************************************************************************************************/
    auto tStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < nw; i++)
    {
        workerObject[i]->tuple.push(std::make_pair("", nullptr));
    }
    return;
}
/*******************************************WORKER *********************************************************/
/***********************************************************************************************************/
void tt_worker(int id, std::string stamp, Worker *ds)
{
    unsigned char *imgPtr;
    int imgReceived = 0;
    double idealService = 0, workerTimeToPush = 0, workerTimeToPop = 0;
    std::pair<std::string, CImg<unsigned char> *> toGatherPair;

    auto tStart = std::chrono::high_resolution_clock::now();
    /********************** Load a copy of the mark in each worker only once in each worker *****************/
    unsigned char *markPtr = markVec[id]->data();
    auto tEnd = std::chrono::high_resolution_clock::now();
    /*********************************************************************************************************/
    while (true)
    {
        toGatherPair = ds->tuple.pop();

        if (toGatherPair.second == nullptr)
        {
            toGather.push(std::make_pair("", nullptr));
            shared_print(id, idealService, workerTimeToPush, imgReceived, 0);
            return;
        }
        imgPtr = toGatherPair.second->data();

        /************************************ Time to procees a single image  *********************************/
        tStart = std::chrono::high_resolution_clock::now();
        int h = ds->end - ds->start;
        int s = ds->start;

        for (int i = 0; i < (width * h); i++)
        {
            if ((int)markPtr[i + s * width] < 50)
            {
                imgPtr[i + s * width] = ((int)markPtr[i + s * width] + ((int)imgPtr[i + s * width] * 0.3)) / 2;
                imgPtr[i + s * width + (height * width)] = ((int)markPtr[i + s * width] + ((int)imgPtr[i + s * width + (height * width)] * 0.59)) / 2;
                imgPtr[i + s * width + 2 * (height * width)] = ((int)markPtr[i + s * width] + ((int)imgPtr[i + s * width + 2 * (height * width)] * 0.11)) / 2;
            }
        }

        if (imgReceived == 0)
        {
            tEnd = std::chrono::high_resolution_clock::now();
            idealService = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        }
        /*******************************************************************************************************/
        tStart = std::chrono::high_resolution_clock::now();
        toGather.push(std::make_pair(toGatherPair.first, toGatherPair.second));
        if (imgReceived == 0)
        {
            /*********************** Time to push a processed image into worker-gather queue********************/
            tEnd = std::chrono::high_resolution_clock::now();
            workerTimeToPush = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
            /***************************************************************************************************/
        }
        imgReceived++;
    }
    return;
}
/*****************************************  GATHER    **********************************************************/
/***************************************************************************************************************/
void tt_gather(std::string destFolder, int nw)
{
    int endFlag = 0;
    int imgPartition = 0;
    int imgReceived = 0;
    std::vector<std::string> buffer;
    std::pair<std::string, CImg<unsigned char> *> toDestDataPair;
    auto tStart = std::chrono::high_resolution_clock::now();
    auto tempInterdeparture = std::chrono::high_resolution_clock::now();
    while (true)
    {
        toDestDataPair = toGather.pop();

        /*****************************************  Recieve all nullpointer   **********************************/
        if (toDestDataPair.second == nullptr)
        {
            endFlag++;
        }

        /****************************************   Terminate Execution    *************************************/
        if (endFlag == nw)
        {
            /**************  Take end time for the parallel execution. Starting time on line 106  ***************/
            auto tParallelEnd = std::chrono::high_resolution_clock::now();
            timeToTerminate = std::chrono::duration<double, std::milli>(tParallelEnd - tParallelStart).count();
            return;
        }
        /**********  Buffer the image and wait for other processed partitions of the same image   **************/
        if (toDestDataPair.first != "")
        {
            buffer.push_back(toDestDataPair.first);
        }
        if (std::find(buffer.begin(), buffer.end(), toDestDataPair.first) != buffer.end())
        {
            imgPartition++;
        }
        tStart = std::chrono::high_resolution_clock::now();
        /********************************************* Got complete image  ***************************************/
        if (imgPartition == nw)
        {
            if (imgReceived == 0)
            {
                /*********************  Time to pop one processed image from worker-gather queue *****************/
                tempInterdeparture = std::chrono::high_resolution_clock::now();
                /*************************************************************************************************/
            }
            if (imgReceived == 1)
            {
                /******************* Time to pop the next consecutive  image from worker-gather queue ************/
                auto tEnd = std::chrono::high_resolution_clock::now();
                gatherInterdeparture = std::chrono::duration<double, std::milli>(tEnd - tempInterdeparture).count();
                /*************************************************************************************************/
            }
            tStart = std::chrono::high_resolution_clock::now();
            /************************** The last incoming image will set the time ********************************/
            completionTime = std::chrono::duration<double, std::milli>(tStart - tCompletionStart).count();
            buffer.erase(std::remove(buffer.begin(), buffer.end(), toDestDataPair.first), buffer.end());
            /***************************** Uncomment if you want to save the images to the disk  *****************/
            // std::string path = destFolder + '/' + toDestDataPair.first.c_str();
            // toDestDataPair.second->save(path.c_str());
            imgPartition = 0;
        }
        imgReceived++;
    }
}
