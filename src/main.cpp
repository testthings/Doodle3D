//openFrameworks & addons
#include "ofMain.h"
#include "ofxExtras.h"
#include "ofxIniSettings.h"
#include "ofxUltimaker.h"
#include "ofxHTTPServer.h"

//globals
ofPath path;
ofImage mask;
ofxIniSettings ini;
ofxUltimaker ultimaker;
ofxGCode gcode;
const int vres=1000;
float vfunc[vres],twists,objectHeight,layerHeight;
float minScale,maxScale,maxScaleDifference,maxObjectHeight;
int targetTemperature=220;
bool debug=false;
bool enableAutoMonitorFolder;
bool useSubpathColors=false;
bool showDeviceName;
string deviceName;
int serverPort;
bool autoDetectDeviceName=false;
bool showStatusMessage=true;

float scaleFunction(float f) {
    //return minScale; //ofMap(f,0,1,minScale,maxScale);
    return vfunc[int(ofMap(f, 0, 1, vres-1, 0, true))];
}

//classes
#include "Btn.h"
#include "Canvas.h"
#include "Files.h"
Files files;

#include "Side.h"
#include "Thermometer.h"
#include "Printer.h"

class ofApp : public ofBaseApp, public ofxHTTPServerListener {
public:

    //application properties
    ofImage bg,bg_busy,vb;
    Btn btnNew,btnSave,btnOops,btnLoadPrevious,btnLoadNext,btnPrint,btnStop;
    Btn btnTwistLeft, btnTwistRight, btnZoomIn, btnZoomOut, btnHigher, btnLower;

    Canvas canvas;
    Side side;
    Thermometer thermometer;
    Printer printer;
    ofxHTTPServer *server;
    ofSerial tmp;

    void setup() {
        loadSettings();
        
        btnNew.setup(0xffff00);
        btnSave.setup(0x00ff00);
        btnOops.setup(0x006464);
        btnLoadPrevious.setup(0x804652);
        btnLoadNext.setup(0x7fb9ad);
        btnPrint.setup(4044666);
        btnStop.setup(0xe6098d);
        btnZoomIn.setup(0x500000);
        btnZoomOut.setup(0x640000);
        btnHigher.setup(0x780000);
        btnLower.setup(0x8c0000);
        btnTwistLeft.setup(0xa00000);
        btnTwistRight.setup(0xb40000);
        
        thermometer.setup();
        canvas.setup();
        files.setup();
        printer.setup();
        bg.loadImage("images/bg.png");
        bg_busy.loadImage("images/bg_busy.png");
        mask.loadImage("images/mask.png");
        
//        ofSetLogLevel(OF_LOG_NOTICE);
        
        ofEnableAlphaBlending();
        ofSetWindowPosition(0,0);
        ofSetFullscreen(ini.get("fullscreen",true));
        ofSetFrameRate(ini.get("frameRate", 30));
        ofSetEscapeQuitsApp(ini.get("quitOnEscape",true));
        ofEnableSmoothing();
        
        if (ini.get("autoDetectDeviceName",true)) {
            ultimaker.autoConnect();
        } else {
            ultimaker.connect(ini.get("device.name",""),115200);
        }
        
        server = ofxHTTPServer::getServer(); // get the instance of the server
        server->setServerRoot("www");		 // folder with files to be served
        server->setUploadDir("upload");		 // folder to save uploaded files
        server->setCallbackExtension("of");	 // extension of urls that aren't files but will generate a post or get event
        server->setListener(*this);
        server->start(ini.get("server.port",serverPort));
    }

    void getRequest(ofxHTTPServerResponse & response){
    }
    
    void postRequest(ofxHTTPServerResponse & response){
        cout << "post" << endl;
        //cout << response.requestFields["data"] << endl;
        
        files.cur = -1;
        canvas.clear();
        
        string data = response.requestFields["data"];
        
        vector<string> items = ofSplitString(data,"\nBEGIN\n");
        items = ofSplitString(items[items.size()-1],"\n\nEND\n");

        if (items.size()>0) {
            vector<string> lines = ofSplitString(items[0],"\n");
            files.loadFromStrings(lines);
            files.save("doodle-"+ofxUrlToSafeLocalPath(ofxGetIsoDateTime())+".txt");
        }

    }


