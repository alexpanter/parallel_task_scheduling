module;

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>

export module TestModule;
import DataStructures;

export using namespace std::chrono_literals;

export struct TimedTaskInfo
{
    std::function<void()> callback = nullptr;
    bool forceSynchronous = true; // true => run on main thread; false => run on parallel thread
};


class ParallelTaskRunner // not exported
{
public:
    ParallelTaskRunner();
    ~ParallelTaskRunner();
    void RunTasks();
    void Notify() {}
    void Terminate();

    template<class Rep, class Period>
    void AddTimedTask(std::chrono::duration<Rep, Period> duration, const TimedTaskInfo& taskInfo);

private:
    void Runner();
    std::condition_variable mCV;
    std::mutex mMutex;
    std::thread mThread;
    std::atomic_bool mRunning;
    std::queue<TimedTaskInfo> mParallelTasks;
};


export class TaskContainer
{
public:
    TaskContainer(uint16_t maxSize = 64);
    ~TaskContainer();
    void ProcessTasks();

    template<class Rep, class Period>
    void AddTimedTask(std::chrono::duration<Rep, Period> duration, const TimedTaskInfo& taskInfo);

private:
    ParallelTaskRunner* mTaskRunner = nullptr;
    LinkedListArray<TimedTaskInfo, 64>* mTaskList; // space for 64 tasks at any given time
};


module :private;


ParallelTaskRunner::ParallelTaskRunner()
{
    mRunning.store(true);
    mThread = std::thread([this]{ this->Runner(); });
}

ParallelTaskRunner::~ParallelTaskRunner()
{
}

void ParallelTaskRunner::RunTasks()
{
}

void ParallelTaskRunner::Terminate()
{
}

template <class Rep, class Period>
void ParallelTaskRunner::AddTimedTask(std::chrono::duration<Rep, Period> duration, const TimedTaskInfo& taskInfo)
{
}

void ParallelTaskRunner::Runner()
{
    std::cout << "TaskRunner::Runner()\n";

    while (mRunning.load())
    {
        std::unique_lock lk(mMutex);
        mCV.wait(lk);

        // TODO: spurious wakeup guard

        // TODO: do stuff

        lk.unlock();
        mCV.notify_one();
    }
}


TaskContainer::TaskContainer(uint16_t maxSize)
{
    mTaskRunner = new ParallelTaskRunner();
    mTaskList = new LinkedListArray<TimedTaskInfo, 64>(); // TODO: Potentially should use maxSize here!
}

TaskContainer::~TaskContainer()
{
}

void TaskContainer::ProcessTasks()
{
    
}



template <class Rep, class Period>
void TaskContainer::AddTimedTask(std::chrono::duration<Rep, Period> duration, const TimedTaskInfo& taskInfo)
{
    if (taskInfo.callback == nullptr)
    {
        std::cerr << "[TaskContainer::AddTimedTask] callback is NULL!" << std::endl;
        return;
    }

    if (!taskInfo.forceSynchronous)
    {
        // delegate to Task Runner
        mTaskRunner->AddTimedTask(duration, taskInfo);
    }
    else
    {
        
    }
}
