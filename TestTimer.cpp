import TestModule;

#include <chrono>
#include <functional>
#include <iostream>
#include <future>

//using namespace std::chrono_literals;

template<class Rep, class Period>
std::future<void> TimerAsync(std::chrono::duration<Rep, Period> duration, const std::function<void()>& callback)
{
    return std::async(std::launch::async, [duration, callback]()
    {
        std::this_thread::sleep_for(duration);
        callback();
    });
}

// GLOBALS
std::atomic_bool gAppRunning = true;

void stop_running()
{
    gAppRunning.store(false);
}

int main(int argc, char* argv[])
{
    gAppRunning.store(true);
    TaskContainer container;

    TimedTaskInfo info { &stop_running, true };
    container.AddTimedTask(5s, info);

    while (gAppRunning.load())
    {
        container.ProcessTasks();

        // Process main thread queue here

        std::cout << "Processing...\n";
        std::this_thread::sleep_for(500ms); // work simulation

    }

    std::cout << "Finished.\n";
    return 0;
}
