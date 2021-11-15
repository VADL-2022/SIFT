//
//  siftMain.cpp
//  (Originally example2.cpp)
//  SIFT
//
//  Created by VADL on 9/3/21.
//  Copyright © 2021 VADL. All rights reserved.
//
// This file runs on the SIFT RPis. It can also run on your computer. You can optionally show a preview window for fine-grain control.

#include "common.hpp"

#include "KeypointsAndMatching.hpp"
#include "compareKeypoints.hpp"
#include "DataSource.hpp"
#include "DataOutput.hpp"
#include "utils.hpp"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifndef USE_COMMAND_LINE_ARGS
#include "Queue.hpp"
#include "lib/ctpl_stl.hpp"
SIFT_T sift;
Queue<ProcessedImage<SIFT_T>, 256 /*32*/> processedImageQueue;
cv::Mat lastImageToFirstImageTransformation; // "Message box" for the accumulated transformations so far
#endif

//// https://docs.opencv.org/master/da/d6a/tutorial_trackbar.html
//const int alpha_slider_max = 2;
//int alpha_slider = 0;
//double alpha;
//double beta;
//static void on_trackbar( int, void* )
//{
//   alpha = (double) alpha_slider/alpha_slider_max ;
//}

// Config //
// Data source
//using DataSourceT = FolderDataSource;
using DataSourceT = CameraDataSource;

// Data output
using DataOutputT = PreviewWindowDataOutput;
// //

template <typename DataSourceT, typename DataOutputT>
int mainInteractive(DataSourceT* src,
                    SIFTState& s,
                    SIFTParams& p,
                    size_t skip,
                    DataOutputT& o
                    #ifdef USE_COMMAND_LINE_ARGS
                      , FileDataOutput& o2,
                      const CommandLineConfig& cfg
                    #endif
);

template <typename DataSourceT, typename DataOutputT>
int mainMission(DataSourceT* src,
                SIFTParams& p,
                DataOutputT& o
                );

int main(int argc, char **argv)
{
    SIFTState s;
    SIFTParams p;
    
	// Set the default "skip"
    size_t skip = 0;//120;//60;//100;//38;//0;
    DataOutputT o;
    
    // Command-line args //
#ifdef USE_COMMAND_LINE_ARGS
    CommandLineConfig cfg;
    FileDataOutput o2("dataOutput/live", 1.0 /* fps */ /*, sizeFrame */);
    std::unique_ptr<DataSourceBase> src;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--folder-data-source") == 0) { // Get from folder instead of camera
            src = std::make_unique<FolderDataSource>(argc, argv, skip); // Read folder determined by command-line arguments
            cfg.folderDataSource = true;
        }
        else if (strcmp(argv[i], "--image-capture-only") == 0) { // For not running SIFT
            cfg.imageCaptureOnly = true;
        }
        else if (strcmp(argv[i], "--image-file-output") == 0) { // Outputs to video instead of preview window
            cfg.imageFileOutput = true;
        }
        else if (strcmp(argv[i], "--main-mission") == 0) {
            cfg.mainMission = true;
        }
        else if (i+1 < argc && strcmp(argv[i], "--sift-params") == 0) { // Outputs to video instead of preview window
            cfg.imageFileOutput = true;
            if (SIFTParams::call_params_function(argv[i+1], p.params) < 0) {
                printf("Invalid params function name given: %s. Value options are:", argv[i+1]);
                SIFTParams::print_params_functions();
                printf(". Exiting.\n");
                exit(1);
            }
            else {
                printf("Using params function %s\n", argv[i+1]);
            }
            i++;
        }
    }
    
    if (!cfg.folderDataSource) {
        src = std::make_unique<DataSourceT>();
    }
    
    if (!cfg.mainMission) {
        p.params = sift_assign_default_parameters();
        mainInteractive(src.get(), s, p, skip, o, o2, cfg);
    }
    else {
        mainMission(src, p, o2);
    }
#else
    DataSourceT src_ = makeDataSource<DataSourceT>(argc, argv, skip); // Read folder determined by command-line arguments
    DataSourceT* src = &src_;
    
    FileDataOutput o2("dataOutput/live", 1.0 /* fps */ /*, sizeFrame */);
    mainMission(src, p, o2);
