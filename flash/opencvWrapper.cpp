#include "AS3.h"
#include "opencv/cv.h"
#include <stdio.h>
#include <sys/time.h>
#include "lrint.cpp"

// FIXME embed haarcascade definition files (old version ?)
/*extern "C"
{
	#include <FbViolaJonesStaticFaceCascadeData.h>
	#include <FbViolaJonesStaticEyesCascadeData.h>
}*/

//screen buffer
long bufferSize;
char* buffer;
//flash webcam size
int frameWidth, frameHeight, frameChannels;
//filter to apply
char * filterType = "grayScale";
cv::CascadeClassifier cascade, nestedCascade;

//FLASH: C memory init function
static AS3_Val initByteArray(void* self, AS3_Val args)
{
	//read buffer size as int from input
	AS3_ArrayValue (args, "IntType", &bufferSize);
	//allocate buffer size
	buffer = new char[bufferSize];
	//return ptr to buffer location in memory
	return AS3_Ptr(buffer);
}

//FLASH: C memory free function
static AS3_Val freeByteArray(void* self, AS3_Val args)
{
	delete[] buffer;
	*buffer=0;
	bufferSize=-1;
	return 0;
}

//FLASH: Camera parameters setup
static AS3_Val setFrameParams(void* self, AS3_Val args)
{
	AS3_ArrayValue(args,"IntType, IntType, IntType",&frameWidth,&frameHeight,&frameChannels);
	//fprintf(stderr, "[OPENCV] setFrameParams: %d - %d - %d", frameWidth, frameHeight, frameChannels);
	return 0;
}

static AS3_Val setFilterType(void* self, AS3_Val args)
{
	AS3_ArrayValue(args, "StrType", &filterType);
	return 0;
}

static AS3_Val applyFilter(void* self, AS3_Val args)
{	
	// Flash image data in ARGB format :
	cv::Mat img (frameHeight, frameWidth, CV_8UC4, (void*)buffer);
	// C process duration
	struct timeval start, end;
    long mtime, seconds, useconds;
    gettimeofday(&start, NULL);
	
	// grayScale filter
	if (strcmp (filterType, "grayScale") == 0) {
		cv::Mat grayImg;
		cv::cvtColor (img, grayImg, CV_RGBA2GRAY); // should be ARGB2GRAY ?

		//mix channels so we output argb data
		int gs_to_argb[] = { 0,1,  0,2,  0,3 };
		mixChannels (&grayImg, 1, &img, 1, gs_to_argb, 3);
		
	// medianBlur filter
	} else if (strcmp (filterType, "medianBlur") == 0) {
		cv::medianBlur (img, img, 5);
		
	// flip vertical
	} else if (strcmp (filterType, "verticalMirror") == 0) {
		cv::flip (img, img, 0);
		
	// flip horizontal
	} else if (strcmp (filterType, "horizontalMirror") == 0) {
		cv::flip (img, img, 1);
	}
	// C process duration
	gettimeofday(&end, NULL);
    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;
    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
	fprintf(stderr, "[OPENCV] applyFilter: %ld", mtime);
	
	return 0;
}

//FLASH: load face detection cascades xml files from flash bytearrays
//must use clib.supplyFile method before calling this method from flash
static AS3_Val loadCascade(void* self, AS3_Val args)
{
	//parse parameters
	char * cascadeType;
	char * cascadeFileName;
	AS3_ArrayValue(args,"StrType, StrType", &cascadeType, &cascadeFileName);
	
	FILE * file;
	long fileSize;
	char * fileBuffer;
	file = fopen(cascadeFileName, "rb");
 
	//Get file size
	fseek (file, 0, SEEK_END);
	fileSize = ftell(file);
	rewind(file);
 
	//Allocate buffer
	fileBuffer = (char*) malloc(sizeof(char)*fileSize);
 
	//Read file into buffer
	fread(fileBuffer, 1, fileSize, file);
	fprintf(stderr, "[OPENCV] loadCascades: %s : %s", cascadeType, cascadeFileName);
	
	//CV CascadeClassifier instance init
	/*cv::FileStorage cascadeFileStorage;
	cascadeFileStorage.open (cascadeFileName, cv::FileStorage::READ);
	fprintf(stderr, "[OPENCV] loadCascades: cascade files empty : %d", cascadeFileStorage.getFirstTopLevelNode().empty());
	bool success = cascade.read (cascadeFileStorage.getFirstTopLevelNode());
	fprintf(stderr, "[OPENCV] loadCascades: cascade files empty : %d", cascade.empty());
	cascadeFileStorage.release ();*/
	
	/*CvHaarClassifierCascade* cascadeClassifier = (CvHaarClassifierCascade*) cvLoad(cascadeFileName,0,0,0);
	fprintf(stderr, "[OPENCV] loadCascades: cascade files loaded");*/
	
	cv::FileNode cascadeFileNode;
	cascadeFileNode.readRaw ("xml", reinterpret_cast<uchar*>(fileBuffer), fileSize);
	bool success = cascade.read (cascadeFileNode);
	fprintf(stderr, "[OPENCV] loadCascades: cascade files empty : %d", cascade.empty());
	if (success) {
		fprintf(stderr, "[OPENCV] loadCascades: cascade files loaded with success !");
	} else {
		fprintf(stderr, "[OPENCV] loadCascades: cascade files failed to load !");
	}
	
	//close file and free allocated buffer
	fclose (file);
	free (fileBuffer);
	
	return 0;
}