    void update() {
        
        canvas.update();
        
        if (ini.get("autoWarmUp",true) && ofGetFrameNum()==100) {
            ultimaker.send("M109 S" + ofToString(targetTemperature));
        }
        
        if (btnZoomIn.selected) canvas.zoom(1);
        if (btnZoomOut.selected) canvas.zoom(-1);

        if (btnHigher.selected) objectHeight = ofClamp(objectHeight+2, 3, maxObjectHeight);
        if (btnLower.selected) objectHeight = ofClamp(objectHeight-2, 3, maxObjectHeight);

        if (btnTwistLeft.selected) twists-=.01;
        if (btnTwistRight.selected) twists+=.01;
        
        if (btnOops.selected && ofGetFrameNum()%5==0) {
            canvas.undo();
        }
        
        if (enableAutoMonitorFolder && ofGetFrameNum()%60==0) {
            //cout << "listDir" << endl;
            files.listDir();
        }
    }

    void draw() {
        ofSetupScreenOrtho(0,0,OF_ORIENTATION_UNKNOWN,true,-200,200);
        ofSetColor(255);
        if (ultimaker.isBusy || !ultimaker.isStartTagFound || (ini.get("autoWarmUp",true) && ofGetFrameNum()<100)) bg_busy.draw(0,0); else bg.draw(0,0);
        canvas.draw();
        if (debug) canvas.drawDebug();
        side.draw();
        thermometer.draw();
        
        ofSetColor(0);
        
        string status;
        if (ofGetFrameNum()<10*30 && !ultimaker.isStartTagFound) {
            ofSetColor(0);
            status = "Connecting to Ultimaker...";
        } else if (ofGetFrameNum()>10*30 && ofGetFrameNum()<20*30 && !ultimaker.isStartTagFound) {
            ofSetColor(255,0,0);
            status = "Failed to connect. Make sure your Ultimaker runs Marlin firmware at speed 115200 bps";
        } else if (debug) {
            ofSetColor(0);
            status = ofToString(ofGetFrameRate());
        } else {
            ofSetColor(0);
            status = files.getFilename();
        }
        
        if (showStatusMessage) ofDrawBitmapString(status, 20,20);
        if (showDeviceName && !autoDetectDeviceName) ofDrawBitmapString(deviceName, 20,35);
    }

    void loadSettings() {
        ini.load("Doodle3D.ini");

        targetTemperature = ini.get("targetTemperature",220);
        objectHeight = ini.get("objectHeight",40.0f);
        maxObjectHeight = ini.get("maxObjectHeight",200.0f);
        layerHeight = ini.get("layerHeight",.2f);
        minScale=ini.get("minScale",.8f);
        maxScale=ini.get("maxScale",1.2f);        
        side.setShape(ini.get("shape","|").at(0));
        maxScaleDifference=ini.get("maxScaleDifference",.1f); //problematic when resolution>layers. should be half of wallThickness or so.
        side.visible = ini.get("side.visible",true);
        side.is3D = ini.get("side.is3D",false);
        side.bounds = ini.get("side.bounds",ofRectangle(900,210,131,390));
        side.border = ini.get("side.border",ofRectangle(880,170,2,470));
        useSubpathColors = ini.get("useSubpathColors",false);
        printer.screenToMillimeterScale=ini.get("screenToMillimeterScale",.3f);
        printer.feedrate = ini.get("speed",35)*60;
        printer.travelrate = ini.get("travelrate",250)*60;
        printer.wallThickness = ini.get("wallThickness",.8f);
        printer.zOffset = ini.get("zOffset",0.0f);
        printer.useSubLayers = ini.get("useSubLayers",true);
        twists = ini.get("twists",0.0f);
        printer.filamentThickness = ini.get("filamentThickness",2.89f)/10; ////waarom /10 ????
        printer.minimalDistanceForRetraction = ini.get("minimalDistanceForRetraction",5);
        printer.retraction = ini.get("retraction",2);
        printer.retractionSpeed = ini.get("retractionSpeed",100)*60;
        printer.loopAlways = ini.get("loopAlways",false);
        thermometer.showWarmUp = ini.get("showWarmUp",false);
        enableAutoMonitorFolder = ini.get("enableAutoMonitorFolder",true);
        showDeviceName = ini.get("showDeviceName",false);
        serverPort = ini.get("server.port",8888);
        deviceName = ini.get("device.name","device.name undefined");
        showStatusMessage = ini.get("showStatusMessage",true);
    }

