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

#ifndef USE_COMMAND_LINE_ARGS
#include "Queue.hpp"
#include "lib/ctpl_stl.hpp"
struct ProcessedImage {
    cv::Mat image;
    
    unique_keypoints_ptr computedKeypoints;
    
    // Matching with the previous image, if any //
    unique_keypoints_ptr out_k1;
    unique_keypoints_ptr out_k2A;
    unique_keypoints_ptr out_k2B;
    // //
    
    SIFTParams p;
};
Queue<ProcessedImage, 32> processedImageQueue;
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
    p.params = sift_assign_default_parameters();
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
    
    mainInteractive(&src, s, p, skip, o, o2, cfg);
#else
    // Set params (will use default if none set) //
    //USE(v3Params);
    //USE(v2Params);
    // //
    
    DataSourceT src_ = makeDataSource<DataSourceT>(argc, argv, skip); // Read folder determined by command-line arguments
    DataSourceT* src = &src_;
    
    mainMission(src, p, o);
#endif
    // //
    
	return 0;
}

template <typename DataSourceT, typename DataOutputT>
int mainMission(DataSourceT* src,
                SIFTParams& p,
                DataOutputT& o
) {
    cv::Mat firstImage;
    cv::Mat prevImage;
    struct sift_keypoints* keypointsPrev;
    ctpl::thread_pool tp(4); // Number of threads in the pool
    // ^^ Note: "the destructor waits for all the functions in the queue to be finished" (or call .stop())
    
    for (size_t i = src->currentIndex;; i++) {
        std::cout << "i: " << i << std::endl;
        t.reset();
        cv::Mat mat = src->get(i);
        t.logElapsed("get image");
        if (mat.empty()) { printf("No more images left to process. Exiting.\n"); break; }
        t.reset();
        cv::Mat greyscale = src->siftImageForMat(i);
        t.logElapsed("siftImageForMat");
        float* x = (float*)greyscale.data;
        size_t w = mat.cols, h = mat.rows;
        //auto path = src->nameForIndex(i);
        
        tp.push([&pOrig=p, x, w, h](int id, /*extra args:*/ size_t i, cv::Mat mat, cv::Mat prevImage) {
            std::cout << "hello from " << id << std::endl;
            
            SIFTParams p(pOrig); // New version of the params we can modify (separately from the other threads)
            p.params = sift_assign_default_parameters();

            // compute sift keypoints
            t.reset();
            int n; // Number of keypoints
            struct sift_keypoints* keypoints;
            struct sift_keypoint_std *k;
            k = my_sift_compute_features(p.params, x, w, h, &n, &keypoints);
            printf("Thread %d: Number of keypoints: %d\n", id, n);
            t.logElapsed(id, "compute features");
            
            // Check if we have to compare with a previous image
            struct sift_keypoints* out_k1 = nullptr;
            struct sift_keypoints* out_k2A = nullptr;
            struct sift_keypoints* out_k2B = nullptr;
            if (!prevImage.empty()) {
                // Setting current parameters for matching
                p.meth_flag = 1;
                p.thresh = 0.6;

                // Matching
                struct sift_keypoints* keypointsPrev = nullptr;
                Timer t2;
                do {
                    t.reset();
                    keypointsPrev = i > 0 ? processedImageQueue.get(i-1).computedKeypoints.get() : nullptr;
                    t.logElapsed(id, "attempt to get keypointsPrev");
                    
                    // Wait until it is good
                    pthread_mutex_lock(&processedImageQueue.mutex);
                    while( processedImageQueue.count == 0 ) // Wait until not empty.
                    {
                        pthread_cond_wait( &processedImageQueue.condition, &processedImageQueue.mutex );
                    }
                } while (keypointsPrev == nullptr);
                t2.logElapsed(id, "get keypointsPrev");
                t.reset();
                out_k1 = sift_malloc_keypoints();
                out_k2A = sift_malloc_keypoints();
                out_k2B = sift_malloc_keypoints();
                matching(keypointsPrev, keypoints, out_k1, out_k2A, out_k2B, p.thresh, p.meth_flag);
                t.logElapsed(id, "find matches");
            }

            t.reset();
            processedImageQueue.enqueue(mat,
                                        unique_keypoints_ptr(keypoints),
                                        unique_keypoints_ptr(out_k1),
                                        unique_keypoints_ptr(out_k2A),
                                        unique_keypoints_ptr(out_k2B),
                                        p);
            t.logElapsed(id, "enqueue procesed image");

            // cleanup
            free(k);
        }, /*extra args:*/ i, mat, prevImage);
        
        // Save this image for next iteration
        prevImage = mat;
        
        if (firstImage.empty()) { // This is the first iteration.
            // Save firstImage once
            firstImage = mat;
        }
    }
    
    tp.stop();
    return 0;
}

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
