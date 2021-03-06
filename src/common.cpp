//
//  common.cpp
//  SIFT
//
//  Created by VADL on 10/31/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#include "common.hpp"

thread_local Timer t;

#pragma mark Drawing

void drawCircle(cv::Mat& img, cv::Point cp, int radius)
{
    //cv::Scalar black( 0, 0, 0 );
    cv::Scalar color = nextRNGColor(img.depth());
    
    cv::circle( img, cp, radius, color );
}
void drawRect(cv::Mat& img, cv::Point center, cv::Size2f size, float orientation_degrees, int thickness) {
    cv::RotatedRect rRect = cv::RotatedRect(center, size, orientation_degrees);
    cv::Point2f vertices[4];
    rRect.points(vertices);
    cv::Scalar color = nextRNGColor(img.depth());
    //printf("%f %f %f\n", color[0], color[1], color[2]);
    for (int i = 0; i < 4; i++)
        cv::line(img, vertices[i], vertices[(i+1)%4], color, thickness);
    
    // To draw a rectangle bounding this one:
    //Rect brect = rRect.boundingRect();
    //cv::rectangle(img, brect, nextRNGColor(img.depth()), 2);
}
void drawSquare(cv::Mat& img, cv::Point center, int size, float orientation_degrees, int thickness) {
    drawRect(img, center, cv::Size2f(size, size), orientation_degrees, thickness);
}

#pragma mark imshow

namespace commonUtils {
// Wrapper around imshow that reuses the same window each time
void imshow(std::string name, cv::Mat& mat) {
    static std::optional<std::string> prevWindowTitle;
    
    if (prevWindowTitle) {
        // Rename window
        cv::setWindowTitle(*prevWindowTitle, name);
    }
    else {
        prevWindowTitle = name;
        cv::namedWindow(name, cv::WINDOW_AUTOSIZE); // Create Window
        
//        // Set up window
//        char TrackbarName[50];
//        sprintf( TrackbarName, "Alpha x %d", alpha_slider_max );
//        cv::createTrackbar( TrackbarName, name, &alpha_slider, alpha_slider_max, on_trackbar );
//        on_trackbar( alpha_slider, 0 );
    }
    cv::imshow(*prevWindowTitle, mat); // prevWindowTitle (the first title) is always used as an identifier for this window, regardless of the renaming done via setWindowTitle().
}
}

#pragma mark RNG

// Based on https://stackoverflow.com/questions/31658132/c-opencv-not-drawing-circles-on-mat-image and https://stackoverflow.com/questions/19400376/how-to-draw-circles-with-random-colors-in-opencv/19401384
#define RNG_SEED 12345
thread_local cv::RNG rng(RNG_SEED); // Random number generator
thread_local cv::Scalar lastColor;
void resetRNG() {
    rng = cv::RNG(RNG_SEED);
}
cv::Scalar nextRNGColor(int matDepth) {
    //return cv::Scalar(0,0,255); // BGR color value (not RGB)
    
    if (matDepth == CV_8U) {
        lastColor = cv::Scalar(rng.uniform(0,255), rng.uniform(0, 255), rng.uniform(0, 255)); // Most of these are rendered as close to white for some reason..
    }
    else if (matDepth == CV_32F || matDepth == CV_64F) {
        lastColor = cv::Scalar(rng.uniform(0, 255) / rng.uniform(1, 255),
                       rng.uniform(0, 255) / rng.uniform(1, 255),
                       rng.uniform(0, 255) / rng.uniform(1, 255));
    }
    else {
        throw "Unsupported matDepth";
    }
    return lastColor;
}
