#include "testApp.h"

using namespace cv;
using namespace ofxCv;

void testApp::setup() {
	ofSetVerticalSync(true);
	ofSetFrameRate(120);
	ofSetCircleResolution(64);
	ofSetLogLevel(OF_LOG_VERBOSE);
  
    scale = 0.15;
  
    float dim = 24; 
	float xInit = OFX_UI_GLOBAL_WIDGET_SPACING; 
    float length = 320-xInit; 

    gui = new ofxUICanvas(0, 0, length+xInit, ofGetHeight());
    gui->addWidgetDown(new ofxUILabel("SETTINGS", OFX_UI_FONT_LARGE)); 
    gui->addSlider("SCALE", 0.0, 1.0, scale, length-xInit,dim);
    ofAddListener(gui->newGUIEvent,this,&testApp::guiEvent);	
	
	grabber.setup();
  
	sick = &grabber;
  
    numVertices = 4;
  	vertices[0].x = 1200;
	vertices[0].y = -800;
	vertices[1].x = 1200;
	vertices[1].y = 800;
	vertices[2].x = 2400;
	vertices[2].y = 800;
	vertices[3].x = 2400;
	vertices[3].y = -800;
    for (int i = 0; i < numVertices; i++){
		vertices[i].bOver = false;
		vertices[i].bBeingDragged = false;
		vertices[i].radius = 40;
        trackingRegion.addVertex(vertices[i].x,vertices[i].y);
	}
    tracker.setRegion(trackingRegion);
	tracker.setMaximumDistance(100);
	tracker.setPersistence(10);
}

void testApp::update() {
	sick->update();
	if(sick->isFrameNew()) {
		tracker.update(*sick);
	}
}

void testApp::exit()
{
	delete gui; 
}

void testApp::guiEvent(ofxUIEventArgs &e)
{
    string name = e.widget->getName();
	if(name == "SCALE")
	{
		ofxUISlider *slider = (ofxUISlider *) e.widget;
		scale = slider->getScaledValue();
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
  	ofTranslate(ofGetWidth() / 2, ofGetHeight() / 2);
	ofScale(scale, scale);
  
	sick->draw(12, 2400);
	tracker.draw();
  
    ofNoFill();
    ofSetColor(255);
    for (int i = 0; i < numVertices; i++){
        if (vertices[i].bOver == true) ofFill();
        else ofNoFill();
        ofCircle(vertices[i].x, vertices[i].y,vertices[i].radius);
    }
	ofPopMatrix();
}

//------------- -------------------------------------------------
void testApp::mouseMoved(int x, int y ){
    int mX = (x - ofGetWidth() / 2) / scale;
    int mY = (y - ofGetHeight() / 2) / scale;
	for (int i = 0; i < numVertices; i++){
		float diffx = mX - vertices[i].x;
		float diffy = mY - vertices[i].y;
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
    int mX = (x - ofGetWidth() / 2) / scale;
    int mY = (y - ofGetHeight() / 2) / scale;
    trackingRegion.clear();
	for (int i = 0; i < numVertices; i++){
		if (vertices[i].bBeingDragged == true){
			vertices[i].x = mX;
			vertices[i].y = mY;
		}
        trackingRegion.addVertex(vertices[i].x,vertices[i].y);
	}
    tracker.setRegion(trackingRegion);
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
    int mX = (x - ofGetWidth() / 2) / scale;
    int mY = (y - ofGetHeight() / 2) / scale;
	for (int i = 0; i < numVertices; i++){
		float diffx = mX - vertices[i].x;
		float diffy = mY - vertices[i].y;
		float dist = sqrt(diffx*diffx + diffy*diffy);
		if (dist < vertices[i].radius){
			vertices[i].bBeingDragged = true;
		} else {
			vertices[i].bBeingDragged = false;
		}	
	}
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

	for (int i = 0; i < numVertices; i++){
		vertices[i].bBeingDragged = false;	
	}
}