    void stop() {
        ultimaker.stopPrint();
        //ultimaker.request("G28 X0 Y0"); //home x,y
        ultimaker.load("gcode/end.gcode");
        ultimaker.startPrint();
        //M84
    }

    void mousePressed(int x, int y, int button) {
        canvas.mousePressed(x, y, button);
        side.mousePressed(x, y, button);
        if (btnNew.hitTest(x,y)) { files.cur=-1; canvas.clear(); files.unloadFile(); }
        if (btnSave.hitTest(x,y)) files.save();
        if (btnLoadPrevious.hitTest(x,y)) files.loadPrevious();
        if (btnLoadNext.hitTest(x,y)) files.loadNext();
        if (btnPrint.hitTest(x,y)) printer.print();
        if (btnStop.hitTest(x,y)) stop();
        if (btnOops.hitTest(x,y)) { btnOops.selected=true; }
        if (btnZoomIn.hitTest(x,y)) btnZoomIn.selected=true;
        if (btnZoomOut.hitTest(x,y)) btnZoomOut.selected=true;
        if (btnHigher.hitTest(x,y)) btnHigher.selected=true;
        if (btnLower.hitTest(x,y)) btnLower.selected=true;
        if (btnTwistLeft.hitTest(x,y)) btnTwistLeft.selected=true;
        if (btnTwistRight.hitTest(x,y)) btnTwistRight.selected=true;
        //cout << ofToHex(mask.getColor(x,y).getHex()) << endl;
    }

    void mouseDragged(int x, int y, int button) {
        canvas.mouseDragged(x, y, button);
        side.mouseDragged(x, y, button);
    }

    void mouseReleased(int x, int y, int button) {
        ofxSimplifyPath(path);
        side.mouseReleased(x, y, button);
        canvas.mouseReleased(x, y, button);
        btnOops.selected=false;
        btnZoomIn.selected=false;
        btnZoomOut.selected=false;
        btnHigher.selected=false;
        btnLower.selected=false;
        btnTwistLeft.selected=false;
        btnTwistRight.selected=false;
    }

    void chmod() {
        string folder = ini.get("copyGCodeToPath","");
        if (folder=="") return;
        string cmd = "chmod -R 777 " + folder;
        system(cmd.c_str());
    }

    void keyPressed(int key) {
        switch (key) {
            case '*': loadSettings(); break;
            case '/': case '\\': case '$': case '#': case '|': case '%': case '@': case '^': case '&': side.setShape(key); break;
            case '3': side.is3D=!side.is3D; break;
            case '<': twists-=.5; break;
            case '>': twists+=.5; break;
            case '?': twists=0; break;
            case 'a': side.toggle(); break;
            case 'b': useSubpathColors=!useSubpathColors; break;
            case 'C': canvas.createCircle(); break;
            case 'c': case 'n': canvas.clear(); files.unloadFile(); break;
            case 'd': debug=!debug; break;
            case 'e': printer.print(true); chmod(); break; //ultimaker.extrude(260,1000); break;
            case 'f': ofToggleFullscreen(); break;
            case 'h': objectHeight+=5; if (objectHeight>maxObjectHeight) objectHeight=maxObjectHeight; break;
            case 'H': objectHeight-=5; if (objectHeight<3) objectHeight=3; break;
            case 'l': files.loadNext(); break;
            case 'L': files.loadPrevious(); break;
            case 'o': files.load(); break;
            case 'p': case 'm': case OF_KEY_RETURN: side.is3D=false; printer.print(); break;
            case 'q': stop(); break;
            case 'r': ultimaker.setRelative(); break;
            case 'S': files.save(); break;
            case 's': files.saveAs(); break;
            case 't': ultimaker.readTemperature(); break;
            case 'u': case 'z': canvas.undo(); break;
            case '~': files.deleteCurrentFile(); break;
            case ' ': files.listDir(); break;
        }
    }

};

//ofxEndApp();

int main() {
    ofAppGlutWindow window;
    window.setGlutDisplayString("rgba double samples>=4");
    ofSetupOpenGL(&window, 1280, 800, OF_WINDOW);
    ofRunApp(new ofApp());
}
  