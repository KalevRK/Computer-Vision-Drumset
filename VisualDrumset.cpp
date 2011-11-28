// VisualDrumset.cpp : Defines the entry point for the console application.
//

/*
	Senior Design Project - Visual Drumset with Stereovision using OpenCV

	Kalev Roomann-Kurrik
	Spring 2011

	Last Updated: Wednesday 4/26/2011

	- System uses two Playstation Eye cameras to obtain two live feeds of the user.
	- The two cameras undergo stereo calibration.
	- After calibration marked drumsticks are tracked in the left camera feed.
	- The x and y image coordinates of the marked drumsticks along with their corresponding 
	  disparities from the stereo calibrated system are used to find the real world 3D
	  position of the drumsticks with respect to the camera setup.
    - The 3D position of the drumsticks is then used for gesture recognition which generates
	  events with sound feedback to emulate the action of an acoustic drumset.

*/

#include "stdafx.h"
#include "cxmisc.h"
#include "cvaux.h"
#include <cv.h>
#include <cxcore.h>
#include <highgui.h>
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>

using namespace std;

// Sample camera capture class
class CLEyeCameraCapture
{
	CHAR _windowName[256];
	GUID _cameraGUID;
	CLEyeCameraInstance _cam;
	CLEyeCameraColorMode _mode;
	CLEyeCameraResolution _resolution;
	float _fps;
	HANDLE _hThread;
	bool _running;
	IplImage *pCapImage;

public:
	CLEyeCameraCapture(LPSTR windowName, GUID cameraGUID, CLEyeCameraColorMode mode, CLEyeCameraResolution resolution, float fps) :
		_cameraGUID(cameraGUID), _cam(NULL), _mode(mode), _resolution(resolution), _fps(fps), _running(false)
	{
		strcpy(_windowName, windowName);
	}

	bool StartCapture()
	{
		_running = true;
		cvNamedWindow(_windowName, CV_WINDOW_AUTOSIZE);
		// Start CLEye image capture thread
		_hThread = CreateThread(NULL, 0, &CLEyeCameraCapture::CaptureThread, this, 0, 0);
		if(_hThread == NULL)
		{
			//MessageBox(NULL,"Could not create capture thread","CLEyeMulticamTest", MB_ICONEXCLAMATION);
			return false;
		}
		return true;
	}
	void StopCapture()
	{
		if(!_running)	return;
		_running = false;
		WaitForSingleObject(_hThread, 1000);
		cvDestroyWindow(_windowName);
	}

	

	// New function written by Kalev to return IplImage captured by camera from thread to main function
	IplImage * getImage()
	{
		return pCapImage;
	}

	bool isRunning()
	{
		if(_running)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	void Run()
	{
		int w, h;
		//IplImage *pCapImage;
		PBYTE pCapBuffer = NULL;
		// Create camera instance
		_cam = CLEyeCreateCamera(_cameraGUID, _mode, _resolution, _fps);
		if(_cam == NULL)		return;
		// Get camera frame dimensions
		CLEyeCameraGetFrameDimensions(_cam, w, h);
		// Depending on color mode chosen, create the appropriate OpenCV image
		if(_mode == CLEYE_COLOR_PROCESSED || _mode == CLEYE_COLOR_RAW)
			pCapImage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 4);
		else
			pCapImage = cvCreateImage(cvSize(w, h), IPL_DEPTH_8U, 1);

		// Set some camera parameters
		CLEyeSetCameraParameter(_cam, CLEYE_GAIN, 0);
		CLEyeSetCameraParameter(_cam, CLEYE_EXPOSURE, 511);
 		CLEyeSetCameraParameter(_cam, CLEYE_ZOOM, (int)(1.0));
 		CLEyeSetCameraParameter(_cam, CLEYE_ROTATION, (int)(0.0));

		// Start capturing
		CLEyeCameraStart(_cam);
		cvGetImageRawData(pCapImage, &pCapBuffer);
		// image capturing loop
		while(_running)
		{
			CLEyeCameraGetFrame(_cam, pCapBuffer);

			//cvShowImage(_windowName, pCapImage);
		}
		// Stop camera capture
		CLEyeCameraStop(_cam);
		// Destroy camera object
		CLEyeDestroyCamera(_cam);
		// Destroy the allocated OpenCV image
		cvReleaseImage(&pCapImage);
		_cam = NULL;
	}
	static DWORD WINAPI CaptureThread(LPVOID instance)
	{
		// seed the rng with current tick count and thread id
		srand(GetTickCount() + GetCurrentThreadId());
		// forward thread to Capture function
		CLEyeCameraCapture *pThis = (CLEyeCameraCapture *)instance;
		pThis->Run();
		return 0;
	}
};