#endif
    // //
    
	return 0;
}

// https://stackoverflow.com/questions/2808398/easily-measure-elapsed-time
template <
    class result_t   = std::chrono::milliseconds,
    class clock_t    = std::chrono::steady_clock,
    class duration_t = std::chrono::milliseconds
>
auto since(std::chrono::time_point<clock_t, duration_t> const& start)
{
    return std::chrono::duration_cast<result_t>(clock_t::now() - start);
}

void* matcherThreadFunc(void* arg) {
    std::atomic<bool>& isStop = *(std::atomic<bool>*)arg;
    do {
        ProcessedImage<SIFT_T> img1, img2;
        std::cout << "Matcher thread: Locking for dequeueOnceOnTwoImages" << std::endl;
        pthread_mutex_lock( &processedImageQueue.mutex );
        while( processedImageQueue.count <= 1 ) // Wait until not empty.
        {
            pthread_cond_wait( &processedImageQueue.condition, &processedImageQueue.mutex );
            std::cout << "Matcher thread: Unlocking for dequeueOnceOnTwoImages 2" << std::endl;
            pthread_mutex_unlock( &processedImageQueue.mutex );
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::cout << "Matcher thread: Locking for dequeueOnceOnTwoImages 2" << std::endl;
            pthread_mutex_lock( &processedImageQueue.mutex );
        }
        std::cout << "Matcher thread: Unlocking for dequeueOnceOnTwoImages 3" << std::endl;
        pthread_mutex_unlock( &processedImageQueue.mutex );
        std::cout << "Matcher thread: Locking for dequeueOnceOnTwoImages 3" << std::endl;
        processedImageQueue.dequeueOnceOnTwoImages(&img1, &img2);
        std::cout << "Matcher thread: Unlocked for dequeueOnceOnTwoImages" << std::endl;
        
        // Setting current parameters for matching
        img2.applyDefaultMatchingParams();

        // Matching and homography
        t.reset();
        sift.findHomography(img1, img2);
        // Accumulate homography
        if (lastImageToFirstImageTransformation.empty()) {
            lastImageToFirstImageTransformation = img2.transformation.inv();
        }
        else {
            lastImageToFirstImageTransformation *= img2.transformation.inv();
        }
        t.logElapsed("find homography");
        
        end:
        // Free memory and mark this as an unused ProcessedImage:
        img2.resetMatching();
    } while (!isStop);
    return (void*)0;
}

