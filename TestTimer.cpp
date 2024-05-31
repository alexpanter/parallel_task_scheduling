import TestModule;
#include <chrono>
#include <functional>
#include <iostream>
#include <future>

using namespace std::chrono_literals;

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

int main(int argc, char* argv[])
{
    gAppRunning.store(true);
    auto future = TimerAsync(2s, []() { std::cout << "Timer finished!\n"; });
    TaskContainer container;

    while (gAppRunning.load())
    {
        container.ProcessTasks();

        // Process main thread queue here

        std::cout << "Processing...\n";
        std::this_thread::sleep_for(1s); // work simulation

        // Optionally terminate program when timer has expired
        auto status = future.wait_for(1ms);
        if (status == std::future_status::ready)
        {
            break;
        }
    }

    std::cout << "Finished.\n";
    return 0;
}
