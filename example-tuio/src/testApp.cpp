#include "testApp.h"

using namespace cv;
using namespace ofxCv;

void testApp::setup() {

    settings.loadFile("settings.xml");

	ofSetVerticalSync(true);
	ofSetCircleResolution(64);
	ofSetLogLevel(OF_LOG_VERBOSE);
  
    isDragging = false;
    dragOffset.x = settings.getValue("settings:drag-offset-x", 0);
    dragOffset.y = settings.getValue("settings:drag-offset-y", 0);
  
    float dim = 24; 
	float xInit = OFX_UI_GLOBAL_WIDGET_SPACING; 
    float length = 320-xInit; 

    scale = 0.15;
    minClusterSize = 1;
    maxPointDistance = 50;
    maxClusterCount = 12;
    maxStddev = 60;

    gui = new ofxUICanvas(0, 0, length+xInit, ofGetHeight());
    gui->addWidgetDown(new ofxUILabel("SETTINGS", OFX_UI_FONT_LARGE));
    gui->addSlider("SCALE", 0.0, 1.0, scale, length-xInit,dim);
    //gui->addSlider("MAX STD DEV", 1, 300, maxStddev, length-xInit,dim);
    //gui->addSlider("MAX CLUSTERS", 1, 100, maxClusterCount, length-xInit,dim);
    gui->addSlider("MIN CLUSTER SIZE", 1, 100, minClusterSize, length-xInit,dim);
    gui->addSlider("MAX POINT DISTANCE", 1, 300, maxPointDistance, length-xInit,dim);
    gui->addSlider("MAX FPS", 1, 60, 30, length-xInit,dim);
    gui->addWidgetDown(new ofxUIFPS(OFX_UI_FONT_MEDIUM)); 
    ofAddListener(gui->newGUIEvent,this,&testApp::guiEvent);
    gui->loadSettings("GUI/guiSettings.xml");
	
	grabber.setup();
  
	sick = &grabber;
  
  	vertices[0].x = settings.getValue("settings:p0:x", 1200.0);
	vertices[0].y = settings.getValue("settings:p0:y", -800.0);
	vertices[1].x = settings.getValue("settings:p1:x", 1200.0);
	vertices[1].y = settings.getValue("settings:p1:y", 800.0);
	vertices[2].x = settings.getValue("settings:p2:x", 2400.0);
	vertices[2].y = settings.getValue("settings:p2:y", 800.0);
	vertices[3].x = settings.getValue("settings:p3:x", 2400.0);
	vertices[3].y = settings.getValue("settings:p3:y", -800.0);
    numVertices = 4;
    for (int i = 0; i < numVertices; i++){
		vertices[i].bOver = false;
		vertices[i].bBeingDragged = false;
		vertices[i].radius = 40;
        trackingRegion.addVertex(vertices[i].x,vertices[i].y);
	}
    tracker.setupNaive(minClusterSize,maxPointDistance);
    tracker.setRegion(trackingRegion);
	tracker.setMaximumDistance(100);
	tracker.setPersistence(10);
  
    tuioServer = new TuioServer("127.0.0.1",3333); //external client
  
    ofSetFrameRate(30);
}

