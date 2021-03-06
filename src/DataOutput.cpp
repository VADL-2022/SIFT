//
//  DataOutput.cpp
//  SIFT
//
//  Created by VADL on 11/3/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#include "DataOutput.hpp"

#include "opencv_highgui_common.hpp"
#include "utils.hpp"
#include "../common.hpp"

void PreviewWindowDataOutput::showCanvas(std::string name, cv::Mat& canvas, bool flush, cv::Rect* crop) {
    t.reset();
    cv::Mat temp = crop ? canvas(*crop) : canvas;
    commonUtils::imshow(name, temp);
    t.logElapsed("show canvas window");
}

int PreviewWindowDataOutput::waitKey(int delay) {
    //if (delay == 0) { delay = 1;  cv::waitKey(delay); return 'g'; }
    return cv::waitKey(delay);
}

FileDataOutput::FileDataOutput(std::string filenameNoExt_, double fps_, cv::Size sizeFrame_) :
fps(fps_), sizeFrame(sizeFrame_), filenameNoExt(filenameNoExt_) {}

void FileDataOutput::run(cv::Mat frame, bool flush, cv::Rect* crop) {
    writerMutex.lock();
    bool first = !writer.isOpened();
    if (first) {
        //int codec = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        int codec = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');

        bool isColor = true; //(frame.channels() > 1);
        //{ out_guard(); std::cout << "Output type: " << mat_type2str(frame.type()) << "\nisColor: " << isColor << std::endl; }
        std::string filename = openFileWithUniqueName(filenameNoExt, ".mp4");
        writer.open(filename, codec, fps, sizeFrame, isColor);
    }
    writerMutex.unlock();

    // Resize/convert, then write to video writer
    cv::Mat newFrame;
    cv::Mat* newFramePtr = &frame;
    { out_guard();
        std::cout << "frame.data: " << (void*)frame.data << std::endl; }
    if (crop && frame.cols > crop->width && frame.rows > crop->height) { // Crop only if not done so already..
        newFrame = (*newFramePtr)(*crop);
        newFramePtr = &newFrame;
    }
    if (newFramePtr->cols != sizeFrame.width || newFramePtr->rows != sizeFrame.height) {
        t.reset();
        cv::resize(*newFramePtr, newFrame, sizeFrame);
        newFramePtr = &newFrame;
        t.logElapsed("resize frame for video writer");
    }
    if (newFramePtr->type() != CV_8UC4 && newFramePtr->type() != CV_8UC3 && newFramePtr->type() != CV_8UC2 && newFramePtr->type() != CV_8UC1) {
        t.reset();
        newFramePtr->convertTo(newFrame, CV_8U, 255); // https://stackoverflow.com/questions/22174002/why-does-opencvs-convertto-function-not-work : need to scale the values up to char's range of 0-255
        newFramePtr = &newFrame;
        t.logElapsed("convert frame for video writer part 1");
    }
    if (newFramePtr->type() == CV_8UC1) {
        t.reset();
        { out_guard();
            std::cout << "Converting from " << mat_type2str(newFramePtr->type()) << std::endl; }
        // https://answers.opencv.org/question/66545/problems-with-the-video-writer-in-opencv-300/
        cv::cvtColor(*newFramePtr, newFrame, cv::COLOR_GRAY2BGR);
        newFramePtr = &newFrame;
        t.logElapsed("convert frame for video writer part 1.1");
    }
    else if (newFramePtr->type() == CV_8UC4) {
        t.reset();
        { out_guard();
            std::cout << "Converting from " << mat_type2str(newFramePtr->type()) << std::endl; }
        // https://answers.opencv.org/question/66545/problems-with-the-video-writer-in-opencv-300/
        cv::cvtColor(*newFramePtr, newFrame, cv::COLOR_RGBA2BGR);
        newFramePtr = &newFrame;
        t.logElapsed("convert frame for video writer part 2");
    }
    t.reset();
    { out_guard();
        std::cout << "newFrame.data: " << (void*)newFrame.data << "\n";
        std::cout << "Writing frame with type " << mat_type2str(newFramePtr->type()) << std::endl; }
    writerMutex.lock();
    writer.write(*newFramePtr);
    if (flush) {
        { out_guard();
            std::cout << "Flushing the video" << std::endl; }
        writer.release(); // We check again if not opened at the top of this function and then reopen the video so this is ok to do here.
        { out_guard();
            std::cout << "Flushed the video" << std::endl; }
    }
    writerMutex.unlock();
    t.logElapsed("write frame for video writer");
}

void FileDataOutput::release() {
    writerMutex.lock();
    if (!releasedWriter) {
        writer.release(); // Save the file
        { out_guard(); // NOTE: this may hang if called from a segfault handler (i.e. if segfault happens inside an out_guard(), but it's ok because it is done *after* the writer.release()... the rest is history. (cout can also have had a mutex locked..)
            std::cout << "Saved the video" << std::endl; }
        releasedWriter = true;
        writerMutex.unlock();
    }
    else {
        writerMutex.unlock();
        { out_guard(); // NOTE: this may hang if called from a segfault handler (i.e. if segfault happens inside an out_guard(), but it's ok because it is done *after* the writer.release()... the rest is history. (cout can also have had a mutex locked..)
            std::cout << "Video was saved already" << std::endl; }
    }
}

void FileDataOutput::showCanvas(std::string name, cv::Mat& canvas, bool flush, cv::Rect* crop) {
    static thread_local Timer t2;
    t2.reset();
    run(canvas, flush, crop);
    t2.logElapsed("save frame to FileDataOutput");
}

int FileDataOutput::waitKey(int delay) {
    // Advance to next image after user presses enter
    { out_guard();
        std::cout << "Press enter to advance to the next frame and get keypoints, g to load or make cached keypoints file, s to apply previous transformations, or q to quit: " << std::flush; }
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
