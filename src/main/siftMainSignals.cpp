//
//  siftMainSignals.cpp
//  SIFT
//
//  Created by VADL on 3/1/22.
//  Copyright © 2022 VADL. All rights reserved.
//

#include "siftMainSignals.hpp"
#include "siftMainCmdConfig.hpp"

#include "../tools/printf.h"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "../DataOutput.hpp"

// Signal handling //
std::atomic<bool> g_stop, g_stopAndFinishRest;
void stopMain() {
    g_stop = true;
}
bool stoppedMain() {
    return g_stop;
}
void stopMainAndFinishRest() {
    g_stopAndFinishRest = true;
}
bool stoppedMainAndFinishRest() {
    return g_stopAndFinishRest;
}

// Prints a stacktrace
//void logTrace() {
//    using namespace backward;
//    StackTrace st; st.load_here();
//    Printer p;
//    p.print(st, stderr);
//}
void ctrlC(int s, siginfo_t *si, void *arg){
    if (CMD_CONFIG(finishRestOnSigInt)) {
        printf_("Caught signal %d. Stopping after draining dispatch queue...\n",s);
        stopMainAndFinishRest();
        return;
    }
    
    printf_("Caught signal %d. Threads are stopping...\n",s); // Can't call printf because it might call malloc but the ctrlC might have happened in the middle of a malloc or free call, leaving data structures for it invalid. So we use `printf_` from https://github.com/mpaland/printf which is thread-safe and malloc-free but slow because it just write()'s all the characters one by one. Generally, "the signal handler code must not call non-reentrant functions that modify the global program data underneath the hood." ( https://www.delftstack.com/howto/c/sigint-in-c/ )
    // Notes:
    /* https://stackoverflow.com/questions/11487900/best-pratice-to-free-allocated-memory-on-sigint :
     Handling SIGINT: I can think of four common ways to handle a SIGINT:

     Use the default handler. Highly recommended, this requires the least amount of code and is unlikely to result in any abnormal program behavior. Your program will simply exit.

     Use longjmp to exit immediately. This is for folk who like to ride fast motorcycles without helmets. It's like playing Russian roulette with library calls. Not recommended.

     Set a flag, and interrupt the main loop's pselect/ppoll. This is a pain to get right, since you have to twiddle with signal masks. You want to interrupt pselect/ppoll only, and not non-reentrant functions like malloc or free, so you have to be very careful with things like signal masks. Not recommended. You have to use pselect/ppoll instead of select/poll, because the "p" versions can set the signal mask atomically. If you use select or poll, a signal can arrive after you check the flag but before you call select/poll, this is bad.

     Create a pipe to communicate between the main thread and the signal handler. Always include this pipe in the call to select/poll. The signal handler simply writes one byte to the pipe, and if the main loop successfully reads one byte from the other end, then it exits cleanly. Highly recommended. You can also have the signal handler uninstall itself, so an impatient user can hit CTRL+C twice to get an immediate exit.
     */
    stopMain();
    
    // Print stack trace
    //backward::sh->handleSignal(s, si, arg);
}
DataOutputBase* g_o2 = nullptr;
#undef BACKTRACE_SET_TERMINATE
#include "../tools/backtrace/backtrace_on_terminate.hpp"
void terminate_handler(bool backtrace) {
    if (g_o2) {
        // [!] Take a risk and do something that can't always succeed in a segfault handler: possible malloc and state mutation:
        ((FileDataOutput*)g_o2)->release();
    
//        ((FileDataOutput*)g_o2)->writer.release(); // Save the file
//                    std::cout << "Saved the video" << std::endl;
    }
    else {
        // [!] Take a risk and do something that can't always succeed in a segfault handler: possible malloc and state mutation:
        //std::cout << "No video to save" << std::endl;
        printf_("No video to save\n");
    }
    
    if (backtrace) {
        backtrace_on_terminate();
    }
}
void segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    printf_("Caught segfault (%s) at address %p. Running terminate_handler().\n", strsignal(signal), si->si_addr);
    
    terminate_handler(false);
    
    // Print stack trace
    //backward::sh->handleSignal(signal, si, arg);
    backtrace(std::cout);
    
    exit(5); // Probably doesn't call atexit since signal handlers don't.
}
// Installs signal handlers for the current thread.
void installSignalHandlers() {
    if (CMD_CONFIG(useSetTerminate)) {
        // https://en.cppreference.com/w/cpp/error/set_terminate
        std::set_terminate([](){
            std::cout << "Unhandled exception detected by SIFT. Running terminate_handler()." << std::endl;
            terminate_handler(true);
            std::abort(); // https://en.cppreference.com/w/cpp/utility/program/abort
        });
    }
    
    // Install ctrl-c handler
    // https://stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event
    struct sigaction sigIntHandler;
    sigIntHandler.sa_sigaction = ctrlC;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    
    // Install segfault handler
    // https://stackoverflow.com/questions/2350489/how-to-catch-segmentation-fault-in-linux
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_sigaction;
    sa.sa_flags   = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL); // Segfault handler! Tries to save stuff ASAP, unlike sigIntHandler.
    
    // Install SIGBUS (?), SIGILL (illegal instruction), and EXC_BAD_ACCESS (similar to segfault but is for "A crash due to a memory access issue occurs when an app uses memory in an unexpected way. Memory access problems have numerous causes, such as dereferencing a pointer to an invalid memory address, writing to read-only memory, or jumping to an instruction at an invalid address. These crashes are most often identified by the EXC_BAD_ACCESS (SIGSEGV) or EXC_BAD_ACCESS (SIGBUS) exceptions" ( https://developer.apple.com/documentation/xcode/investigating-memory-access-crashes )) handlers
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}
// //