ctpl::thread_pool tp(4); // Number of threads in the pool
// ^^ Note: "the destructor waits for all the functions in the queue to be finished" (or call .stop())
void ctrlC(int s){
    printf("Caught signal %d. Threads are stopping...\n",s);
    tp.isStop = true;
}
template <typename DataSourceT, typename DataOutputT>
int mainMission(DataSourceT* src,
                SIFTParams& p,
                DataOutputT& o2
) {
    // Install ctrl-c handler
    // https://stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = ctrlC;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    
    cv::Mat firstImage;
    cv::Mat prevImage;
    pthread_t matcherThread;
    pthread_create(&matcherThread, NULL, matcherThreadFunc, &tp.isStop);
    
    auto last = std::chrono::steady_clock::now();
    auto fps = src->fps();
    const long long timeBetweenFrames_milliseconds = 1/fps * 1000;
    std::cout << "Target fps: " << fps << std::endl;
    //std::atomic<size_t> offset = 0; // Moves back the indices shown to SIFT threads
    for (size_t i = src->currentIndex; !tp.isStop; i++) {
        std::cout << "i: " << i << std::endl;
        if (src->wantsCustomFPS()) {
            auto sinceLast_milliseconds = since(last).count();
            if (sinceLast_milliseconds < timeBetweenFrames_milliseconds) {
                // Sleep
                auto sleepTime = timeBetweenFrames_milliseconds - sinceLast_milliseconds;
                std::cout << "Sleeping for " << sleepTime << " milliseconds" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            }
            last = std::chrono::steady_clock::now();
        }
        t.reset();
        cv::Mat mat = src->get(i);
        std::cout << "CAP_PROP_POS_MSEC: " << src->timeMilliseconds() << std::endl;
        t.logElapsed("get image");
        if (mat.empty()) { printf("No more images left to process. Exiting.\n"); break; }
        o2.showCanvas("", mat);
        t.reset();
        cv::Mat greyscale = src->siftImageForMat(i);
        t.logElapsed("siftImageForMat");
        //auto path = src->nameForIndex(i);
        
        std::cout << "Pushing function to thread pool, currently has " << tp.n_idle() << " idle thread(s) and " << tp.q.size() << " function(s) queued" << std::endl;
        tp.push([&pOrig=p](int id, /*extra args:*/ size_t i, cv::Mat greyscale) {
            std::cout << "hello from " << id << std::endl;

            SIFTParams p(pOrig); // New version of the params we can modify (separately from the other threads)
            p.params = sift_assign_default_parameters();
            v2Params(p.params);

            // compute sift keypoints
#if SIFT_IMPL == SIFTAnatomy
            auto pair = sift.findKeypoints(id, p, greyscale);
            int n = pair.second.second; // Number of keypoints
            struct sift_keypoints* keypoints = pair.first;
            struct sift_keypoint_std *k = pair.second.first;
#elif SIFT_IMPL == SIFTOpenCV
            auto vec = sift.findKeypoints(id, p, greyscale);
#endif
            
            t.reset();
            bool isFirstSleep = true;
            do {
                if (isFirstSleep) {
                    std::cout << "Thread " << id << ": Locking" << std::endl;
                }
                pthread_mutex_lock( &processedImageQueue.mutex );
                if (isFirstSleep) {
                    std::cout << "Thread " << id << ": Locked" << std::endl;
                }
                if ((i == 0 && processedImageQueue.count == 0) // Edge case for first image
                    || (processedImageQueue.readPtr == i - 1) // If we're writing right after the last element, can enqueue our sequentially ordered image
                    ) {
                    std::cout << "Thread " << id << ": enqueue" << std::endl;
                    #if SIFT_IMPL == SIFTAnatomy
                    processedImageQueue.enqueueNoLock(greyscale,
                                            shared_keypoints_ptr(keypoints),
                                            shared_keypoints_ptr(),
                                            shared_keypoints_ptr(),
                                            shared_keypoints_ptr(),
                                            cv::Mat(),
                                            p,
                                            i);
                    #elif SIFT_IMPL == SIFTOpenCV
                    processedImageQueue.enqueueNoLock(greyscale,
                                            vec
                                            cv::Mat());
                    #endif
                }
                else {
                    auto ms = 10;
                    if (isFirstSleep) {
                        std::cout << "Thread " << id << ": Unlocking" << std::endl;
                    }
                    pthread_mutex_unlock( &processedImageQueue.mutex );
                    if (isFirstSleep) {
                        std::cout << "Thread " << id << ": Unlocked" << std::endl;
                        std::cout << "Thread " << id << ": Sleeping " << ms << " milliseconds at least once (and possibly locking and unlocking a few times more)..." << std::endl;
                        isFirstSleep = false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                    continue;
                }
                std::cout << "Thread " << id << ": Unlocking 2" << std::endl;
                pthread_mutex_unlock( &processedImageQueue.mutex );
                std::cout << "Thread " << id << ": Unlocked 2" << std::endl;
                break;
            } while (true);
            t.logElapsed(id, "enqueue processed image");

            end:
            // cleanup
            free(k);
        }, /*extra args:*/ i /*- offset*/, greyscale);
        
        // Save this image for next iteration
        prevImage = mat;
        
        if (firstImage.empty()) { // This is the first iteration.
            // Save firstImage once
            firstImage = mat;
        }
    }
    
    o2.writer.release(); // Save the file
    tp.isStop = true;
    int ret1;
    pthread_join(matcherThread, (void**)&ret1);
    
    tp.stop();
    
    // Print the final homography
    cv::Mat& M = lastImageToFirstImageTransformation;
    cv::Ptr<cv::Formatter> fmt = cv::Formatter::get(cv::Formatter::FMT_DEFAULT);
    fmt->set64fPrecision(4);
    fmt->set32fPrecision(4);
    auto str = fmt->format(M);
    std::cout << str << std::endl;
    
    // Save final homography to an image
    cv::Mat canvas;
    cv::warpPerspective(firstImage, canvas /* <-- destination */, M, firstImage.size());
    auto name = openFileWithUniqueName("dataOutput/scaled", ".png");
    std::cout << "Saving to " << name << std::endl;
    cv::imwrite(name, canvas);
    return 0;
}

#ifdef USE_COMMAND_LINE_ARGS
template <typename DataSourceT, typename DataOutputT>
int mainInteractive(DataSourceT* src,
                    SIFTState& s,
                    SIFTParams& p,
                    size_t skip,
                    DataOutputT& o
                    #ifdef USE_COMMAND_LINE_ARGS
                      , FileDataOutput& o2,
                      const CommandLineConfig& cfg
                    #endif
) {
    cv::Mat canvas;
    
	//--- print the files sorted by filename
    bool retryNeeded = false;
    for (size_t i = src->currentIndex;; i++) {
        std::cout << "i: " << i << std::endl;
        cv::Mat mat = src->get(i);
        if (mat.empty()) { printf("No more images left to process. Exiting.\n"); break; }
        cv::Mat greyscale = src->siftImageForMat(i);
        float* x = (float*)greyscale.data;
        size_t w = mat.cols, h = mat.rows;
        auto path = src->nameForIndex(i);

        // Initialize canvas if needed
        if (canvas.data == nullptr) {
            puts("Init canvas");
            canvas = cv::Mat(h, w, CV_32FC4);
        }

		// compute sift keypoints
		int n; // Number of keypoints
		struct sift_keypoints* keypoints;
		struct sift_keypoint_std *k;
        if (!CMD_CONFIG(imageCaptureOnly)) {
            if (s.loadedKeypoints == nullptr) {
                t.reset();
                k = my_sift_compute_features(p.params, x, w, h, &n, &keypoints);
                printf("Number of keypoints: %d\n", n);
                t.logElapsed("compute features");
            }
            else {
                k = s.loadedK.release(); // "Releases the ownership of the managed object if any." ( https://en.cppreference.com/w/cpp/memory/unique_ptr/release )
                keypoints = s.loadedKeypoints.release();
                n = s.loadedKeypointsSize;
            }
        }

        cv::Mat backtorgb = src->colorImageForMat(i);
		if (i == skip) {
			puts("Init firstImage");
            s.firstImage = backtorgb;
		}

		// Draw keypoints on `o.canvas`
        if (!CMD_CONFIG(imageCaptureOnly)) {
            t.reset();
        }
        backtorgb.copyTo(canvas);
        if (!CMD_CONFIG(imageCaptureOnly)) {
            for(int i=0; i<n; i++){
                drawSquare(canvas, cv::Point(k[i].x, k[i].y), k[i].scale, k[i].orientation, 1);
                //break;
                // fprintf(f, "%f %f %f %f ", k[i].x, k[i].y, k[i].scale, k[i].orientation);
                // for(int j=0; j<128; j++){
                // 	fprintf(f, "%u ", k[i].descriptor[j]);
                // }
                // fprintf(f, "\n");
            }
            t.logElapsed("draw keypoints");
        }

		// Compare keypoints if we had some previously and render to canvas if needed
        if (!CMD_CONFIG(imageCaptureOnly)) {
            retryNeeded = compareKeypoints(canvas, s, p, keypoints, backtorgb COMPARE_KEYPOINTS_ADDITIONAL_ARGS);
        }

        bool exit;
        if (CMD_CONFIG(imageFileOutput)) {
            #ifdef USE_COMMAND_LINE_ARGS
            exit = run(o2, canvas, *src, s, p, backtorgb, keypoints, retryNeeded, i, n);
            #endif
        }
        else {
            exit = run(o, canvas, *src, s, p, backtorgb, keypoints, retryNeeded, i, n);
        }
	
		// write to standard output
		//sift_write_to_file("/dev/stdout", k, n);

		// cleanup
        free(k);
		
		// Save keypoints
        if (!retryNeeded) {
            s.computedKeypoints.push_back(keypoints);
        }
        
        if (exit) {
            break;
        }
		
		// Reset RNG so some colors coincide
		resetRNG();
	}
    
    if (CMD_CONFIG(imageFileOutput)) {
        #ifdef USE_COMMAND_LINE_ARGS
        o2.writer.release(); // Save the file
        #endif
    }
    
    return 0;
}
#endif // USE_COMMAND_LINE_ARGS