static AS3_Val detectAndDraw(void* self, AS3_Val args)
{	
	// Flash image data in ARGB format :
	cv::Mat img (frameHeight, frameWidth, CV_8UC4, (void*)buffer);

    int i = 0;
    double t = 0;
	double scale = 1;
    vector<cv::Rect> faces;
    const static cv::Scalar colors[] =  {
		cv::Scalar(0, 0, 255),
        cv::Scalar(0,128,255),
        cv::Scalar(0,255,255),
        cv::Scalar(0,255,0),
        cv::Scalar(255,128,0),
        cv::Scalar(255,255,0),
        cv::Scalar(255,0,0),
        cv::Scalar(255,0,255)
	};
    /*cv::Mat grayImg, smallImg( cvRound (img.rows/scale), cvRound(img.cols/scale), CV_8UC1 );
	
	cv::cvtColor( img, grayImg, CV_RGBA2GRAY );
	fprintf(stderr, "[OPENCV] grayImg: %d - %d - %d", grayImg.rows, grayImg.cols, grayImg.channels());
    cv::resize( grayImg, smallImg, smallImg.size(), 0, 0);//, INTER_LINEAR );
	fprintf(stderr, "[OPENCV] smallImg: %d - %d - %d", smallImg.rows, smallImg.cols, smallImg.channels());
    cv::equalizeHist( smallImg, smallImg );*/
	
	/*cv::CascadeClassifier cascade;
	cascade.oldCascade = cv::Ptr<CvHaarClassifierCascade>((CvHaarClassifierCascade*)&kFaceDetectorClassifierCascade);
	fprintf(stderr, "[OPENCV] cascade.oldCascade: %s - %d", cascade.oldCascade, cascade.oldCascade.empty());*/
	
	// test draw
	cv::Point center;
	center.x = 100;
    center.y = 100;
    int radius = 50;
	cv::Scalar color = colors[0];
    circle( img, center, radius, color, 3, 8, 0 );
	
    /*if(!cascade.oldCascade.empty()) cascade.detectMultiScale( smallImg, faces,
        1.1, 2, 0
        //|CV_HAAR_FIND_BIGGEST_OBJECT
        //|CV_HAAR_DO_ROUGH_SEARCH
        |CV_HAAR_SCALE_IMAGE
        ,
        cv::Size(30, 30) );*/
    // for( vector<cv::Rect>::const_iterator r = faces.begin(); r != faces.end(); r++, i++ )
    // {
        // cv::Mat smallImgROI;
        // vector<cv::Rect> nestedObjects;
        // cv::Point center;
        // cv::Scalar color = colors[i%8];
        // int radius;
        // center.x = cvRound((r->x + r->width*0.5)*scale);
        // center.y = cvRound((r->y + r->height*0.5)*scale);
        // radius = cvRound((r->width + r->height)*0.25*scale);
        // circle( img, center, radius, color, 3, 8, 0 );
        // // if( nestedCascade.empty() )
            // // continue;
        // // smallImgROI = smallImg(*r);
        // // nestedCascade.detectMultiScale( smallImgROI, nestedObjects,
            // // 1.1, 2, 0
            // // //|CV_HAAR_FIND_BIGGEST_OBJECT
            // // //|CV_HAAR_DO_ROUGH_SEARCH
            // // //|CV_HAAR_DO_CANNY_PRUNING
            // // |CV_HAAR_SCALE_IMAGE
            // // ,
            // // Size(30, 30) );
        // // for( vector<Rect>::const_iterator nr = nestedObjects.begin(); nr != nestedObjects.end(); nr++ )
        // // {
            // // center.x = cvRound((r->x + nr->x + nr->width*0.5)*scale);
            // // center.y = cvRound((r->y + nr->y + nr->height*0.5)*scale);
            // // radius = cvRound((nr->width + nr->height)*0.25*scale);
            // // circle( img, center, radius, color, 3, 8, 0 );
        // // }
    // }
	
	return 0;
}

//entry point
int main()
{
	AS3_Val initByteArrayMethod = AS3_Function(NULL, initByteArray);
	AS3_Val freeByteArrayMethod = AS3_Function(NULL, freeByteArray);
	AS3_Val setFrameParamsMethod = AS3_Function(NULL, setFrameParams);
	AS3_Val loadCascadeMethod = AS3_Function(NULL, loadCascade);
	AS3_Val detectAndDrawMethod = AS3_Function(NULL, detectAndDraw);
	AS3_Val applyFilterMethod = AS3_Function(NULL, applyFilter);
	AS3_Val setFilterTypeMethod = AS3_Function(NULL, setFilterType);

	AS3_Val result = AS3_Object("initByteArray: AS3ValType,\
		freeByteArray: AS3ValType,\
		setFrameParams: AS3ValType,\
		loadCascade: AS3ValType,\
		detectAndDraw: AS3ValType,\
		applyFilter: AS3ValType,\
		setFilterType: AS3ValType",
		initByteArrayMethod,
		freeByteArrayMethod,
		setFrameParamsMethod,
		loadCascadeMethod,
		detectAndDrawMethod,
		applyFilterMethod,
		setFilterTypeMethod);

	AS3_Release (initByteArrayMethod);
	AS3_Release (freeByteArrayMethod);
	AS3_Release (setFrameParamsMethod);
	AS3_Release (loadCascadeMethod);
	AS3_Release (detectAndDrawMethod);
	AS3_Release (applyFilterMethod);
	AS3_Release (setFilterTypeMethod);

	AS3_LibInit (result);
	return 0;
}