int _tmain(int argc, _TCHAR* argv[])
{

	// Ask user whether they want normal mode or debug mode
	// Normal mode is continuous frame capture after calibration
	// Debug mode is waiting after each frame for user input in order to get
	// the next frame so that intermediate values can be seen
	int DEBUG;
	printf("Select operating mode (0 = Normal 1 = Debug): ");
	cin >> DEBUG;
	printf("\n");
	if(DEBUG == 0)
	{
		printf("You have selected Normal mode\n");
	}
	else
	{
		printf("You have selected Debug mode\n");
	}

	CLEyeCameraCapture *cam[2] = { NULL };
	srand(GetTickCount());
	// Query for number of connected cameras
	int numCams = CLEyeGetCameraCount();
	if(numCams == 0)
	{
		printf("No PS3Eye cameras detected\n");
		return -1;
	}
	printf("Found %d cameras\n", numCams);
	
	
	
	for(int i = 0; i < numCams; i++)
	{
		char windowName[64];
		// Query unique camera uuid
		GUID guid = CLEyeGetCameraUUID(i);
		printf("Camera %d GUID: [%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x]\n", 
						i+1, guid.Data1, guid.Data2, guid.Data3,
						guid.Data4[0], guid.Data4[1], guid.Data4[2],
						guid.Data4[3], guid.Data4[4], guid.Data4[5],
						guid.Data4[6], guid.Data4[7]);
		sprintf(windowName, "Camera Window %d", i+1);
		// Create camera capture object
		// Randomize resolution and color mode
		cam[i] = new CLEyeCameraCapture(windowName, guid, CLEYE_COLOR_PROCESSED, CLEYE_QVGA, 30);	

		printf("Starting capture on camera %d\n", i+1);
		cam[i]->StartCapture();
	}

	printf("outside of camera setup for loop\n");

	// Images to hold camera feeds
	IplImage *feed0;
	IplImage *feed1;

	
	// The <ESC> key will exit the program
	CLEyeCameraCapture *pCam = NULL;
	int param = -1, key;
	printf("Press Esc to start capturing images\n");
	
	while((key = cvWaitKey(0)) != 0x1b)
	{
	}

	int frameCounter = 0;

	// Arrays of captured images for calibration
	//IplImage *leftCamCaptures[2] = { NULL };
	//IplImage *rightCamCaptures[2] = { NULL };

	printf("Capture 15 images of chessboard pattern\n");

	// Allocate matrices and variables for points and parameters
	int displayCorners = 1;
	int showUndistorted = 1;
	bool isVerticalStereo = false;
	const int maxScale = 1;
	const float squareSize = 1.f; // Currently 1 square = 1 unit (also equal to 1 inch)
	int nx = 8;
	int ny = 6;
	int i,j, nframes, n=nx*ny, N = 0;
	int result_left = 0;
	int result_right = 0;
	int count = 0;
	int s = 1;

	vector<CvPoint3D32f> objectPoints;
	vector<CvPoint2D32f> points[2];
	vector<CvPoint2D32f> temp(n);
	vector<int> npoints;
	vector<uchar> active[2];

	// Arrays and vectors
	double M1[3][3], M2[3][3], D1[5], D2[5];
	double R[3][3], T[3], E[3][3], F[3][3];
	CvMat _M1 = cvMat(3, 3, CV_64F, M1);
	CvMat _M2 = cvMat(3, 3, CV_64F, M2);
	CvMat _D1 = cvMat(1, 5, CV_64F, D1);
	CvMat _D2 = cvMat(1, 5, CV_64F, D2);
	CvMat _R = cvMat(3, 3, CV_64F, R);
	CvMat _T = cvMat(3, 1, CV_64F, T);
	CvMat _E = cvMat(3, 3, CV_64F, E);
	CvMat _F = cvMat(3, 3, CV_64F, F);
	
	CvSize imageSize = {0,0};

	cvNamedWindow("corners", 1);

	// Image holders for all 20 calibration images

	IplImage* left1;
	IplImage* left2;
	IplImage* left3;
	IplImage* left4;
	IplImage* left5;
	IplImage* left6;
	IplImage* left7;
	IplImage* left8;
	IplImage* left9;
	IplImage* left10;
	IplImage* left11;
	IplImage* left12;
	IplImage* left13;
	IplImage* left14;
	IplImage* left15;

	IplImage* right1;
	IplImage* right2;
	IplImage* right3;
	IplImage* right4;
	IplImage* right5;
	IplImage* right6;
	IplImage* right7;
	IplImage* right8;
	IplImage* right9;
	IplImage* right10;
	IplImage* right11;
	IplImage* right12;
	IplImage* right13;
	IplImage* right14;
	IplImage* right15;

	feed0 = cam[0]->getImage();

	// Setup of variables for object tracking and gesture recognition
	IplImage* prev_frame = cvCreateImage(cvGetSize(feed0), 8, 3);
	IplImage* grayFrame = cvCreateImage(cvGetSize(feed0), IPL_DEPTH_8U, 1);
	IplImage* grayPrevFrame = cvCreateImage(cvGetSize(feed0), IPL_DEPTH_8U, 1);

	// Component images
	IplImage* red = cvCreateImage(cvGetSize(feed0), 8, 1);
	IplImage* green = cvCreateImage(cvGetSize(feed0), 8, 1);
	IplImage* blue = cvCreateImage(cvGetSize(feed0), 8, 1);

	// HSV image
	IplImage* hsvFrame = cvCreateImage(cvGetSize(feed0), 8, 3);

	// Threshold image
	IplImage* threshFrame = cvCreateImage(cvGetSize(feed0), 8, 1);


	// First capture 15 sets of images for calibration
	while(frameCounter < 15)
	{

		// Take pictures of chessboard pattern for calibration
		// Space bar takes a new image
		if(cam[0]->isRunning() == true && cam[1]->isRunning() == true)
		{
			feed0 = cam[0]->getImage();
			feed1 = cam[1]->getImage();
			cvShowImage("Camera Window 1", feed0);
			cvShowImage("Camera Window 2", feed1);

			if((key = cvWaitKey(5)) == 0x20)
			{

			// Get Chessboard points from two images
			printf("Performing stereo calibration\n");

			printf("Working on left cam image %d\n",frameCounter+1);

			IplImage* timg_left = cvCreateImage(cvGetSize(feed0),8,4);
			cvCopyImage(feed0,timg_left);

			IplImage* timg_right = cvCreateImage(cvGetSize(feed1),8,4);
			cvCopyImage(feed1,timg_right);

			switch(frameCounter)
			{
				case 0:
					left1 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left1);
					right1 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right1);
					break;
				case 1:
					left2 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left2);
					right2 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right2);
					break;
				case 2:
					left3 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left3);
					right3 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right3);
					break;
				case 3:
					left4 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left4);
					right4 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right4);
					break;
				case 4:
					left5 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left5);
					right5 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right5);
					break;
				case 5:
					left6 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left6);
					right6 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right6);
					break;
				case 6:
					left7 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left7);
					right7 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right7);
					break;
				case 7:
					left8 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left8);
					right8 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right8);
					break;
				case 8:
					left9 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left9);
					right9 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right9);
					break;
				case 9:
					left10 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left10);
					right10 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right10);
					break;
				case 10:
					left11 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left11);
					right11 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right11);
					break;
				case 11:
					left12 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left12);
					right12 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right12);
					break;
				case 12:
					left13 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left13);
					right13 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right13);
					break;
				case 13:
					left14 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left14);
					right14 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right14);
					break;
				case 14:
					left15 = cvCreateImage(cvGetSize(feed0),8,4);
					cvCopyImage(feed0,left15);
					right15 = cvCreateImage(cvGetSize(feed1),8,4);
					cvCopyImage(feed1,right15);
					break;
				default:
					break;
			}

			printf("temp image created\n");

			imageSize = cvGetSize(feed0);

			vector<CvPoint2D32f>& pts = points[0];

			printf("Finding chessboard corners\n");

			result_right = cvFindChessboardCorners(timg_right,cvSize(nx,ny),&temp[0],&count,CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
			result_left = cvFindChessboardCorners(timg_left,cvSize(nx,ny),&temp[0],&count,CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
		

			printf("result_left = %d\n", result_left);
			printf("result_right = %d\n", result_right);
			

			// if not all of the corners were notify the user to try again
			if((result_right == 0) || (result_left == 0))
			{
				printf("Not all of the corners were found for those two views\n");
				printf("Please try again\n");
			}

			// if all the corners were found in both images then proceed
			if((result_right != 0) && (result_left != 0))
			{

			printf("Found chessboard corners\n");

			if(timg_left != feed0)
				cvReleaseImage(&timg_left);
			if(result_left || s == maxScale)
			{
				for(j=0; j < count; j++)
				{
					temp[j].x /= s;
					temp[j].y /= s;
				}
			}

			printf("Displaying corners\n");
			if(displayCorners)
			{
				IplImage* cimg = cvCreateImage(cvGetSize(feed0),8,4);
				cvCopyImage(feed0,cimg);
				cvDrawChessboardCorners(cimg, cvSize(nx,ny), &temp[0], count, result_left);
				cvShowImage("corners",cimg);
				cvReleaseImage(&cimg);

				printf("Press space bar to continue\n");
				if(cvWaitKey(0) == 32) // Space bar to continue
				{
				}
			}

			N = pts.size();

			pts.resize(N + n, cvPoint2D32f(0,0));
			active[0].push_back((uchar)result_left);

			// Subpixel accuracy
			if(result_left)
			{
				IplImage *gray_image = cvCreateImage(imageSize,8,1);
				cvCvtColor(feed0, gray_image, CV_BGR2GRAY);
				cvFindCornerSubPix(gray_image, &temp[0], count, cvSize(11,11), cvSize(-1,-1),cvTermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS,30,0.1));
				copy(temp.begin(),temp.end(), pts.begin() + N);
			}

			printf("Done with left cam image %d\n",frameCounter);

			printf("Working on right cam image %d\n",frameCounter);

			cvCopyImage(feed1,timg_right);

			printf("Copied feed1 to timg_right\n");

			imageSize = cvGetSize(feed1);

			printf("set imageSize\n");

			vector <CvPoint2D32f>& pts2 = points[1];

			printf("set pts\n");

			result_right = cvFindChessboardCorners(timg_right,cvSize(nx,ny),&temp[0],&count,CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
			
			printf("Called cvFindChessboardCorners\n");
			
			if(timg_right != feed1)
				cvReleaseImage(&timg_right);
			if(result_right || s == maxScale)
			{
				for(j=0; j < count; j++)
				{
					temp[j].x /= s;
					temp[j].y /= s;
				}
			}

			if(displayCorners)
			{
				IplImage *cimg = cvCreateImage(imageSize,8,4);
				cvCopyImage(feed1,cimg);
				cvDrawChessboardCorners(cimg, cvSize(nx,ny), &temp[0], count, result_right);
				cvShowImage("corners",cimg);
				cvReleaseImage(&cimg);
			
				printf("Press space bar to continue\n");
				if(cvWaitKey(0) == 32) // Space bar to continue
				{
				}
			}

			N = pts2.size();

			pts2.resize(N + n, cvPoint2D32f(0,0));
			active[1].push_back((uchar)result_right);

			// Subpixel accuracy
			if(result_right)
			{
				IplImage *gray_image = cvCreateImage(imageSize,8,1);
				cvCvtColor(feed1,gray_image,CV_BGR2GRAY);
				printf("Performing subpixel accuracy\n");
				cvFindCornerSubPix(gray_image, &temp[0], count, cvSize(11,11), cvSize(-1,-1),cvTermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS,30,0.1));
				copy(temp.begin(),temp.end(), pts2.begin() + N);
			}

			printf("Done with right cam image %d\n",frameCounter);

			// Increment frame counter
			frameCounter++;

			}
			
			}
		}
	}

	// Get number of good chessboard patterns found
	nframes = active[0].size();
	printf("Number of good left patterns found = %d\n",nframes);

	nframes = active[1].size();
	printf("Number of good right patterns found = %d\n",nframes);

	// Get Chessboard 3D Object point list
	objectPoints.resize(nframes*n);
	for(i=0; i<ny; i++)
	{
		for(j=0; j<nx; j++)
		{
			objectPoints[i*nx + j] = cvPoint3D32f(i*squareSize, j*squareSize, 0);
		}
	}

	printf("Setup object points\n");

	for(i=1; i <nframes; i++)
	{
		copy(objectPoints.begin(), objectPoints.begin() + n, objectPoints.begin() + i*n);
	}

	printf("Copied object points\n");

	npoints.resize(nframes,n);
	N = nframes * n;

	CvMat _objectPoints = cvMat(1, N, CV_32FC3, &objectPoints[0]);
	CvMat _imagePoints1 = cvMat(1, N, CV_32FC2, &points[0][0]);
	CvMat _imagePoints2 = cvMat(1, N, CV_32FC2, &points[1][0]);
	CvMat _npoints = cvMat(1, npoints.size(), CV_32S, &npoints[0]);

	cvSetIdentity(&_M1);
	cvSetIdentity(&_M2);
	cvZero(&_D1);
	cvZero(&_D2);

	// Perform Stereo Calibration
	printf("Performing actual calibration\n");
	fflush(stdout);

	cvStereoCalibrate(&_objectPoints, &_imagePoints1, &_imagePoints2, &_npoints, &_M1, &_D1, &_M2, &_D2, imageSize, &_R, &_T, &_E, &_F, cvTermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 100, 1e-5),CV_CALIB_FIX_ASPECT_RATIO + CV_CALIB_ZERO_TANGENT_DIST + CV_CALIB_SAME_FOCAL_LENGTH);
	printf("done\n");

	// Calibration check
	vector<CvPoint3D32f> lines[2];
	points[0].resize(N);
	points[1].resize(N);
	_imagePoints1 = cvMat(1, N, CV_32FC2, &points[0][0]);
	_imagePoints2 = cvMat(1, N, CV_32FC2, &points[1][0]);
	lines[0].resize(N);
	lines[1].resize(N);
	CvMat _L1 = cvMat(1, N, CV_32FC3, &lines[0][0]);
	CvMat _L2 = cvMat(1, N, CV_32FC3, &lines[1][0]);

	cvUndistortPoints(&_imagePoints1, &_imagePoints1, &_M1, &_D1, 0, &_M1);
	cvUndistortPoints(&_imagePoints2, &_imagePoints2, &_M2, &_D2, 0, &_M2);
	cvComputeCorrespondEpilines(&_imagePoints1, 1, &_F, &_L1);
	cvComputeCorrespondEpilines(&_imagePoints2, 2, &_F, &_L2);

	double avgError = 0;

	for(i=0; i<N; i++)
	{
		double error = fabs(points[0][i].x*lines[1][i].x + points[0][i].y*lines[1][i].y + lines[1][i].z) + fabs(points[1][i].x*lines[0][i].x + points[1][i].y*lines[0][i].y + lines[0][i].z);
		avgError += error;
	}

	printf("The average error = %g\n", avgError/(nframes*n));


	// Rectification
	CvMat* mx1 = cvCreateMat(imageSize.height, imageSize.width, CV_32F);
	CvMat* my1 = cvCreateMat(imageSize.height, imageSize.width, CV_32F);
	CvMat* mx2 = cvCreateMat(imageSize.height, imageSize.width, CV_32F);
	CvMat* my2 = cvCreateMat(imageSize.height, imageSize.width, CV_32F);

	CvMat* img1r = cvCreateMat(imageSize.height, imageSize.width, CV_8U);
	CvMat* img2r = cvCreateMat(imageSize.height, imageSize.width, CV_8U);
	CvMat* disp = cvCreateMat(imageSize.height, imageSize.width, CV_16S);
	CvMat* vdisp = cvCreateMat(imageSize.height, imageSize.width, CV_8U); 

	CvMat* pair;
	double R1[3][3], R2[3][3], P1[3][4], P2[3][4], Q[4][4];
	CvMat _R1 = cvMat(3, 3, CV_64F, R1);
	CvMat _R2 = cvMat(3, 3, CV_64F, R2);
	CvMat _Q = cvMat(4,4,CV_64F, Q);

	printf("Created matrices for rectification\n");

	// Bouguet's Method (using calibrated stereo system)
	CvMat _P1 = cvMat(3, 4, CV_64F, P1);
	CvMat _P2 = cvMat(3, 4, CV_64F, P2);
	cvStereoRectify(&_M1, &_M2, &_D1, &_D2, imageSize, &_R, &_T, &_R1, &_R2, &_P1, &_P2, &_Q, 0 /*CV_CALIB_ZERO_DISPARITY*/);
	isVerticalStereo = fabs(P2[1][3]) > fabs(P2[0][3]);

	printf("Called cvStereoRectify\n");

	// maps for cvRemap()
	cvInitUndistortRectifyMap(&_M1,&_D1,&_R1,&_P1,mx1,my1);
	cvInitUndistortRectifyMap(&_M2,&_D2,&_R2,&_P2,mx2,my2);

	printf("Called cvInitUndistortRectifyMap\n");

	cvNamedWindow("rectified", 1);

	// Rectify images and find disparity map
	pair = cvCreateMat(imageSize.height,imageSize.width*2,CV_8UC3);

	// Setup for finding stereo correspondences
	CvStereoBMState *BMState = cvCreateStereoBMState();
	assert(BMState != 0);
	BMState->preFilterSize=41;
	BMState->preFilterCap=31;
	BMState->SADWindowSize=41;
	BMState->minDisparity=-64;
	BMState->numberOfDisparities=128;
	BMState->textureThreshold=10;
	BMState->uniquenessRatio=15;

	IplImage* img1 = cvCreateImage(cvGetSize(left1),8,1);
	IplImage* img2 = cvCreateImage(cvGetSize(right1),8,1);

	printf("Performed setup for correspondences\n");

	for(i=0; i < nframes; i++)
	{
		printf("Going through disparity map loop iteration %d\n",i);

		switch(i)
		{
			case 0:
				cvCvtColor(left1, img1, CV_BGR2GRAY);
				cvCvtColor(right1, img2, CV_BGR2GRAY);
				break;
			case 1:
				cvCvtColor(left2, img1, CV_BGR2GRAY);
				cvCvtColor(right2, img2, CV_BGR2GRAY);
				break;
			case 2:
				cvCvtColor(left3, img1, CV_BGR2GRAY);
				cvCvtColor(right3, img2, CV_BGR2GRAY);
				break;
			case 3:
				cvCvtColor(left4, img1, CV_BGR2GRAY);
				cvCvtColor(right4, img2, CV_BGR2GRAY);
				break;
			case 4:
				cvCvtColor(left5, img1, CV_BGR2GRAY);
				cvCvtColor(right5, img2, CV_BGR2GRAY);
				break;
			case 5:
				cvCvtColor(left6, img1, CV_BGR2GRAY);
				cvCvtColor(right6, img2, CV_BGR2GRAY);
				break;
			case 6:
				cvCvtColor(left7, img1, CV_BGR2GRAY);
				cvCvtColor(right7, img2, CV_BGR2GRAY);
				break;
			case 7:
				cvCvtColor(left8, img1, CV_BGR2GRAY);
				cvCvtColor(right8, img2, CV_BGR2GRAY);
				break;
			case 8:
				cvCvtColor(left9, img1, CV_BGR2GRAY);
				cvCvtColor(right9, img2, CV_BGR2GRAY);
				break;
			case 9:
				cvCvtColor(left10, img1, CV_BGR2GRAY);
				cvCvtColor(right10, img2, CV_BGR2GRAY);
				break;
			case 10:
				cvCvtColor(left11, img1, CV_BGR2GRAY);
				cvCvtColor(right11, img2, CV_BGR2GRAY);
				break;
			case 11:
				cvCvtColor(left12, img1, CV_BGR2GRAY);
				cvCvtColor(right12, img2, CV_BGR2GRAY);
				break;
			case 12:
				cvCvtColor(left13, img1, CV_BGR2GRAY);
				cvCvtColor(right13, img2, CV_BGR2GRAY);
				break;
			case 13:
				cvCvtColor(left14, img1, CV_BGR2GRAY);
				cvCvtColor(right14, img2, CV_BGR2GRAY);
				break;
			case 14:
				cvCvtColor(left15, img1, CV_BGR2GRAY);
				cvCvtColor(right15, img2, CV_BGR2GRAY);
				break;
			default:
				break;
		}

		
		printf("Selected images based on value of i\n");

		if(img1 && img2)
		{
			CvMat part;
			cvRemap(img1, img1r, mx1, my1);
			cvRemap(img2, img2r, mx2, my2);

			printf("Called cvRemap\n");

			if(!isVerticalStereo)
			{
				cvFindStereoCorrespondenceBM(img1r, img2r, disp, BMState);
				cvNormalize(disp, vdisp, 0, 256, CV_MINMAX);
				cvNamedWindow("disparity");
				cvShowImage("disparity", vdisp);

				cvGetCols(pair, &part, 0, imageSize.width);
				cvCvtColor(img1r, &part, CV_GRAY2BGR);
				cvGetCols(pair, &part, imageSize.width,imageSize.width*2);
				cvCvtColor(img2r, &part, CV_GRAY2BGR);
				
				for(j=0; j<imageSize.height; j +=16)
				{
					cvLine(pair, cvPoint(0,j), cvPoint(imageSize.width*2,j),CV_RGB(0,255,0));
				}

			}

			cvShowImage("rectified", pair);
			if(cvWaitKey() == 27)
				break;

		}

	}

	printf("\nEsc exits\n");

	int stop = 0;

	// Setup object tracking and gesture recognition by running through algorithm once
	// on last set of images from both cameras
	// Matrices to store x,y locations of tracked features for two images in sequence
	// (both left and right camera images)
	int *firstXLeft = new int[100];
	int *firstYLeft = new int[100];
	int *firstXRight = new int[100];
	int *firstYRight = new int[100];

	int *secondXLeft = new int[100];
	int *secondYLeft = new int[100];
	int *secondXRight = new int[100];
	int *secondYRight = new int[100];

	for(int k=0;k<100;k++)
	{
		firstXLeft[k] = 0;
		firstYLeft[k] = 0;
		firstXRight[k] = 0;
		firstYRight[k] = 0;

		secondXLeft[k] = 0;
		secondYLeft[k] = 0;
		secondXRight[k] = 0;
		secondYRight[k] = 0;
	}

	int firstCounterLeft = 0;
	int firstCounterRight = 0;
	int secondCounterLeft = 0;
	int secondCounterRight = 0;

	// Convert last capture from left camera to HSV colorspace
	cvCvtColor(left15, hsvFrame, CV_BGR2HSV);

	// Threshold in HSV colorspace
	cvInRangeS(hsvFrame, cvScalar(0, 110, 30), cvScalar(9, 255, 130), threshFrame);
	cvShowImage("Thresholded HSV Image Left", threshFrame);

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
				cvRectangle(left15,point0,point1,CV_RGB(255,0,0),2);
				firstXLeft[firstCounterLeft] = x;
				firstYLeft[firstCounterLeft] = y;
				firstCounterLeft++;
			}
		}
	}

	// show image with tracked points in left camera feed window
	cvShowImage("Camera Window 1", left15);


	// repeat with last frame from right camera

	// Convert last capture from left camera to HSV colorspace
	cvCvtColor(right15, hsvFrame, CV_BGR2HSV);

	// Threshold in HSV colorspace
	cvInRangeS(hsvFrame, cvScalar(0, 110, 30), cvScalar(9, 255, 130), threshFrame);
	cvShowImage("Thresholded HSV Image Right", threshFrame);

	// Process thresholded image block by block and see where the areas of greatest intensity are
	// At those locations right now just draw boxes over the feed image and display that

	sum = 0;

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
				cvRectangle(right15,point0,point1,CV_RGB(255,0,0),2);
				firstXRight[firstCounterRight] = x;
				firstYRight[firstCounterRight] = y;
				firstCounterRight++;
			}
		}
	}

	// show image with tracked points in left camera feed window
	cvShowImage("Camera Window 2", right15);


	// Hit detection flag used to see if a tracked point was inside of the
	// hit cube in the previous frame
	// If a tracked point was inside of the hit cube in the previous frame
	// then no sound event is triggered
	int playSound = 0;

	int oneIn = 0;



	// Loop to capture images, to peform object tracking and gesture 
	// recognition, and to do event handling based on 3D positional data
	while(stop == 0)
	{
		// exit on Esc key
		if((key = cvWaitKey(5)) == 0x1b)
			stop = 1;

		// update camera feeds
		feed0 = cam[0]->getImage();
		feed1 = cam[1]->getImage();

		cvCvtColor(feed0,img1,CV_BGR2GRAY);
		cvCvtColor(feed1,img2,CV_BGR2GRAY);
				
		CvMat part;
		cvRemap(img1, img1r, mx1, my1);
		cvRemap(img2, img2r, mx2, my2);

		// calculate the disparity map for the current frame
		cvFindStereoCorrespondenceBM(img1r, img2r, disp, BMState);
		cvNormalize(disp, vdisp, 0, 256, CV_MINMAX);
		
		if(DEBUG)
		{
			cvNamedWindow("disparity");
			cvShowImage("disparity", vdisp);
		}

		cvGetCols(pair, &part, 0, imageSize.width);
		cvCvtColor(img1r, &part, CV_GRAY2BGR);
		cvGetCols(pair, &part, imageSize.width,imageSize.width*2);
		cvCvtColor(img2r, &part, CV_GRAY2BGR);
				
		for(j=0; j<imageSize.height; j +=16)
		{
			cvLine(pair, cvPoint(0,j), cvPoint(imageSize.width*2,j),CV_RGB(0,255,0));
		}

		if(DEBUG)
		{
			cvShowImage("rectified", pair);
		}

		CvPoint rectTL = cvPoint(200,280);
		CvPoint rectBR = cvPoint(440,460);
		

		// Step 1: threshold the left camera image in order to isolate red objects

		// Convert Image to HSV colorspace
		cvCvtColor(feed0, hsvFrame, CV_BGR2HSV);

		// Threshold in HSV colorspace
		cvInRangeS(hsvFrame, cvScalar(0, 110, 30), cvScalar(9, 255, 130), threshFrame);

		if(DEBUG)
		{
			cvShowImage("Thresholded HSV Image Left", threshFrame);
		}

		// Step 2: Locate 2D pixel coordinates of any tracked features and draw boxes
		// over tracked features in the left camera feed
		// Process thresholded image block by block and see where the areas of greatest intensity are
		// At those locations right now just draw boxes over the feed image and display that

		sum = 0;
		secondCounterLeft = 0;

		for(int k=0;k<100;k++)
		{
			secondXLeft[k] = 0;
			secondYLeft[k] = 0;
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
					cvRectangle(feed0,point0,point1,CV_RGB(255,0,0),2);
					secondXLeft[secondCounterLeft] = x;
					secondYLeft[secondCounterLeft] = y;
					secondCounterLeft++;
				}	
			}
		}


		// repeat for right camera feed

		// Step 1: threshold the right camera image in order to isolate red objects

		// Convert Image to HSV colorspace
		cvCvtColor(feed1, hsvFrame, CV_BGR2HSV);

		// Threshold in HSV colorspace
		cvInRangeS(hsvFrame, cvScalar(0, 110, 30), cvScalar(9, 255, 130), threshFrame);

		if(DEBUG)
		{
			cvShowImage("Thresholded HSV Image Right", threshFrame);
		}

		// Step 2: Locate 2D pixel coordinates of any tracked features and draw boxes
		// over tracked features in the left camera feed
		// Process thresholded image block by block and see where the areas of greatest intensity are
		// At those locations right now just draw boxes over the feed image and display that

		sum = 0;
		secondCounterRight = 0;

		for(int k=0;k<100;k++)
		{
			secondXRight[k] = 0;
			secondYRight[k] = 0;
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
					cvRectangle(feed1,point0,point1,CV_RGB(255,0,0),2);
					secondXRight[secondCounterRight] = x;
					secondYRight[secondCounterRight] = y;
					secondCounterRight++;
				}	
			}
		}

		// Visual Feedback for user
		// Going with a 2D rectange indicating the front side of the 3D hit detection box
		// Was unable to implement OpenGL 3D environment before Final Project deadline, so
		// using visual feedback similar to Interim Project
		// By keeping the sticks approximately 8-12 inches in front of the left camera
		// and moving the sticks into the box and out again a sound can be triggered
		// Draw a red rectangle
		IplImage* testImage = cvCreateImage(cvGetSize(feed0),8,4);
		cvCopyImage(feed0,testImage);
		cvCircle(testImage,cvPoint(320,240),10,CV_RGB(0,0,255),1);
		cvShowImage("Test Image", testImage);
		
		

		// update the display of the feeds to show the boxes over the tracked points
		cvShowImage("Camera Window 1", feed0);
		cvShowImage("Camera Window 2", feed1);

		// display a list of pixel coordinates for each tracked point

		// clear the console
		system("cls");

		if(DEBUG)
		{
			printf("     Left Camera				Right Camera\n");
		}



		
		if(DEBUG)
		{
			for(int i=0; i<secondCounterLeft; i++)
			{
				printf("X[%d] = %d	Y[%d] = %d		X[%d] = %d  Y[%d] = %d\n",i,secondXLeft[i],
					i,secondYLeft[i],i,secondXRight[i],i,secondYRight[i]);	
			}

			printf("\n");
			printf("Undistorted Rectified Pixel Coordinates\n");
		

			int rectX1, rectY1, rectX2, rectY2;

			// Print out undistorted rectified pixel values
			for(int i=0; i<secondCounterLeft; i++)
			{
				rectX1 = cvmGet(mx1,secondYLeft[i],secondXLeft[i]);
				rectY1 = cvmGet(my1,secondYLeft[i],secondXLeft[i]);
				rectX2 = cvmGet(mx2,secondYRight[i],secondXRight[i]);
				rectY2 = cvmGet(my2,secondYRight[i],secondXRight[i]);

				printf("X[%d] = %d  Y[%d] = %d      X[%d] = %d  Y[%d] = %d\n",i,rectX1,
					i,rectY1,i,rectX2,i,rectY2);
			}

			printf("\n");
		}

		// Find corresponding tracked points in left and right images using y-values
		// from feed images from both cameras
		// Start with left[0] and go through all points in right[]
		// if there's a point in right[] with the same y-value as left[0]
		// they are a match
		// Find their disparity
		// Repeat for all other elements of left[]

		int disparity;

		float X, Y, Z, W;

		//CvPoint3D32f twoD;
		//CvPoint3D32f threeD;

		CvMat* twoDmatrix = cvCreateMat(1,3,CV_32F);
		CvMat* threeDmatrix = cvCreateMat(1,3,CV_32F);

		// reset flag for whether any points have been found inside
		// of the drum cube this frame
		oneIn = 0;

		for(int i=0;i<secondCounterLeft;i++)
		{
			for(int j=0;j<secondCounterRight;j++)
			{
				if(secondYLeft[i] == secondYRight[j])
				{
					if(DEBUG)
					{
						printf("Found match between Left camera Y[%d] and Right camera Y[%d]\n",i,j);
					}

					// Compute horizontal disparity between corresponding points
					disparity = secondXLeft[i] - secondXRight[j];

					if(DEBUG)
					{
						printf("disparity calculated, setting matrix elements\n");
					}

					// Compute 3D real world coordinate of tracked point
					// using left image x,y pixel coordinates and disparity between
					// point in left and right images along with Q matrix
					//twoD.x = secondXLeft[i];
					//twoD.y = secondYLeft[i];
					//twoD.z = disparity;

					/*cvmSet(twoDmatrix,0,0,secondXLeft[i]);
					cvmSet(twoDmatrix,0,1,secondXRight[i]);
					cvmSet(twoDmatrix,0,2,disparity);

					printf("Matrix elements set, calling cvPerspectiveTransform\n");

					cvPerspectiveTransform(twoDmatrix,threeDmatrix,&_Q);
					*/
					
					

					X = secondXLeft[i]*Q[0][0] + Q[0][3];
					Y = secondYLeft[i]*Q[1][1] + Q[1][3];
					Z = Q[2][3];
					W = disparity*Q[3][2] + Q[3][3];

					X = X/W;
					Y = Y/W;
					Z = Z/W;

					if(DEBUG)
					{
						printf("Left Camera: X[%d] = %d Y[%d] = %d\n",i,secondXLeft[i],i,secondYLeft[i]);
						printf("Right Camera: X[%d] = %d Y[%d] = %d\n",j,secondXRight[j],j,secondYRight[j]);
						printf("X = %d Y = %d disparity = %d\n",secondXLeft[i],secondYLeft[i],disparity);
						printf("3D coordinates\n");
						printf("X = %f Y = %f Z = %f\n\n",X,Y,Z);
					}

					// check to see if program reliably recognizes when stick is inside the drum cube
					if((X > -1.5) && (X < 3.5) && (Y > -5) && (Y < -1.5) && (Z > -17) && (Z < -7))
					{
						printf("Inside the drum cube\n");

						// set flag showing that at least one tracked point is 
						// inside of the drum cube this frame	
						oneIn = 1;

						printf("playSound == %d\n",playSound);

						if(playSound == 0)
						{
							printf("No tracked points in drum cube last frame so playing sound\n");
							// Play sound file here
							PlaySound(TEXT("C:\\Users\\Kalev\\Documents\\Visual Studio 2008\\Projects\\VisualDrumset\\VisualDrumset\\snare.wav"), NULL, SND_FILENAME | SND_ASYNC);

							// set the playSound flag for the next frame
							playSound = 1;
							
							
						}
					}
					// tracked point not inside of the drum cube this frame so
					// set playSound to zero so that a sound will register if
					// a tracked point is inside of the drum cube in the next frame
					else if(oneIn == 0)
					{
						printf("No tracked points found in drum cube yet this frame so playSound = 0\n\n");

						playSound = 0;
					}


				}
			}
		}

		printf("Value of playSound after going through all the points in this frame = %d\n", playSound);


		// Debug mode - wait for user input to get next frame
		if(DEBUG)
		{
			// Wait for space bar press to get next frame
			if(cvWaitKey() == 27)
				break;
		}

		

	}

	cvReleaseImage(&img1);
	cvReleaseImage(&img2);

	cvReleaseStereoBMState(&BMState);
	cvReleaseMat(&mx1);
	cvReleaseMat(&my1);
	cvReleaseMat(&mx2);
	cvReleaseMat(&my2);
	cvReleaseMat(&img1r);
	cvReleaseMat(&img2r);
	cvReleaseMat(&disp);
	


	printf("Press Esc to exit program\n");
	while((key = cvWaitKey(0)) != 0x1b)
	{
	}

	
	
	for(int i = 0; i < numCams; i++)
	{
		printf("Stopping capture on camera %d\n", i+1);
		cam[i]->StopCapture();
		delete cam[i];
	}

	cvReleaseImage(&left1);
	cvReleaseImage(&right1);
	cvReleaseImage(&left2);
	cvReleaseImage(&right2);
	cvReleaseImage(&left3);
	cvReleaseImage(&right3);
	cvReleaseImage(&left4);
	cvReleaseImage(&right4);
	cvReleaseImage(&left5);
	cvReleaseImage(&right5);
	cvReleaseImage(&left6);
	cvReleaseImage(&right6);
	cvReleaseImage(&left7);
	cvReleaseImage(&right7);
	cvReleaseImage(&left8);
	cvReleaseImage(&right8);
	cvReleaseImage(&left9);
	cvReleaseImage(&right9);
	cvReleaseImage(&left10);
	cvReleaseImage(&right10);
	cvReleaseImage(&left11);
	cvReleaseImage(&right11);
	cvReleaseImage(&left12);
	cvReleaseImage(&right12);
	cvReleaseImage(&left13);
	cvReleaseImage(&right13);
	cvReleaseImage(&left14);
	cvReleaseImage(&right14);
	cvReleaseImage(&left15);
	cvReleaseImage(&right15);

	return 0;
}

