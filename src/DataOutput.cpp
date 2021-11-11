//
//  DataOutput.cpp
//  SIFT
//
//  Created by VADL on 11/3/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#include "DataOutput.hpp"

#include "opencv2/highgui.hpp"
#include <fstream>
#include "utils.hpp"

void PreviewWindowDataOutput::showCanvas(std::string name, cv::Mat& canvas) {
    t.reset();
    imshow(name, canvas);
    t.logElapsed("show canvas window");
}

int PreviewWindowDataOutput::waitKey(int delay) {
    return cv::waitKey(delay);
}

FileDataOutput::FileDataOutput(std::string filenameNoExt_, double fps_, cv::Size sizeFrame_) :
fps(fps_), sizeFrame(sizeFrame_), filenameNoExt(filenameNoExt_) {}

// Throws if fails
std::string openFileWithUniqueName(std::string name, std::string extension) {
  // Based on https://stackoverflow.com/questions/13108973/creating-file-names-automatically-c
  
  std::ofstream ofile;

  std::string fname;
  for(unsigned int n = 0; ; ++ n)
    {
      fname = name + std::to_string(n) + extension;

      std::ifstream ifile;
      ifile.open(fname.c_str());

      if(ifile.is_open())
	{
	}
      else
	{
	  ofile.open(fname.c_str());
	  break;
	}

      ifile.close();
    }

  if(!ofile.is_open())
    {
      //return "";
      throw "";
    }

  return fname;
}

void FileDataOutput::run(cv::Mat frame) {
    bool first = !writer.isOpened();
    if (first) {
        int codec = cv::VideoWriter::fourcc('a', 'v', 'c', '1');

        bool isColor = (frame.channels() > 1);
        std::cout << "Output type: " << mat_type2str(frame.type()) << "\nisColor: " << isColor << std::endl;
        std::string filename = openFileWithUniqueName(filenameNoExt, ".mp4");
        writer.open(filename, codec, fps, sizeFrame, isColor);
    }

    cv::Mat newFrame;
    if (frame.cols != sizeFrame.width || frame.rows != sizeFrame.height) {
        t.reset();
        cv::resize(frame, newFrame, sizeFrame);
        t.logElapsed("resize frame for video writer");
    }
    if (frame.depth() != CV_8U) {
        t.reset();
        newFrame.convertTo(newFrame, CV_8U);
        t.logElapsed("convert frame for video writer");
    }
    t.reset();
    std::cout << "Writing frame with type " << mat_type2str(newFrame.type()) << std::endl;
    writer.write(newFrame);
    t.logElapsed("write frame for video writer");
}

void FileDataOutput::showCanvas(std::string name, cv::Mat& canvas) {
    t.reset();
    run(canvas);
    t.logElapsed("save frame to FileDataOutput");
}

int FileDataOutput::waitKey(int delay) {
    // Advance to next image after user presses enter
    std::cout << "Press enter to advance to the next frame and get keypoints, g to load or make cached keypoints file, s to apply previous transformations, or q to quit: " << std::flush;
//        std::string n;
//        std::getline(std::cin, n);
    char n = getchar();
    
    if (n == '\n') {
        return 'd'; // Simply advance to next image
    }
    else {
        return n; // The command given
    }
}
