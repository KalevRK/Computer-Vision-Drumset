// singleCameraProject.cpp : Defines the entry point for the console application.

/*  Kalev Roomann-Kurrik
	3/30/2011

	Interim Senior Design Project

	A single camera computer vision-based percussion system.
	These components will be combined with the stereo imaging in TwoFeedTest
	to produce the final two camera three dimensional senior project
*/

#include "stdafx.h"
#include "math.h"

#include <windows.h>
#include <mmsystem.h>

#include <cv.h>
#include <cxcore.h>
#include <highgui.h>


int main(int argc, CHAR* argv[])
{
	CvCapture* capture;

	IplImage* frame;
	
	capture = cvCreateCameraCapture(0);

	assert(capture != NULL);

	frame = cvQueryFrame(capture);

	IplImage* prev_frame = cvCreateImage(cvGetSize(frame), 8, 3);
	IplImage* grayFrame = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 1);
	IplImage* grayPrevFrame = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 1);

	// Component images
	IplImage* red = cvCreateImage(cvGetSize(frame), 8, 1);
	IplImage* green = cvCreateImage(cvGetSize(frame), 8, 1);
	IplImage* blue = cvCreateImage(cvGetSize(frame), 8, 1);

	// HSV image
	IplImage* hsvFrame = cvCreateImage(cvGetSize(frame), 8, 3);

	// Threshold image
	IplImage* threshFrame = cvCreateImage(cvGetSize(frame), 8, 1);

	// Setup prev_frame and frame
	frame = cvQueryFrame(capture);
	cvCopy(frame,prev_frame,0);
	frame = cvQueryFrame(capture);

	// Matrices to store x,y locations of tracked features for two images in sequence
	int *firstX = new int[100];
	int *firstY = new int[100];
	int *secondX = new int[100];
	int *secondY = new int[100];

	for(int k=0;k<100;k++)
	{
		firstX[k] = 0;
		firstY[k] = 0;
		secondX[k] = 0;
		secondY[k] = 0;
	}

	int firstCounter = 0;
	int secondCounter = 0;

	// Flags for whether a specific hit box had a tracked point in it the previous frame
	int snareFlag = 0;
	int hihatFlag = 0;
	int kickFlag = 0;

	// Flags for which sounds to play each frame
	int playSnare = 0;
	int playHihat = 0;
	int playKick = 0;


	// Take one frame first to start things off for comparing two frames
		// Capture the new current frame
		frame = cvQueryFrame(capture);
	
		/* Tracking Red Object
		   - If this works then can tape tip of drumsticks and shoes red to track them
		   - Needs to be fast and reliable
		 */

		// Convert Image to HSV colorspace
		cvCvtColor(frame, hsvFrame, CV_BGR2HSV);

		// Threshold in HSV colorspace
		cvInRangeS(hsvFrame, cvScalar(0, 130, 60), cvScalar(8, 255, 120), threshFrame);

		cvShowImage("Thresholded HSV Image", threshFrame);

		// Process thresholded image block by block and see where the areas of greatest intensity are
		// At those locations right now just draw boxes over the feed image and display that

		int sum = 0;

		// Go through with a 11x11 block across the 640x480 image
		for(int y=6;y<(threshFrame->height-10);y=y+10)
		{
			// Get pointers to desired rows
			uchar* pointer1 = (uchar*) (threshFrame->imageData + (y-5) * threshFrame->widthStep);
			uchar* pointer2 = (uchar*) (threshFrame->imageData + (y-4) * threshFrame->widthStep);
			uchar* pointer3 = (uchar*) (threshFrame->imageData + (y-3) * threshFrame->widthStep);
			uchar* pointer4 = (uchar*) (threshFrame->imageData + (y-2) * threshFrame->widthStep);
			uchar* pointer5 = (uchar*) (threshFrame->imageData + (y-1) * threshFrame->widthStep);
			uchar* pointer6 = (uchar*) (threshFrame->imageData + (y) * threshFrame->widthStep);
			uchar* pointer7 = (uchar*) (threshFrame->imageData + (y+1) * threshFrame->widthStep);
			uchar* pointer8 = (uchar*) (threshFrame->imageData + (y+2) * threshFrame->widthStep);
			uchar* pointer9 = (uchar*) (threshFrame->imageData + (y+3) * threshFrame->widthStep);
			uchar* pointer10 = (uchar*) (threshFrame->imageData + (y+4) * threshFrame->widthStep);
			uchar* pointer11 = (uchar*) (threshFrame->imageData + (y+5) * threshFrame->widthStep);

			// Go through the current row of the image by intervals of 10
			for(int x=6;x<(threshFrame->width-10);x=x+10)
			{
				// reset sum
				sum = 0;

				// add up pixel values in 11x11 box
				for(int i = 0; i<6; i++)
				{
					if(i==0)
					{
						sum = sum + pointer1[x] + pointer2[x] + pointer3[x] + pointer4[x] + pointer5[x] + pointer6[x]
						+ pointer7[x] + pointer8[x] + pointer9[x] + pointer10[x] + pointer11[x];
					}
					else
					{
						sum = sum + pointer1[x-i] + pointer1[x+i] + pointer2[x-i] + pointer2[x+i]
					    + pointer3[x-i] + pointer3[x+i] + pointer4[x-i] + pointer4[x+i]
						+ pointer5[x-i] + pointer5[x+i] + pointer6[x-i] + pointer6[x+i]
						+ pointer7[x-i] + pointer7[x+i] + pointer8[x-i] + pointer8[x+i]
						+ pointer9[x-i] + pointer9[x+i] + pointer10[x-1] + pointer10[x+i]
						+ pointer11[x-i] + pointer11[x+i];
					}
				}

				// if the sum of the values across the entire 11x11 window is above a certain
				// threshold value then draw a rectangle at that location and store the
				// x and y coordinates of the center of the window
				if(sum>1500)
				{
					CvPoint point0 = cvPoint(x-5,y-5);
					CvPoint point1 = cvPoint(x+5,y+5);
					cvRectangle(frame,point0,point1,CV_RGB(255,0,0),2);
					firstX[firstCounter] = x;
					firstY[firstCounter] = y;
					firstCounter++;
				}
			}
		}

	// loop to continually perform thresholding, tracking, event recognition, and sound playback
	while(1)
	{
	
		// Capture the new current frame
		frame = cvQueryFrame(capture);
	
		/* Tracking Red Object
		   - If this works then can tape tip of drumsticks and shoes red to track them
		   - Needs to be fast and reliable
		 */

		// Convert Image to HSV colorspace
		cvCvtColor(frame, hsvFrame, CV_BGR2HSV);

		// Threshold in HSV colorspace
		// 0 < H < 5
		// 200 < S < 255
		// 80 < V < 120
		cvInRangeS(hsvFrame, cvScalar(0, 130, 20), cvScalar(7, 255, 140), threshFrame);


		cvShowImage("Thresholded HSV Image", threshFrame);

		// Process thresholded image block by block and see where the areas of greatest intensity are
		// At those locations right now just draw boxes over the feed image and display that

		int sum = 0;
		secondCounter = 0;

		for(int k=0;k<100;k++)
		{
			secondX[k] = 0;
			secondY[k] = 0;
		}

		// Go through with a 11x11 block across the 640x480 image
		for(int y=6;y<(threshFrame->height-10);y=y+10)
		{
			// Get pointers to desired rows
			uchar* pointer1 = (uchar*) (threshFrame->imageData + (y-5) * threshFrame->widthStep);
			uchar* pointer2 = (uchar*) (threshFrame->imageData + (y-4) * threshFrame->widthStep);
			uchar* pointer3 = (uchar*) (threshFrame->imageData + (y-3) * threshFrame->widthStep);
			uchar* pointer4 = (uchar*) (threshFrame->imageData + (y-2) * threshFrame->widthStep);
			uchar* pointer5 = (uchar*) (threshFrame->imageData + (y-1) * threshFrame->widthStep);
			uchar* pointer6 = (uchar*) (threshFrame->imageData + (y) * threshFrame->widthStep);
			uchar* pointer7 = (uchar*) (threshFrame->imageData + (y+1) * threshFrame->widthStep);
			uchar* pointer8 = (uchar*) (threshFrame->imageData + (y+2) * threshFrame->widthStep);
			uchar* pointer9 = (uchar*) (threshFrame->imageData + (y+3) * threshFrame->widthStep);
			uchar* pointer10 = (uchar*) (threshFrame->imageData + (y+4) * threshFrame->widthStep);
			uchar* pointer11 = (uchar*) (threshFrame->imageData + (y+5) * threshFrame->widthStep);

			// Go through the current row of the image by intervals of 10
			for(int x=6;x<(threshFrame->width-10);x=x+10)
			{
				// reset sum
				sum = 0;

				// add up pixel values in 11x11 box
				for(int i = 0; i<6; i++)
				{
					if(i==0)
					{
						sum = sum + pointer1[x] + pointer2[x] + pointer3[x] + pointer4[x] + pointer5[x] + pointer6[x]
						+ pointer7[x] + pointer8[x] + pointer9[x] + pointer10[x] + pointer11[x];
					}
					else
					{
						sum = sum + pointer1[x-i] + pointer1[x+i] + pointer2[x-i] + pointer2[x+i]
					    + pointer3[x-i] + pointer3[x+i] + pointer4[x-i] + pointer4[x+i]
						+ pointer5[x-i] + pointer5[x+i] + pointer6[x-i] + pointer6[x+i]
						+ pointer7[x-i] + pointer7[x+i] + pointer8[x-i] + pointer8[x+i]
						+ pointer9[x-i] + pointer9[x+i] + pointer10[x-1] + pointer10[x+i]
						+ pointer11[x-i] + pointer11[x+i];
					}
				}

				// if the sum of the values across the entire 11x11 window is above a certain
				// threshold value then draw a rectangle at that location and store the
				// x and y coordinates of the center of the window
				if(sum>1500)
				{
					CvPoint point0 = cvPoint(x-5,y-5);
					CvPoint point1 = cvPoint(x+5,y+5);
					cvRectangle(frame,point0,point1,CV_RGB(255,0,0),2);
					secondX[secondCounter] = x;
					secondY[secondCounter] = y;
					secondCounter++;
				}	
			}
		}

		// check to see if any points were inside hit box in previous frame
		for(int k=0;k<firstCounter;k++)
		{
			// check snare
			if(firstX[k] < 360 && firstX[k] > 280 && firstY[k] < 300 && firstY[k] > 240)
			{
				snareFlag = 1;
				break;
			}
			else
			{
				snareFlag = 0;
			}
		}

		for(int k=0;k<firstCounter;k++)
		{	
			// check hi-hat
			if(firstX[k] < 460 && firstX[k] > 370 && firstY[k] < 330 && firstY[k] > 290)
			{
				hihatFlag = 1;
				break;
			}
			else
			{
				hihatFlag = 0;
			}
		}

		for(int k=0;k<firstCounter;k++)
		{
			// check kick
			if(firstX[k] < 280 && firstX[k] > 200 && firstY[k] < 479 && firstY[k] > 460)
			{
				kickFlag = 1;
				break;
			}
			else
			{
				kickFlag = 0;
			}
		}

		// If no points were in the box in the previous frame then check to see if any points were in
		// the box this frame and if so trigger an event

		for(int l=0;l<secondCounter;l++)
		{
			if(secondX[l] < 360 && secondX[l] > 280 && secondY[l] < 340 && secondY[l] > 300 && snareFlag == 0)
			{
				playSnare = 1;
			}
			else if(secondX[l] < 460 && secondX[l] > 370 && secondY[l] < 330 && secondY[l] > 290 && hihatFlag == 0)
			{
				playHihat = 1;
			}
			else if(secondX[l] < 280 && secondX[l] > 200 && secondY[l] < 479 && secondY[l] > 460 && kickFlag == 0)
			{
				playKick = 1;
			}
		}

		// Check playing flags to see which sounds to play
		
		// Kick, Snare, and Hi-hat
		if(playKick == 1 && playSnare == 1 && playHihat == 1)
		{
			printf("Kick		Snare		Hi-hat\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\kicksnarehihat.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}
		// Kick and Snare
		else if(playKick == 1 && playSnare == 1)
		{
			printf("Kick		Snare\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\kicksnare.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}
		// Kick and Hi-hat
		else if(playKick == 1 && playHihat == 1)
		{
			printf("Kick					Hi-hat\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\kickhihat.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}
		// Snare and Hi-hat
		else if(playSnare == 1 && playHihat == 1)
		{
			printf("			Snare		Hi-hat\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\snarehihat.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}
		// Snare only
		else if(playSnare == 1)
		{
			printf("			Snare\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\snareshort.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}
		// Hi-hat only
		else if(playHihat == 1)
		{
			printf("						Hi-hat\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\hihatshort.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}
		// Kick only
		else if(playKick == 1)
		{
			printf("Kick\n");
			PlaySound(_T("C:\\Users\\Kalev\\Desktop\\Drums\\kickshort.wav"), 0, SND_FILENAME|SND_ASYNC|SND_NOSTOP);
		}

		// Draw hit boxes onto current frame
		// Snare
		cvRectangle(frame, cvPoint(280,300), cvPoint(360,340), CV_RGB(255,0,0), 1);
		// Hi-hat
		cvRectangle(frame, cvPoint(370,290), cvPoint(460,330), CV_RGB(255,0,0), 1);
		// Kick
		cvRectangle(frame, cvPoint(200,460), cvPoint(280,479), CV_RGB(255,0,0), 1);

		cvShowImage("Feed", frame);

		// copy the last frame into the prev_frame image
		cvCopy(frame,prev_frame,0);

		// copy set of points from second image in sequence to be used as set of points from
		// first image in sequence next time
		for(int k=0;k<secondCounter;k++)
		{
			firstX[k] = secondX[k];
			firstY[k] = secondY[k];
		}

		firstCounter = secondCounter;

		// Reset playing flags
		playSnare = 0;
		playHihat = 0;
		playKick = 0;
		
		if(cvWaitKey(5) == 27)
			break;
	}

	cvReleaseCapture(&capture);

	cvReleaseImage(&red);
	cvReleaseImage(&green);
	cvReleaseImage(&blue);
	cvReleaseImage(&hsvFrame);
	cvReleaseImage(&threshFrame);

	cvDestroyWindow("Feed");

	delete [] firstX;
	delete [] firstY;
	delete [] secondX;
	delete [] secondY;

	return 0;
}
