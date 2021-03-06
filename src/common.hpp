//
//  common.hpp
//  SIFT
//
//  Created by VADL on 10/31/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#ifndef common_hpp
#define common_hpp

#include "Includes.hpp"

#include "Config.hpp"

// Each thread needs its own timer for meaningful results, so we make it thread_local.
// https://stackoverflow.com/questions/11983875/what-does-the-thread-local-mean-in-c11 : "When you declare a variable thread_local then each thread has its own copy. When you refer to it by name, then the copy associated with the current thread is used."
extern thread_local Timer t;

void drawCircle(cv::Mat& img, cv::Point cp, int radius);
void drawRect(cv::Mat& img, cv::Point center, cv::Size2f size, float orientation_degrees, int thickness = 2);
void drawSquare(cv::Mat& img, cv::Point center, int size, float orientation_degrees, int thickness = 2);

// Wrapper around imshow that reuses the same window each time
namespace commonUtils {
void imshow(std::string name, cv::Mat& mat);
}

extern thread_local cv::RNG rng; // Random number generator
extern thread_local cv::Scalar lastColor;
void resetRNG();
cv::Scalar nextRNGColor(int matDepth);

#endif /* common_hpp */