void testApp::update() {
	sick->update();
	if(sick->isFrameNew()) {
		tracker.update(*sick);
	}
  
    TuioTime frameTime = TuioTime::getSessionTime();
	tuioServer->initFrame(frameTime);
    vector<unsigned int> currentLabels = tracker.getCurrentLabels();
  
    //add/update cursors
    for(int i = 0; i < currentLabels.size(); i++){
    
      //calculate normalized coordinates
      //http://math.stackexchange.com/questions/13404/mapping-irregular-quadrilateral-to-a-rectangle
      cv:Point_<float> c = tracker.getCurrent(currentLabels[i]);
      ofVec2f p = ofVec2f(c.x,c.y);
  
      ofVec2f p0 = ofVec2f(vertices[0].x,vertices[0].y);
      ofVec2f p1 = ofVec2f(vertices[1].x,vertices[1].y);
      ofVec2f p2 = ofVec2f(vertices[2].x,vertices[2].y);
      ofVec2f p3 = ofVec2f(vertices[3].x,vertices[3].y);
    
      ofVec2f n0 = ofVec2f(p1-p0).getPerpendicular();
      ofVec2f n1 = ofVec2f(p2-p1).getPerpendicular();
      ofVec2f n2 = ofVec2f(p3-p2).getPerpendicular();
      ofVec2f n3 = ofVec2f(p0-p3).getPerpendicular();
    
      float u =  ((p-p1).dot(n0)) / ( (p-p1).dot(n0) + (p-p3).dot(n2) );
      float v =  1-((p-p1).dot(n1)) / ( (p-p1).dot(n1) + (p-p3).dot(n3) );
      
      bool tracked = false;
      for(int j = 0; j < clusters.size(); j++){
        if(currentLabels[i] == clusters[j].label){
          tracked = true;
          clusters[j].cursor->update(frameTime,u,v);
          tuioServer->updateExternalTuioCursor(clusters[j].cursor);
          break;
        }
      }
      if(!tracked){
        trackedCluster c;
        c.label = currentLabels[i];
        c.cursor = new TuioCursor(frameTime,c.label,c.label,u,v);
        tuioServer->addExternalTuioCursor(c.cursor);
        clusters.push_back(c);
      }
    }
  
    //remove inactive cursors
    for(int i = 0; i < clusters.size(); i++){
      bool active = false;
      for(int j = 0; j < currentLabels.size(); j ++){
        if(currentLabels[i] == clusters[j].label){
          active = true;
          break;
        }
      }
      if(!active){
        tuioServer->removeExternalTuioCursor(clusters[i].cursor);
        clusters.erase(clusters.begin()+i);
      }
    }
  
	tuioServer->commitFrame();
}

void testApp::exit()
{
    gui->saveSettings("GUI/guiSettings.xml");
    settings.saveFile("settings.xml");
	delete gui; 
}

void testApp::guiEvent(ofxUIEventArgs &e)
{
    isDragging = false;
    string name = e.widget->getName();
	if(name == "SCALE"){
		ofxUISlider *slider = (ofxUISlider *) e.widget;
		scale = slider->getScaledValue();
	}else if(name == "MAX STD DEV"){
        ofxUISlider *slider = (ofxUISlider *) e.widget;
        this->maxStddev = slider->getScaledValue();
        tracker.setupKmeans(this->maxStddev,this->maxClusterCount);
    }else if(name == "MAX CLUSTERS"){
        ofxUISlider *slider = (ofxUISlider *) e.widget;
        this->maxClusterCount = slider->getScaledValue();
        tracker.setupKmeans(this->maxStddev,this->maxClusterCount);
    }else if(name == "MIN CLUSTER SIZE"){
        ofxUISlider *slider = (ofxUISlider *) e.widget;
        this->minClusterSize = slider->getScaledValue();
        tracker.setupNaive(this->minClusterSize,this->maxPointDistance);
    }else if(name == "MAX POINT DISTANCE"){
        ofxUISlider *slider = (ofxUISlider *) e.widget;
        this->maxPointDistance = slider->getScaledValue();
        tracker.setupNaive(this->minClusterSize,this->maxPointDistance);
    }else if(name == "MAX FPS"){
        ofxUISlider *slider = (ofxUISlider *) e.widget;
        ofSetFrameRate(slider->getScaledValue());
    }
}

