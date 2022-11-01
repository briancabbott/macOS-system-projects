//===-------------------------- cxa_guard.cxx -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <pthread.h>
#include <stdlib.h>

#include "cxxabi.h"
#include "abort_message.h"

//
// This file implements the __cxa_guard_* functions as defined at:
//     http://www.codesourcery.com/public/cxx-abi/abi.html
//
// The goal of these functions is to support thread-safe, one-time 
// initialization of function scope variables.  The compiler will generate
// code like the following:
//
//  if ( obj_guard.first_byte == 0 ) {
//      if ( __cxa_guard_acquire(&obj_guard) ) {
//        try {
//          ... initialize the object ...;
//        } 
//        catch (...) {
//          __cxa_guard_abort(&obj_guard);
//          throw;
//        }
//        ... queue object destructor with __cxa_atexit() ...;
//        __cxa_guard_release(&obj_guard);
//      }
//   }
//
// Notes:
//   ojb_guard is a 64-bytes in size and statically initialized to zero.
//
//   Section 6.7 of the C++ Spec says "If control re-enters the declaration
//   recursively while the object is being initialized, the behavior is
//   undefined".  This implementation calls abort().
//


// Note don't use function local statics to avoid use of cxa functions...
static pthread_mutex_t __guard_mutex;
static pthread_once_t __once_control = PTHREAD_ONCE_INIT;


static void makeRecusiveMutex()
{
    pthread_mutexattr_t recursiveMutexAttr;
    pthread_mutexattr_init(&recursiveMutexAttr);
    pthread_mutexattr_settype(&recursiveMutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&__guard_mutex, &recursiveMutexAttr);
}

__attribute__((noinline))
static pthread_mutex_t* guard_mutex()
{
    pthread_once(&__once_control, &makeRecusiveMutex);
    return &__guard_mutex;
}

// helper functions for getting/setting flags in guard_object
static bool initializerHasRun(uint64_t* guard_object)
{
    return ( *((uint8_t*)guard_object) != 0 );
}

static void setInitializerHasRun(uint64_t* guard_object)
{
    *((uint8_t*)guard_object)  = 1;
}

static bool inUse(uint64_t* guard_object)
{
    return ( ((uint8_t*)guard_object)[1] != 0 );
}

static void setInUse(uint64_t* guard_object)
{
    ((uint8_t*)guard_object)[1] = 1;
}

static void setNotInUse(uint64_t* guard_object)
{
    ((uint8_t*)guard_object)[1] = 0;
}


//
// Returns 1 if the caller needs to run the initializer and then either
// call __cxa_guard_release() or __cxa_guard_abort().  If zero is returned,
// then the initializer has already been run.  This function blocks
// if another thread is currently running the initializer.  This function
// aborts if called again on the same guard object without an intervening
// call to __cxa_guard_release() or __cxa_guard_abort().
//
int __cxxabiv1::__cxa_guard_acquire(uint64_t* guard_object)
{
    // Double check that the initializer has not already been run
    if ( initializerHasRun(guard_object) )
        return 0;

    // We now need to acquire a lock that allows only one thread
    // to run the initializer.  If a different thread calls
    // __cxa_guard_acquire() with the same guard object, we want 
    // that thread to block until this thread is done running the 
    // initializer and calls __cxa_guard_release().  But if the same
    // thread calls __cxa_guard_acquire() with the same guard object,
    // we want to abort.  
    // To implement this we have one global pthread recursive mutex 
    // shared by all guard objects, but only one at a time.  

    int result = ::pthread_mutex_lock(guard_mutex());
    if ( result != 0 ) {
        abort_message("__cxa_guard_acquire(): pthread_mutex_lock "
                      "failed with %d\n", result);
    }
    // At this point all other threads will block in __cxa_guard_acquire()
    
    // Check if another thread has completed initializer run
    if ( initializerHasRun(guard_object) ) {
        int result = ::pthread_mutex_unlock(guard_mutex());
        if ( result != 0 ) {
            abort_message("__cxa_guard_acquire(): pthread_mutex_unlock "
                          "failed with %d\n", result);
        }
        return 0;
    }
    
    // The pthread mutex is recursive to allow other lazy initialized
    // function locals to be evaluated during evaluation of this one.
    // But if the same thread can call __cxa_guard_acquire() on the 
    // *same* guard object again, we call abort();
    if ( inUse(guard_object) ) {
        abort_message("__cxa_guard_acquire(): initializer for function "
                      "local static variable called enclosing function\n");
    }
    
    // mark this guard object as being in use
    setInUse(guard_object);

    // return non-zero to tell caller to run initializer
    return 1;
}



//
// Sets the first byte of the guard_object to a non-zero value.
// Releases any locks acquired by __cxa_guard_acquire().
//
void __cxxabiv1::__cxa_guard_release(uint64_t* guard_object)
{
    // first mark initalizer as having been run, so 
    // other threads won't try to re-run it.
    setInitializerHasRun(guard_object);

    // release global mutex
    int result = ::pthread_mutex_unlock(guard_mutex());
    if ( result != 0 ) {
        abort_message("__cxa_guard_acquire(): pthread_mutex_unlock "
                      "failed with %d\n", result);
    }
}



//
// Releases any locks acquired by __cxa_guard_acquire().
//
void __cxxabiv1::__cxa_guard_abort(uint64_t* guard_object)
{
    int result = ::pthread_mutex_unlock(guard_mutex());
    if ( result != 0 ) {
        abort_message("__cxa_guard_abort(): pthread_mutex_unlock "
                      "failed with %d\n", result);
    }
	
	// now reset state, so possible to try to initialize again
	setNotInUse(guard_object);
}



