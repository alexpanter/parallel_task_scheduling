import TestModule;

#include <chrono>
#include <iostream>
#include <thread>

// GLOBALS
std::atomic_bool gAppRunning = true;

void stop_running()
{
    std::cout << "[Thread=" << std::this_thread::get_id() << "] stop_running()\n";
    gAppRunning.store(false);
}

void parallel_sayhi()
{
    std::cout << "[Thread=" << std::this_thread::get_id()
        << "] Hello there, I'm from a parallel universe!\n";
}

int main(int argc, char* argv[])
{
    gAppRunning.store(true);

    TaskContainerInfo containerInfo {};
    containerInfo.maxSize = 64;
    // TODO: Here we could go crazy and reserve 1 main thread, 1 audio thread, 1 physics thread, and
    // TODO: dedicate what's left (std::thread::hardware_concurrency() - 3) for parallel task execution.
    containerInfo.numParallelThreads = 1U;
    TaskContainer container(containerInfo);

    container.AddTimedTask(1s, { &parallel_sayhi, false });
    container.AddTimedTask(2s, { &stop_running, true });

    while (gAppRunning.load())
    {
        container.ProcessTasks();

        // Process main thread queue here

        std::cout << "Processing...\n";
        std::this_thread::sleep_for(500ms); // work simulation / frame limiter
    }

    container.Terminate();

    std::cout << "Finished.\n";
    return 0;
}
