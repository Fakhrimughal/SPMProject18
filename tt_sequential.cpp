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
#define pixelBorder = 50

using namespace cimg_library;

void sequential(std::vector<std::pair<std::string, CImg<unsigned char> *>> toSequential, std::string destFolder, CImg<unsigned char> *mark);
double timeSequential = 0, timeToReadStamp = 0, timeToReadImg = 0, timeToCreateCopy = 0, sequentialTime = 0, completionTime = 0, interarrival = 0, interdeparture = 0, serviceTime = 0, timeToSaveSingle = 0;
int height = 0, width = 0, count = 0;
auto tCompletionStart = std::chrono::high_resolution_clock::now();
std::vector<CImg<unsigned char> *> markVec;
std::vector<std::pair<std::string, CImg<unsigned char> *>> toSequential;

int main(int argc, char *argv[])
{
	if (argc == 1)
	{
		std::cout << "Usage is: " << argv[0] << " img output logo.jpg num_of_images" << std::endl;
		return (0);
	}

	std::string srcImg = argv[1];
	std::string destFolder = argv[2];
	std::string stamp = argv[3];
	int totalImages = atoi(argv[4]);
	if (argc == 5)
	{
		printf("\n\n**************************** SEQUENTIAL MODEL  ***********************************\n\n");
		CImg<unsigned char> *mark = new CImg<unsigned char>();
		CImg<unsigned char> *img = new CImg<unsigned char>();
		/******************************* Time to read image from disk **************************************/
		auto tStart = std::chrono::high_resolution_clock::now();
		img->load(srcImg.c_str());
		auto tEnd = std::chrono::high_resolution_clock::now();
		timeToReadImg = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		/***************************************************************************************************/

		/******************************* Time to read mark from disk ***************************************/
		tStart = std::chrono::high_resolution_clock::now();
		mark->load(stamp.c_str());
		height = mark->height();
		width = mark->width();
		tEnd = std::chrono::high_resolution_clock::now();
		timeToReadStamp = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		/***************************************************************************************************/

		/******************************* Time to create copies *********************************************/
		tStart = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < totalImages; i++)
		{
			CImg<unsigned char> *imgPointer = new CImg<unsigned char>(*img);
			std::string name = "img";
			name.append(std::to_string(i) + ".jpg");
			toSequential.push_back(std::make_pair(name, imgPointer));
		}
		tEnd = std::chrono::high_resolution_clock::now();
		timeToCreateCopy = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

		/******************************* Call the function  ************************************************/
		sequential(toSequential, destFolder, mark);
		tEnd = std::chrono::high_resolution_clock::now();
		timeSequential = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

		std::cerr
			<< "\t\tReading the watermark " << width << " X " << height << " : " << timeToReadStamp << " msecs\n"
			<< "\t\tReading the image " << width << " X " << height << " : " << timeToReadImg << " msecs\n"
			<< "\t\tCreate " << totalImages << " copies of " << srcImg << " : " << timeToCreateCopy << " msecs\n"
			<< "\t\tInterarrival time (Ta) : " << interarrival << " msecs\n"
			<< "\t\tService Time : " << serviceTime << " msecs\n"
			<< "========>       Sequential execution (Tseq) : " << sequentialTime << "  msecs   \t\t<========\n"
			<< "\t\tInterdeparture time (Td) : " << interdeparture << " msecs\n"
			<< "\t\tSaving single image : " << timeToSaveSingle << " msecs\n"
			<< "\t\tService Time : " << serviceTime << " msecs\n"
			<< "\t\tThe maximum time among endpoints : " << std::max(std::max(interarrival, interdeparture), serviceTime) << " msecs\n"
			<< "\t\tCompletion time (Tc) : " << completionTime << " msecs\n"
			<< std::endl;
		if ((serviceTime > interarrival) && (serviceTime > interdeparture))
			std::cerr << "\t\tBottleneck !!! " << std::endl;
		std::cerr << "\n\n**********************************************************************************\n";
	}
	else
		std::cout << "Few/More parameters included (image.jpg output logo.jpg copies)" << std::endl;
	return 0;
}

void sequential(std::vector<std::pair<std::string, CImg<unsigned char> *>> toSequential, std::string destFolder, CImg<unsigned char> *mark)
{
	unsigned char *imgPtr;
	/*********************************************** Time to read from memory *****************************************************/
	auto tStart = std::chrono::high_resolution_clock::now();
	unsigned char *markPtr = mark->data();
	auto tEnd = std::chrono::high_resolution_clock::now();
	interarrival = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	/******************************************************************************************************************************/
	std::string destPath;
	int imgIndex = 0;

	/******************************************** Start time for the sequential execution. Ending time on line 195 ****************/
	auto tSequential = std::chrono::high_resolution_clock::now();

	for (const auto &temp : toSequential)
	{
		/*************************************************** Get the time of the first image sent *********************************/
		if (imgIndex == 0)
			tCompletionStart = std::chrono::high_resolution_clock::now();

		imgPtr = temp.second->data();

		/******************************* time to process a single image  **********************************************************/
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
		if (imgIndex == 0)
		{
			/* Get the end time needed to process a single image  */
			tEnd = std::chrono::high_resolution_clock::now();
			serviceTime = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		}
		imgIndex++;
	}
	/******************************888 Get the time when the last image is send out  ***********************************************/
	tEnd = std::chrono::high_resolution_clock::now();
	sequentialTime = std::chrono::duration<double, std::milli>(tEnd - tSequential).count();
	/* Reset counter*/
	imgIndex = 0;

	tStart = std::chrono::high_resolution_clock::now();
	for (const auto &temp : toSequential)
	{
		destPath = (destFolder + "\\" + temp.first).c_str();
		if (imgIndex == 0)
		{
			/*********************  Time to send a processed image to memory********************************************************/
			auto tEnd = std::chrono::high_resolution_clock::now();
			interdeparture = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			/***********************************************************************************************************************/
		}

		/**************************************** time to save an image ************************************************************/
		// tStart = std::chrono::high_resolution_clock::now();
		// temp.second->save(destPath.c_str());
		/***************************************************************************************************************************/

		if (imgIndex == 0)
		{
			/************************************** Time to save a single image into the memory  ***********************************/
			tEnd = std::chrono::high_resolution_clock::now();
			timeToSaveSingle = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		}
		/************************************* The last incoming image will set the time ********************************************/
		tEnd = std::chrono::high_resolution_clock::now();
		completionTime = std::chrono::duration<double, std::milli>(tEnd - tCompletionStart).count();
		imgIndex++;
	}
}
