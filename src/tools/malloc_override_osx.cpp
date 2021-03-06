//
//  malloc_override_osx.cpp
//  SIFT
//
//  Created by VADL on 12/5/21.
//  Copyright © 2021 VADL. All rights reserved.
//
// https://gist.github.com/monitorjbl/3dc6d62cf5514892d5ab22a59ff34861

#include "malloc_override_osx.hpp"
#include "file_descriptors.h"

#ifdef USE_PTR_INC_MALLOC

real_malloc _r_malloc = nullptr;
real_calloc _r_calloc = nullptr;
real_free _r_free = nullptr;
real_realloc _r_realloc = nullptr;

namespace memory{
    namespace _internal{

    #ifdef __APPLE__
        std::atomic_ulong allocated(0);
    #else
       std::atomic<unsigned long> allocated(0);
    #endif
    }

    unsigned long current(){
        return _internal::allocated.load();
    }
}

// NOTE: When linking jemalloc, these (at least in one case_ grab libjemalloc.2.dylib's malloc functions instead of the malloc, etc. from C standard library. It can be seen in the debugger of Xcode, and jemalloc_wrapper.h's functions cause infinite recursion if a call to them is implemented by the CALL_r_malloc(), etc. macros
#ifdef MYMALLOC
thread_local bool isMtraceHack = false;
#endif
void mtrace_init(void){
    // Calling dlsym can call _dlerror_run which can call calloc to show error messages, causing an infinite recursion.
    _r_malloc = reinterpret_cast<real_malloc>(reinterpret_cast<long>(dlsym(RTLD_NEXT, "malloc")));
    if (!_r_malloc) {
        char* e = dlerror();
        write(STDOUT, e, strlen(e));
    }
    _r_calloc = reinterpret_cast<real_calloc>(reinterpret_cast<long>(dlsym(RTLD_NEXT, "calloc")));
    if (!_r_calloc) {
        char* e = dlerror();
        write(STDOUT, e, strlen(e));
    }
    _r_free = reinterpret_cast<real_free>(reinterpret_cast<long>(dlsym(RTLD_NEXT, "free")));
    if (!_r_free) {
        char* e = dlerror();
        write(STDOUT, e, strlen(e));
    }
    _r_realloc = reinterpret_cast<real_realloc>(reinterpret_cast<long>(dlsym(RTLD_NEXT, "realloc")));
    if (!_r_realloc) {
        char* e = dlerror();
        write(STDOUT, e, strlen(e));
    }
    if (NULL == _r_malloc || NULL == _r_calloc || NULL == _r_free || NULL == _r_realloc) {
        fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
    }
}

// https://stackoverflow.com/questions/4840410/how-to-align-a-pointer-in-c //
#include <cstdint>
/** Returns the number to add to align the given pointer to a 8, 16, 32, or 64-bit
    boundary.
    @author Cale McCollough.
    @param  ptr The address to align.
    @return The offset to add to the ptr to align it. */
template<typename T>
inline uintptr_t MemoryAlignOffset (const void* ptr) {
    return ((~reinterpret_cast<uintptr_t> (ptr)) + 1) & (sizeof (T) - 1);
}

/** Word aligns the given byte pointer up in addresses.
    @author Cale McCollough.
    @param ptr Pointer to align.
    @return Next word aligned up pointer. */
template<typename T>
inline T* MemoryAlign (T* ptr) {
    uintptr_t offset = MemoryAlignOffset<uintptr_t> (ptr);
    char* aligned_ptr = reinterpret_cast<char*> (ptr) + offset;
    return reinterpret_cast<T*> (aligned_ptr);
}
// //
void* mallocModeDidHandle(size_t size) {
    // Check our malloc mode
    if (currentMallocImpl == MallocImpl_PointerIncWithFreeAll) {
        if (
            // Pointer increment if we can.
            mallocWithFreeAll_current + size < mallocWithFreeAll_max) {
            // Pointer increment since we can.
            void* ret = mallocWithFreeAll_current;
            mallocWithFreeAll_current += size;
            using AlignT = uint64_t;
            mallocWithFreeAll_current = (char*)MemoryAlign<AlignT>((AlignT*)mallocWithFreeAll_current); // uint64_t is used for 8 byte alignment (arbitrary type of 64-bit size)
            static_assert(sizeof(AlignT*) == 8, "Find a type with size 8 bytes and use it instead");
            postMalloc();
            numPointerIncMallocsThisFrame++;
            return ret;
        }
        else {
            mallocWithFreeAll_hitLimitCount++;
            void* p = malloc(size);
            postMalloc();
            return p;
        }
    }
    else {
        return nullptr;
    }
}

