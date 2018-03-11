# Sean's Thread Pool

This is a single-file, header only implementation of a thread pool using
C++17 features. It was created for my own edification and intended only
for my own use, but I hope someone else may find it useful or the
implementation instructive. It has no dependencies beyond a C++17-compliant
compiler and the standard library.

The interface is simple:

    using namespace ThreadPool;
    
    // Create a thread pool:
    ThreadPool thread_pool(num_threads);
    
    // Any callable can be submitted to the thread pool very simply;
    // define some function we want to submit:
    auto f = [](int x, int y) { return x + y; };
    
    // Call ThreadPool::submit(f, args...) to submit the task; this function
    // returns std::future<std::invoke_result_t<f, args...>>
    std::future<int> future = thread_pool.submit(f, 2, 3);
    
For a less trivial example, see 'calc_pi.cpp' which implements a Monte Carlo
algorithm to calculate pi using the thread pool.

## Acknowledgement

[This article](http://roar11.com/2016/01/a-platform-independent-thread-pool-using-c14/)
by Will Pearce was invaluable in developing the understanding needed of 
synchronization primitives and the considerations necessary to get a thread
pool working.

## License

Copyright (c) Sean McBane 2018 under the terms of the MIT License; 
see LICENSE.txt for the details.
