import TaskSchedulingModule;

#include <chrono>
#include <iostream>
#include <thread>

// GLOBALS
std::atomic_bool gAppRunning = true;

void stop_running()
{
    std::cout << "Stop_running()\n";
    gAppRunning.store(false);
}

void parallel_sayhi()
{
    std::this_thread::sleep_for(50ms); // work simulation
    std::cout << "[Thread=" << std::this_thread::get_id()
        << "] Hello there, I'm from a parallel universe!\n";
}

int main(int argc, char* argv[])
{
    gAppRunning.store(true);

    TaskSchedulerInfo info;
    info.maxSize = 64U;
    // TODO: Here we could go crazy and reserve 1 main thread, 1 audio thread, 1 physics thread, and
    // TODO: dedicate what's left (std::thread::hardware_concurrency() - 3) for parallel task execution.
    info.numParallelThreads = 4U; // Try 0 for only synchronous!
    TaskScheduler taskScheduler(info);

    for (int i = 0; i < 10; i++) { taskScheduler.AddTimedTask(5s, { &parallel_sayhi, false }); }
    taskScheduler.AddTimedTask(10s, { &stop_running, true });

    while (gAppRunning.load())
    {
        taskScheduler.ProcessTasks();

        // Possibly game loop stuff here
        std::cout << "Processing...\n";

        std::this_thread::sleep_for(1000ms); // frame limiter
    }

    // NOTE: Here we can try to wait (immediate execution) for any tasks remaining!
    taskScheduler.AddTimedTask(30s, { []{ std::cout << "Wait for me!\n"; }, true });
    taskScheduler.Terminate(true); // false (=default) ignores remaining tasks

    std::cout << "Finished.\n";
    return 0;
}
