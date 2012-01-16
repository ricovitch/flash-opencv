#include "AS3.h"
#include "opencv/cv.h"
#include <stdio.h>
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
	// convert ARGB to BGRA :
	cv::Mat imgflash (frameHeight, frameWidth, CV_8UC4, (void*)buffer);//(void*)dst);
	cv::Mat img (imgflash.rows, imgflash.cols, CV_8UC4);
	int from_to[] = { 0,3,  1,2,  2,1,  3,0 };
	mixChannels( &imgflash, 1, &img, 2, from_to, 4 );
	
	// grayScale filter
	if (strcmp (filterType, "grayScale") == 0) {
		cv::Mat grayImg;
		cv::cvtColor (img, grayImg, CV_BGRA2GRAY);

		//mix channels so we output rgb data
		int from_to_gs[] = { 0,0,  0,1,  0,2 };
		mixChannels (&grayImg, 1, &img, 1, from_to_gs, 3);
		
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
	
	// convert BGRA to ARGB :
	cv::Mat out( img.rows, img.cols, CV_8UC4 );
	mixChannels( &img, 1, &out, 2, from_to, 4 );
	
	// copy output image data to buffer
	out.copyTo (imgflash);
	
	return 0;
}

//FLASH: load face detection cascades xml files from flash bytearrays
//must use clib.supplyFile method before calling this method from flash
/*static AS3_Val loadCascades(void* self, AS3_Val args)
{
	//parse parameters
	char * cascadeFileName;
	char * nestedCascadFileName;
	AS3_ArrayValue(args,"StrType, StrType", &cascadeFileName, &nestedCascadFileName);
	
	if( !cascade.load( cascadeFileName ) )
    {
		return AS3_String("error loading cascade file");
	}
	if( !nestedCascade.load( nestedCascadFileName ) )
    {
		return AS3_String("error loading nested cascade file");
	}
	return AS3_String("cascade files loaded with success !");
}*/

static AS3_Val detectAndDraw(void* self, AS3_Val args)
{
	//parse parameters
	AS3_Val byteArr;
	long szByteArr = frameHeight * frameWidth * 4;
	AS3_ArrayValue (args, "AS3ValType", &byteArr);
	
	//alloc memory for transfers
	uchar *dst = new uchar[szByteArr];
	
	//read data
	AS3_ByteArray_readBytes ((void*)dst, byteArr, szByteArr);

	//image > convert argb to bgra :
	cv::Mat imgflash (frameHeight, frameWidth, CV_8UC4, (void*)dst);
	cv::Mat img (imgflash.rows, imgflash.cols, CV_8UC4);
	int from_to[] = { 0,3,  1,2,  2,1,  3,0 };
	mixChannels( &imgflash, 1, &img, 2, from_to, 4 );

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
    //cv::imshow( "result", img );
	
	// convert bgra back to argb :
	cv::Mat out( img.rows, img.cols, CV_8UC4 );
	mixChannels( &img, 1, &out, 2, from_to, 4 );
	
	AS3_ByteArray_seek(byteArr, 0, SEEK_SET);
	AS3_ByteArray_writeBytes(byteArr, out.data, szByteArr);
	delete[] dst;  
	return byteArr;
}

//entry point
int main()
{
	AS3_Val initByteArrayMethod = AS3_Function(NULL, initByteArray);
	AS3_Val freeByteArrayMethod = AS3_Function(NULL, freeByteArray);
	AS3_Val setFrameParamsMethod = AS3_Function(NULL, setFrameParams);
	//AS3_Val loadCascadesMethod = AS3_Function(NULL, loadCascades);
	AS3_Val detectAndDrawMethod = AS3_Function(NULL, detectAndDraw);
	AS3_Val applyFilterMethod = AS3_Function(NULL, applyFilter);
	AS3_Val setFilterTypeMethod = AS3_Function(NULL, setFilterType);

	AS3_Val result = AS3_Object("initByteArray: AS3ValType,\
		freeByteArray: AS3ValType,\
		setFrameParams: AS3ValType,\
		detectAndDraw: AS3ValType,\
		applyFilter: AS3ValType,\
		setFilterType: AS3ValType",
		initByteArrayMethod,
		freeByteArrayMethod,
		setFrameParamsMethod,
		//loadCascadesMethod,
		detectAndDrawMethod,
		applyFilterMethod,
		setFilterTypeMethod);

	AS3_Release (initByteArrayMethod);
	AS3_Release (freeByteArrayMethod);
	AS3_Release (setFrameParamsMethod);
	//AS3_Release (loadCascadesMethod);
	AS3_Release (detectAndDrawMethod);
	AS3_Release (applyFilterMethod);
	AS3_Release (setFilterTypeMethod);

	AS3_LibInit (result);
	return 0;
}