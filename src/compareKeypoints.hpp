//
//  compareKeypoints.hpp
//  SIFT
//
//  Created by VADL on 10/31/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#ifndef compareKeypoints_hpp
#define compareKeypoints_hpp

#include "common.hpp"
#include "DataOutput.hpp"

// Forward declarations
struct SIFTState;
struct SIFTParams;

// Returns true to mean you need to retry with the modified params (modified by this function) on the previous file.
bool compareKeypoints(PreviewWindowDataOutput& o, SIFTState& s, SIFTParams& p, struct sift_keypoints* keypoints, cv::Mat& backtorgb);

#endif /* compareKeypoints_hpp */
