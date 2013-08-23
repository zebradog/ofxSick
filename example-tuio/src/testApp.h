#pragma once

#include "ofMain.h"
#include "ofxSick.h"
#include "ofxSickTracker.h"
#include "ofxCv.h"
#include "ofxUI.h"

typedef struct {

	float 	x;
	float 	y;
	bool 	bBeingDragged;
	bool 	bOver;
	float 	radius;
	
}draggableVertex;

class testApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	
    void mouseMoved(int x, int y );
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
  
    void exit(); 
    void guiEvent(ofxUIEventArgs &e);
	    
    int numVertices;
    float scale;
    draggableVertex vertices[4];
	
	ofxSickGrabber grabber;
	ofxSick* sick;

    ofxUICanvas *gui; 	
  
	ofxSickTracker<ofxSickFollower> tracker;
	ofPolyline trackingRegion;
};