void* malloc(size_t size){
    if(_r_malloc==NULL) {
        mtrace_init();
    }

    // Thread-safe because thread_local (TLS) variables are used here:
    preMalloc();
    // Check our malloc mode
    void *p;
    if ((p = mallocModeDidHandle(size)) == nullptr) {
        p = CALL_r_malloc(size);
        postMalloc();
    }
    numMallocsThisFrame++;
    #ifdef __APPLE__
    size_t realSize = malloc_size(p);
    memory::_internal::allocated += realSize;
    #endif
    MALLOC_LOG("malloc", realSize, p);
    return p;
}

#include "myMalloc_.h"
thread_local int callocCounter = 0;
void* calloc(size_t nitems, size_t size){
    if(_r_calloc==NULL) {
        if (callocCounter++ <= 3 /* <-- num dlsym() calls in mtrace_init() */) {
            // First calloc must be semi-normal since dyld uses it with dlsym error messages
            void* p = _malloc(size);
            memset(p, 0, size);
            return p;
        }
//#ifdef MYMALLOC
//        using namespace backward;
//        isMtraceHack = true;
//        StackTrace st; st.load_here();
//        Printer p;
//        p.print(st, stderr);
//        std::ostringstream ss;
//        p.print(st, ss);
//        auto s = ss.str();
//        if (s.find("_dlerror_run") != std::string::npos) {
//            // Let it calloc, this is from https://github.com/lattera/glibc/blob/master/dlfcn/dlerror.c , else infinite recursion
//            void* p = _malloc(size);
//            memset(p, 0, size);
//            return p;
//        }
//#endif
        
        mtrace_init();
    }
    
    preMalloc();
    // Check our malloc mode
    void *p;
    if ((p = mallocModeDidHandle(size)) == nullptr) {
        p = CALL_r_calloc(nitems, size);
        postMalloc();
    }
    else {
        memset(p, 0, size); // calloc implementation
    }
    size_t realSize = CALLmalloc_size(p);
    memory::_internal::allocated += realSize;
    MALLOC_LOG("calloc", realSize, p);
    return p;
}

void free(void* p){
    if(_r_free==nullptr) {
        if (callocCounter <= 3 /* <-- num dlsym() calls in mtrace_init() */) {
            // First calloc must be semi-normal since dyld uses it with dlsym error messages
            _free(p);
            return;
        }
        mtrace_init();
    }

    if(p != nullptr){
        size_t realSize = CALLmalloc_size(p);
        preFree();
        // Check our malloc mode
        if (currentMallocImpl == MallocImpl_PointerIncWithFreeAll) {
            // No-op, nothing to free
        }
        else {
            CALL_r_free(p);
        }
        postFree();
        memory::_internal::allocated -= realSize;
        MALLOC_LOG("free", realSize, p);
    } else {
        preFree();
        if (currentMallocImpl == MallocImpl_Default) {
            CALL_r_free(p);
        }
        postFree();
    }
}

void* realloc(void* p, size_t new_size){
    if(_r_realloc==nullptr) {
        mtrace_init();
    }

    preMalloc();
    // No malloc-mode supported since we don't know the original allocation size and it's not worth tracking
    assert(currentMallocImpl == MallocImpl_Default);
    p = CALL_r_realloc(p, new_size);
    postMalloc();
    return p;
}

void *operator new(size_t size) noexcept(false){
#ifdef MYMALLOC
    if (isMtraceHack) {
        return _malloc(size);
    }
#endif
    return malloc(size);
}

void *operator new [](size_t size) noexcept(false){
#ifdef MYMALLOC
    if (isMtraceHack) {
        return _malloc(size);
    }
#endif
    return malloc(size);
}

void operator delete(void * p) throw(){
#ifdef MYMALLOC
    if (isMtraceHack) {
        return _free(p);
    }
#endif
    free(p);
}

#endif
