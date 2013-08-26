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
  
    bool isDragging;
    ofVec2f dragOffset, dragStart;
	    
    int numVertices, minClusterSize, maxPointDistance;
    unsigned int maxClusterCount;
    float scale, maxStddev;
    draggableVertex vertices[4];
	
	ofxSickGrabber grabber;
	ofxSick* sick;
  
    vector<cv::Point2f> clusters;
  
    int mX, mY;

    ofxUICanvas *gui; 	
  
	ofxSickTracker<ofxSickFollower> tracker;
	ofPolyline trackingRegion;
};