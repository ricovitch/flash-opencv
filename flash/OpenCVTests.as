package
{
	import cmodule.opencv.CLibInit;
	
	import com.bit101.components.ComboBox;
	import com.fittingbox.utils.debug.Stats;
	
	import flash.display.Bitmap;
	import flash.display.BitmapData;
	import flash.display.Sprite;
	import flash.display.StageAlign;
	import flash.display.StageScaleMode;
	import flash.events.Event;
	import flash.events.TimerEvent;
	import flash.filters.ColorMatrixFilter;
	import flash.geom.Matrix;
	import flash.geom.Rectangle;
	import flash.media.Camera;
	import flash.media.Video;
	import flash.utils.ByteArray;
	import flash.utils.Endian;
	import flash.utils.Timer;
	import flash.utils.getTimer;
	
	[SWF(frameRate=30)]
	public class OpenCVTests extends Sprite
	{
		// opencv cascade files
		[Embed(source="haarcascade_frontalface_alt2.xml", mimeType="application/octet-stream")]
		private const CASCADE_FILE:Class;
		private var _cascadeFile:ByteArray = new CASCADE_FILE() as ByteArray;
		
		[Embed(source="haarcascade_eye_tree_eyeglasses.xml", mimeType="application/octet-stream")]
		private const CASCADE_NESTED_FILE:Class;
		private var _cascadeNestedFile:ByteArray = new CASCADE_NESTED_FILE() as ByteArray;
		
		// opencv c library
		private var _openCVCLib:CLibInit;
		private var _openCV:Object;
		private var _dataPosition:uint;  // C memory bytes offset
		private var _cByteData:ByteArray; // C memory bytes pointer
		
		// camera / video
		private var camera:Camera;
		private var video:Video;
		
		// frame capture/draw
		private var frameCapture:BitmapData; 
		private var frameDraw:BitmapData;
		private var bmp:Bitmap;
		private var frameData:ByteArray;
		
		// ui
		private var _opencvFilterName:String = "raw";
		private var _opencvFilterList:ComboBox;		
		
		public function OpenCVTests()
		{
			stage.align = StageAlign.TOP_LEFT;
			stage.scaleMode = StageScaleMode.NO_SCALE;
			
			// initialize C library
			setupCLib();
			
			// supply cascade files
			setupCascades();
			
			// initialize Camera
			setupCamera();
			
			// init UI
			setupUI();
		}
		
		private function setupCLib():void
		{
			_openCVCLib = new CLibInit();
			_openCV = _openCVCLib.init();
			
			// get C byte data pointer (that is, C RAM chunk)
			// @see http://jasonbshaw.com/?p=183
			var ns:Namespace = new Namespace("cmodule.opencv");
			_cByteData = (ns::gstate).ds;
			trace("Initiliazed openCV C library");
			trace("Total RAM size: " + _cByteData.length);
			trace("Data start offset: " + _cByteData.position);
			trace("Bytes available: " + _cByteData.bytesAvailable);
		}
		
		private function setupUI () : void
		{
			// opencv lib method list
			_opencvFilterList = new ComboBox (this, 20, 20, _opencvFilterName, [
				"raw", "grayScale", "horizontalMirror", "verticalMirror", "medianBlur", "detectAndDraw"
			]);
			_opencvFilterList.addEventListener(Event.SELECT, onOpencvFilterSelect);
			
			// stats
			with (addChild (new Stats (false))) {
				x = stage.stageWidth - 100;
				y = 20;
			}
		}
		private function onOpencvFilterSelect (evt:Event) : void
		{
			_opencvFilterName = _opencvFilterList.items[_opencvFilterList.selectedIndex];
			_openCV.setFilterType (_opencvFilterName);
		}
		
		private function setupCascades ():void
		{
			_openCVCLib.supplyFile ("haarcascade_frontalface_alt2.xml", _cascadeFile);
			_openCV.loadCascade ("cascade", "haarcascade_frontalface_alt2.xml");
			/*_openCVCLib.supplyFile ("haarcascade_eye_tree_eyeglasses.xml", _cascadeNestedFile);
			_openCV.loadCascade ("cascadeNested", "haarcascade_eye_tree_eyeglasses.xml");*/
		}
		
		private function setupCamera():void
		{
			camera = Camera.getCamera();
			//initialized camera
			if (camera!=null){
				camera.setMode(640,480,30,true);
				
				//make a new video obj
				video = new Video(camera.width,camera.height);
				video.attachCamera (camera);
				
				// make a framedata for a current frame and for draw frame
				frameCapture = new BitmapData(video.width, video.height);
				frameDraw = new BitmapData(video.width,video.height);
				
				//acquire first frame to make a correct pixel storage
				frameCapture.draw(video);
				var bounds:Rectangle = new Rectangle(0,0,frameCapture.width,frameCapture.height);
				var pixels:ByteArray = frameCapture.getPixels(bounds);
				//get pointer to C byte data
				trace("Allocating "+pixels.length+" bytes in C memory");
				_dataPosition = _openCV.initByteArray(pixels.length);
				trace("Allocated at "+_dataPosition);
				
				//trace("Transfering frame parameters to C libraries");
				//set frame properties for C memory libraries
				_openCV.setFrameParams (frameCapture.width, frameCapture.height, pixels.length/(frameCapture.width*frameCapture.height));
				
				//create a drawing bitmap
				bmp = new Bitmap(frameDraw);
				
				// FIXME apply color matrix filter to invert endian corruption of image data 
				var matrix:Array = new Array();
				matrix=matrix.concat([0, 1, 0, 0, 0]);// red
				matrix=matrix.concat([1, 0, 0, 0, 0]);// green
				matrix=matrix.concat([0, 0, 0, 1, 0]);// blue
				matrix=matrix.concat([0, 0, 0, 0, 255]);// alpha
				bmp.filters = [new ColorMatrixFilter(matrix)];
				
				//Add bmp to show
				with (addChild(bmp)) {
					x = (stage.stageWidth - 640)*.5;
					y = (stage.stageHeight - 480)*.5;
				}
				
				//make an enter frame listener
				addEventListener(Event.ENTER_FRAME,enterFrameHandler);
				/*var testTimer:Timer = new Timer (40);
				testTimer.addEventListener(TimerEvent.TIMER, enterFrameHandler);
				testTimer.start();*/
			}
		}
		
		private function enterFrameHandler(event:Event = null):void
		{
			if (camera!=null)
			{
				//Get a frame from camera
				frameCapture.draw (video);
				
				//Get pixel data from frame
				var bounds:Rectangle = new Rectangle (0, 0, frameCapture.width, frameCapture.height);
				frameData = frameCapture.getPixels (bounds);
				frameData.position = 0;
				var dataLength:int = frameData.length;
				
				//C++ will do its dirty job here...
				//first we have to make C++ aware of the pixels we want to use
				//and actually copy them to C memory!
				_cByteData.position = _dataPosition;
				_cByteData.writeBytes (frameData, 0, dataLength);
				var start:int = getTimer();
				switch (_opencvFilterName) {
					case "raw" :
						break;
					case "grayScale" :
					case "medianBlur" :
					case "horizontalMirror" :
					case "verticalMirror" :
						_openCV.applyFilter ();
						break;
					case "detectAndDraw" :
						_openCV.detectAndDraw ();
						break;
				}
				var end:int = getTimer();
				trace ("[FLASH] applyFilter: " + (end - start));
				
				// read output image data as BIG_ENDIAN (Alchemy Memory is LITTLE_ENDIAN) 
				/**
				frameData.position = 0;
				frameData.writeBytes (_cByteData, _dataPosition, dataLength);
				frameData.endian = Endian.BIG_ENDIAN;
				frameData.position = 0;
				// draw result
				frameDraw.setPixels (bounds, frameData);
				/*/
				
				// direct without endian conversion
				_cByteData.position = _dataPosition;
				frameDraw.setPixels (bounds, _cByteData);
				/**/
			}
		}
	}
}