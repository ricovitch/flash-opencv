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
	import flash.text.TextField;
	import flash.text.TextFieldAutoSize;
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
		private var _openCVMemory:ByteArray; 	// C memory bytes pointer
		private var _openCVMemoryPTR:uint;  	// C memory bytes offset
		private var _openCV:Object; 			// C lib
		
		// camera / video
		private var _camera:Camera;
		private var _video:Video;
		
		// frame capture/draw
		private var _frameCapture:BitmapData; 
		private var _frameDraw:BitmapData;
		private var _bmp:Bitmap;
		private var _frameData:ByteArray;
		
		// ui
		private var _opencvFilterName:String = "raw";
		private var _opencvFilterList:ComboBox;
		private var _stats:Stats;
		private var _debugTF:TextField;
		
		// filter apply duration average
		private var _durations:Array = [];
		private var _durAverage:Number = 0;
		
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
		
		/**
		 * initialize C library and get "Alchemy Memory" ByteArray
		 */
		private function setupCLib():void
		{
			_openCVCLib = new CLibInit();
			_openCV = _openCVCLib.init();
			
			// get C byte data pointer (that is, C RAM chunk)
			// @see http://jasonbshaw.com/?p=183
			var ns:Namespace = new Namespace("cmodule.opencv");
			_openCVMemory = (ns::gstate).ds;
			trace("Initiliazed openCV C library");
			trace("Total RAM size: " + _openCVMemory.length);
			trace("Data start offset: " + _openCVMemory.position);
			trace("Bytes available: " + _openCVMemory.bytesAvailable);
		}
		
		/**
		 * initialize UI
		 */
		private function setupUI () : void
		{
			// opencv lib method list
			_opencvFilterList = new ComboBox (this, 20, 20, "Choose a filter", [
				"raw", "grayScale", "horizontalMirror", "verticalMirror", "medianBlur", "detectAndDraw"
			]);
			_opencvFilterList.addEventListener(Event.SELECT, onOpencvFilterSelect);
			
			// stats
			_stats = addChild (new Stats (false)) as Stats;
			
			// debug TF
			_debugTF = addChild (new TextField ()) as TextField;
			_debugTF.width = 640;
			_debugTF.multiline = true;
			_debugTF.wordWrap = true;
			_debugTF.autoSize = TextFieldAutoSize.LEFT;
			
			stage.addEventListener(Event.RESIZE, onStageResize);
			onStageResize ();
		}
		// filter ComboBox select
		private function onOpencvFilterSelect (evt:Event) : void
		{
			_opencvFilterName = _opencvFilterList.items[_opencvFilterList.selectedIndex];
			_openCV.setFilterType (_opencvFilterName);
			_durations = [];
		}
		// stage resize
		private function onStageResize (evt:Event = null) : void
		{
			_stats.x = stage.stageWidth - 100;
			_stats.y = 20;
			_bmp.x = _debugTF.x = (stage.stageWidth - 640)*.5;
			_bmp.y = (stage.stageHeight - 480)*.5;
			_debugTF.y = _bmp.y + 490;
		}
		
		/**
		 * FIXME supply cascade files to opencv lib
		 */
		private function setupCascades ():void
		{
			_openCVCLib.supplyFile ("haarcascade_frontalface_alt2.xml", _cascadeFile);
			_openCV.loadCascade ("cascade", "haarcascade_frontalface_alt2.xml");
			/*_openCVCLib.supplyFile ("haarcascade_eye_tree_eyeglasses.xml", _cascadeNestedFile);
			_openCV.loadCascade ("cascadeNested", "haarcascade_eye_tree_eyeglasses.xml");*/
		}
		
		/**
		 * setup camera/video and frame objects
		 */
		private function setupCamera():void
		{
			_camera = Camera.getCamera();
			//initialized camera
			if (_camera!=null){
				_camera.setMode(640,480,30,true);
				
				//make a new video obj
				_video = new Video(_camera.width,_camera.height);
				_video.attachCamera (_camera);
				
				// make a framedata for a current frame and for draw frame
				_frameCapture = new BitmapData(_video.width, _video.height);
				_frameDraw = new BitmapData(_video.width,_video.height);
				
				//acquire first frame to make a correct pixel storage
				_frameCapture.draw(_video);
				var bounds:Rectangle = new Rectangle(0,0,_frameCapture.width,_frameCapture.height);
				var pixels:ByteArray = _frameCapture.getPixels(bounds);
				//get pointer to C byte data
				trace("Allocating "+pixels.length+" bytes in C memory");
				_openCVMemoryPTR = _openCV.initByteArray(pixels.length);
				trace("Allocated at "+_openCVMemoryPTR);
				
				//trace("Transfering frame parameters to C libraries");
				//set frame properties for C memory libraries
				_openCV.setFrameParams (_frameCapture.width, _frameCapture.height, pixels.length/(_frameCapture.width*_frameCapture.height));
				
				//create a drawing bitmap
				_bmp = new Bitmap(_frameDraw);
				
				// apply color matrix filter to invert endian corruption of image data 
				var matrix:Array = new Array();
				matrix=matrix.concat([0, 1, 0, 0, 0]);// red
				matrix=matrix.concat([1, 0, 0, 0, 0]);// green
				matrix=matrix.concat([0, 0, 0, 1, 0]);// blue
				matrix=matrix.concat([0, 0, 0, 0, 255]);// alpha
				_bmp.filters = [new ColorMatrixFilter(matrix)];
				addChild(_bmp);
				
				//make an enter frame listener
				addEventListener(Event.ENTER_FRAME,enterFrameHandler);
			}
		}
		
		/**
		 * enterframe handler
		 */
		private function enterFrameHandler(event:Event = null):void
		{
			if (_camera!=null)
			{
				//Get a frame from camera
				_frameCapture.draw (_video);
				
				//Get pixel data from frame
				var bounds:Rectangle = new Rectangle (0, 0, _frameCapture.width, _frameCapture.height);
				_frameData = _frameCapture.getPixels (bounds);
				_frameData.position = 0;
				var dataLength:int = _frameData.length;
				
				//C++ will do its dirty job here...
				//first we have to make C++ aware of the pixels we want to use
				//and actually copy them to C memory!
				_openCVMemory.position = _openCVMemoryPTR;
				_openCVMemory.writeBytes (_frameData, 0, dataLength);
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
				// duration / average duration
				var duration:int = getTimer() - start;
				_durations.push (duration);
				_durAverage = 0;
				var i:int, len:int = _durations.length;
				for (i=0; i<len; ++i) _durAverage += _durations[i];
				_durAverage /= len;
				_debugTF.text = "Filter applied in : " + duration + " ms (AVG: " + _durAverage + " ms)";
				
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
				_openCVMemory.position = _openCVMemoryPTR;
				_frameDraw.setPixels (bounds, _openCVMemory);
				/**/
			}
		}
	}
}