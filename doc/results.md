Actors:
 - Server: This is how acts the role of the NodeJs caller. Like a NodeApi function call.
 - Context: The video processing context, where the input is read and the output is written

API requirements
 - Having a callback from the top level API that sends notification when the processing is done.
 - Main thread should not be blocked at all.
 - It should be readable and local reasoning should be easy.

Runtime expectations:
 - Worker threads are never waiting. There is always work todo.
 - Main thread is not blocked.

Results:
 - Main thread is not blocked. (Starting the concurrent processing took 0.012ms. (See below in the pictures)
 - Worker threads are not waiting. The idle time on a worker thread when switching task is around 0.003ms
 - When 15 + 1 thread is used on a 16 core machine the CPU usage went up above 90 percent sometimes hit the 100%
 - The last read on the worker thread is reading the eof (had also different time)

About the implementation:
 - For profiling I used optick (ex brofiler)
 - The waits are busyWaits with a volaitile integer modification (to guarantee that the thread doesn't go to sleep due to compiler optimization)

Good to know:
 - For reading the capture file you can use Optick.1.4.0.0 gui application.
 - For running the code one need to manually copy the OptickCore.so next to the executable binary.