void testApp::draw() {
	ofBackground(0);

	ofPushMatrix();
    ofTranslate(20, ofGetHeight() - 20);
	ofSetColor(255);
	ofCircle(0, 0, 7);
	ofDrawBitmapString(ofToString(tracker.size()) + " clusters", -4, 4);
	ofPopMatrix();
	
	ofPushMatrix();
  	ofTranslate(ofGetWidth() / 2 + dragOffset.x, ofGetHeight() / 2 + dragOffset.y);
	ofScale(scale, scale);
  
	sick->draw(12, 2400);
	tracker.draw();
  
    ofNoFill();
    ofSetColor(255);
    for (int i = 0; i < numVertices; i++){
        if (vertices[i].bOver == true) ofFill();
        else ofNoFill();
        ofCircle(vertices[i].x,vertices[i].y,vertices[i].radius);
    }
  
    float length = 100;
  
    //http://math.stackexchange.com/questions/13404/mapping-irregular-quadrilateral-to-a-rectangle
    ofVec2f p0 = ofVec2f(vertices[0].x,vertices[0].y);
    ofVec2f p1 = ofVec2f(vertices[1].x,vertices[1].y);
    ofVec2f p2 = ofVec2f(vertices[2].x,vertices[2].y);
    ofVec2f p3 = ofVec2f(vertices[3].x,vertices[3].y);
  
    ofDrawBitmapString("P0",ofPoint(p0.x+length/2,p0.y-length/2));
    ofDrawBitmapString("P1",ofPoint(p1.x+length/2,p1.y-length/2));
    ofDrawBitmapString("P2",ofPoint(p2.x+length/2,p2.y-length/2));
    ofDrawBitmapString("P3",ofPoint(p3.x+length/2,p3.y-length/2));

    ofVec2f n0 = ofVec2f(p1-p0).getPerpendicular();
    ofVec2f n1 = ofVec2f(p2-p1).getPerpendicular();
    ofVec2f n2 = ofVec2f(p3-p2).getPerpendicular();
    ofVec2f n3 = ofVec2f(p0-p3).getPerpendicular();
    
    //draw normal vectors http://forum.openframeworks.cc/index.php?topic=8105.0
    ofLine(p0.getMiddle(p1),p0.getMiddle(p1)-n0*length);
    ofLine(p1.getMiddle(p2),p1.getMiddle(p2)-n1*length);
    ofLine(p2.getMiddle(p3),p2.getMiddle(p3)-n2*length);
    ofLine(p3.getMiddle(p0),p3.getMiddle(p0)-n3*length);
    
    if(trackingRegion.inside(mousePosition.x,mousePosition.y)){
        ofSetColor(255,0,255);
      
        ofVec2f p = ofVec2f(mousePosition.x, mousePosition.y);
      
        double u =  ((p-p1).dot(n0)) / ( (p-p1).dot(n0) + (p-p3).dot(n2) );
        double v =  1-((p-p1).dot(n1)) / ( (p-p1).dot(n1) + (p-p3).dot(n3) );
      
        ofLine(ofPoint(p.x+length,p.y),ofPoint(p.x-length,p.y));
        ofLine(ofPoint(p.x,p.y+length),ofPoint(p.x,p.y-length));
      
        ofSetColor(255);
        ofDrawBitmapString(ofToString(u)+','+ofToString(v),ofPoint(mousePosition.x+length/2,mousePosition.y-length/2));
    }
  
	ofPopMatrix();
}

//------------- -------------------------------------------------
void testApp::mouseMoved(int x, int y ){
    mousePosition.x = (x - ofGetWidth() / 2 - dragOffset.x) / scale;
    mousePosition.y = (y - ofGetHeight() / 2 - dragOffset.y) / scale;
	for (int i = 0; i < numVertices; i++){
		float diffx = mousePosition.x - vertices[i].x;
		float diffy = mousePosition.y - vertices[i].y;
		float dist = sqrt(diffx*diffx + diffy*diffy);
		if (dist < vertices[i].radius){
			vertices[i].bOver = true;
		} else {
			vertices[i].bOver = false;
		}	
	}
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
    mousePosition.x = (x - ofGetWidth() / 2 - dragOffset.x) / scale;
    mousePosition.y = (y - ofGetHeight() / 2 - dragOffset.y) / scale;
    trackingRegion.clear();
	for (int i = 0; i < numVertices; i++){
		if (vertices[i].bBeingDragged == true){
			vertices[i].x = mousePosition.x;
			vertices[i].y = mousePosition.y;
            settings.setValue("settings:p"+ofToString(i)+":x",mousePosition.x);
            settings.setValue("settings:p"+ofToString(i)+":y",mousePosition.y);
		}
        trackingRegion.addVertex(vertices[i].x,vertices[i].y);
	}
    tracker.setRegion(trackingRegion);
    if(isDragging){
      dragOffset.x = x - dragStart.x;
      dragOffset.y = y - dragStart.y;
      settings.setValue("settings:drag-offset-x", dragOffset.x);
      settings.setValue("settings:drag-offset-y", dragOffset.y);
    }
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
    mousePosition.x = (x - ofGetWidth() / 2 - dragOffset.x) / scale;
    mousePosition.y = (y - ofGetHeight() / 2 - dragOffset.y) / scale;
    bool onVertice = false;
	for (int i = 0; i < numVertices; i++){
		float diffx = mousePosition.x - vertices[i].x;
		float diffy = mousePosition.y - vertices[i].y;
		float dist = sqrt(diffx*diffx + diffy*diffy);
		if (dist < vertices[i].radius){
			vertices[i].bBeingDragged = onVertice = true;
		} else {
			vertices[i].bBeingDragged = false;
		}	
	}
    if(!onVertice){
      isDragging = true;
      dragStart.x = x - dragOffset.x;
      dragStart.y = y - dragOffset.y;
    }
    
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
    isDragging = false;
	for (int i = 0; i < numVertices; i++){
		vertices[i].bBeingDragged = false;	
	}
}

