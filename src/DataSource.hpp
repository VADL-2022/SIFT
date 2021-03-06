//
//  DataSource.hpp
//  SIFT
//
//  Created by VADL on 11/2/21.
//  Copyright © 2021 VADL. All rights reserved.
//

#ifndef DataSource_hpp
#define DataSource_hpp

#include <string>
#include <vector>
#include "opencv_common.hpp"
#include <utility>
#include <unordered_map>
#include "Config.hpp"

// Note: cv::Mat is reference counted automatically

struct DataSourceBase {
    #ifdef USE_COMMAND_LINE_ARGS
    virtual ~DataSourceBase() {}
    #endif
    
    MaybeVirtual bool hasNext() MaybePureVirtual;
    MaybeVirtual cv::Mat next() MaybePureVirtual;
    MaybeVirtual cv::Mat get(size_t index) MaybePureVirtual;
    MaybeVirtual std::string nameForIndex(size_t index) MaybePureVirtual;
    MaybeVirtual float* dataForMat(size_t index) MaybePureVirtual;
    
    MaybeVirtual cv::Mat siftImageForMat(size_t index) MaybePureVirtual;
    MaybeVirtual cv::Mat colorImageForMat(size_t index) MaybePureVirtual;
    
    MaybeVirtual bool wantsCustomFPS() const MaybePureVirtual;
    MaybeVirtual double fps() const MaybePureVirtual;
    MaybeVirtual double timeMilliseconds() const MaybePureVirtual;
    
    MaybeVirtual bool shouldCrop() const;
    MaybeVirtual cv::Rect crop() const;
    
    size_t currentIndex; // Index to save into next
    bool _crop = false;
    cv::Rect _cropRect; // Only used if _crop is true
protected:
    void initCrop(cv::Size sizeFrame);
};

// Gets data from a stream like a queue or something. But the data can come from an arbitrary function: getNextInStream_, which should return a color cv::Mat of type CV_8UC3 and of RGB format.
struct StreamDataSource : public DataSourceBase
{
    StreamDataSource(std::function<cv::Mat(void)> getNextInStream_);
    ~StreamDataSource() {};
    
    bool hasNext();
    cv::Mat next();
    
    cv::Mat get(size_t index);
    
    std::string nameForIndex(size_t index);
    float* dataForMat(size_t index) { return (float*)get(index).data; }
    cv::Mat siftImageForMat(size_t index);
    cv::Mat colorImageForMat(size_t index);
    
    bool wantsCustomFPS() const { return false; }
    double fps() const { return DBL_MAX; }
    double timeMilliseconds() const { return 0; }
    
//    bool shouldCrop() const { return false; }
//    cv::Rect crop() const { return cv::Rect(); }
    
    std::unordered_map<size_t, cv::Mat> cache;
protected:
    std::function<cv::Mat(void)> getNextInStream;
    cv::Size sizeFrame;
    bool inittedCrop = false;
};

struct SharedMemoryDataSource : public StreamDataSource
{
    SharedMemoryDataSource(int fd);
    ~SharedMemoryDataSource() {};
    
    // Inherited: //
//    bool hasNext();
//    cv::Mat next();
//
//    cv::Mat get(size_t index);
//
//    std::string nameForIndex(size_t index);
//    float* dataForMat(size_t index) { return (float*)get(index).data; }
//    cv::Mat siftImageForMat(size_t index);
//    cv::Mat colorImageForMat(size_t index);
    // //
    
    bool wantsCustomFPS() const { return false; }
    double fps() const { return DBL_MAX; }
    double timeMilliseconds() const { return 0; }
private:
    void* shmem; size_t shmem_size; int shmemFD; // Shared memory from some source like a parent process etc.
    uint64_t lastShmemFrameCounter = std::numeric_limits<uint64_t>::max(); // Frame counter (may/likely use(s) different indexing starting index from our `currentIndex`) of the last image grabbed from shared memory.
};

struct FolderDataSource : public DataSourceBase
{
    FolderDataSource(std::string folderPath, size_t skip);
    FolderDataSource(int argc, char** argv, size_t skip);
    ~FolderDataSource();
    
    bool hasNext();
    cv::Mat next();
    
    cv::Mat get(size_t index);
    
    std::string nameForIndex(size_t index);
    float* dataForMat(size_t index) { return (float*)get(index).data; }
    cv::Mat siftImageForMat(size_t index);
    cv::Mat colorImageForMat(size_t index);
    
    bool wantsCustomFPS() const { return false; }
    double fps() const { return DBL_MAX; }
    double timeMilliseconds() const { return 0; }
    
//    bool shouldCrop() const { return false; }
//    cv::Rect crop() const { return cv::Rect(); }
    
    std::unordered_map<size_t, cv::Mat> cache;
//private:
    std::vector<std::string> files;
private:
    cv::Size sizeFrame;
    bool inittedCrop = false;
    
    void init(std::string folderPath);
};

struct OpenCVVideoCaptureDataSource : public DataSourceBase
{
    OpenCVVideoCaptureDataSource();
    
    bool hasNext() { return cap.isOpened(); }
    cv::Mat next();
    
    cv::Mat get(size_t index);
    
    std::string nameForIndex(size_t index);
    float* dataForMat(size_t index) { return (float*)get(index).data; }
    cv::Mat siftImageForMat(size_t index);
    cv::Mat colorImageForMat(size_t index);
    
    bool wantsCustomFPS() const { return wantedFPS.has_value(); }
    double fps() const { return wantsCustomFPS() ? wantedFPS.value() : cap.get(cv::CAP_PROP_FPS); }
    double timeMilliseconds() const { return cap.get(cv::CAP_PROP_POS_MSEC); }
    
//    bool shouldCrop() const;
//    cv::Rect crop() const;
    
protected:
    std::optional<double> wantedFPS;
    
    static const double default_fps;
    
    cv::VideoCapture cap;
    std::unordered_map<size_t, cv::Mat> cache;
};

struct CameraDataSource : public OpenCVVideoCaptureDataSource
{
    CameraDataSource();
    CameraDataSource(int argc, char** argv);
    
    bool hasNext();
    
    using OpenCVVideoCaptureDataSource::get;
    
    static const cv::Size default_sizeFrame;
protected:
    void init(double fps, cv::Size sizeFrame);
};

struct VideoFileDataSource : public OpenCVVideoCaptureDataSource
{
    VideoFileDataSource();
    VideoFileDataSource(int argc, char** argv);
    
    cv::Mat get(size_t index);
    
private:
    double frame_rate;
    double frame_msec;
    double video_time;
protected:
    bool readBackwards = true;
    static const std::string default_filePath;
    void init(std::string filePath);
};


// Parital template specialization. But maybe use this: https://stackoverflow.com/questions/31500426/why-does-enable-if-t-in-template-arguments-complains-about-redefinitions
template<class T>
extern T makeDataSource(int argc, char** argv, size_t skip) { throw 0; }
template<>
FolderDataSource makeDataSource(int argc, char** argv, size_t skip);
template<>
CameraDataSource makeDataSource(int argc, char** argv, size_t skip);
template<>
VideoFileDataSource makeDataSource(int argc, char** argv, size_t skip);

#endif /* DataSource_hpp */
