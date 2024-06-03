﻿module;

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <semaphore>
#include <unordered_set>

export module TestModule;

export using namespace std::chrono_literals;

export struct TimedTaskInfo
{
    std::function<void()> callback = nullptr;
    bool forceSynchronous = true; // true => run on main thread; false => run on parallel thread
};

struct TaskWithTimer
{
    TimedTaskInfo taskInfo;
    std::chrono::milliseconds duration;
};


class Constants16bit
{
public:
    static constexpr uint16_t Invalid  = 0x8000U; // 10000000 00000000
};

struct LinkedListNode // not exported
{
    TaskWithTimer element {};
    LinkedListNode& operator=(const TaskWithTimer& other) { element = other; return *this; }
};

template<uint16_t Size>
class LinkedListArray
{
    static_assert(Size < Constants16bit::Invalid);
public:
    LinkedListArray();
    ~LinkedListArray();
    bool Insert(const TaskWithTimer& elem);
    void ForEach(const std::function<bool(TaskWithTimer&)>& iterate); // iterate returns 'true' is element should be removed
    void PostIterate(); // cleanup any elements marked as so

private:
    LinkedListNode mList[Size];
    std::unordered_set<uint16_t> mAllocated;

    // free-list implemented as a stack (probably better cache performance)
    uint16_t mFreeList[Size];
    uint16_t mFreeCount;

    // elements marked for removal
    uint16_t mRemovals[Size];
    uint16_t mRemovalCount;
};


class ParallelTaskRunner // not exported
{
public:
    ParallelTaskRunner(const uint8_t numParallelThreads);
    ~ParallelTaskRunner();
    void Terminate();
    void RunTask(const TimedTaskInfo& taskInfo);

private:
    void Runner();
    std::condition_variable mCV;
    std::vector<std::thread> mThreads;
    std::atomic_bool mRunning;
    std::binary_semaphore mSem {1}; // ready!
    std::queue<TimedTaskInfo> mQueue;
};


export struct TaskContainerInfo // Yes, I'm a Vulkan programmer ^^
{
    uint16_t maxSize {64}; // TODO: This value is not actually implemented
    uint8_t numParallelThreads {1U};
};

export class TaskContainer
{
public:
    TaskContainer(const TaskContainerInfo& info);
    ~TaskContainer();
    void ProcessTasks();
    void AddTimedTask(std::chrono::seconds duration, const TimedTaskInfo& taskInfo);
    void Terminate(bool waitForSynchronousTasks = false, bool waitForParallelTasks = false);

private:
    bool ForEachTask(TaskWithTimer& taskInfo);
    ParallelTaskRunner* mTaskRunner = nullptr;
    LinkedListArray<64>* mTaskList = nullptr; // space for max 64 tasks at any given time

    std::chrono::time_point<std::chrono::system_clock> mTimer; // TODO: Different possibilities for template arg
    std::chrono::milliseconds mElapsed;
};


module :private;


template <uint16_t Size>
LinkedListArray<Size>::LinkedListArray()
{
    for (uint16_t i = 0; i < Size; i++)
    {
        mFreeList[i] = i;
        mRemovals[i] = Constants16bit::Invalid;
    }
    mFreeCount = Size;
    mRemovalCount = 0U;
}

template <uint16_t Size>
LinkedListArray<Size>::~LinkedListArray()
{
    mFreeCount = 0; // insertion will fail
    mAllocated.clear(); // ForEach will have 0 iterations
    mRemovalCount = 0U; // PostIterate will have 0 iterations
}

template <uint16_t Size>
bool LinkedListArray<Size>::Insert(const TaskWithTimer& elem)
{
    if (mFreeCount == 0) { return false; }
    const uint16_t index = mFreeList[--mFreeCount];
    mList[index] = elem; // insert at back
    mAllocated.insert(index);
    return true;
}

template <uint16_t Size>
void LinkedListArray<Size>::ForEach(const std::function<bool(TaskWithTimer&)>& iterate)
{
    for (const uint16_t index : mAllocated)
    {
        TaskWithTimer& elem = mList[index].element;
        if (iterate(elem))
        {
            mRemovals[mRemovalCount++] = index;
        }
    }
}

template <uint16_t Size>
void LinkedListArray<Size>::PostIterate()
{
    for (uint16_t i = 0; i < mRemovalCount; i++)
    {
        mAllocated.erase(mRemovals[i]);
        mFreeList[mFreeCount++] = mRemovals[i];
    }
    mRemovalCount = 0U;
}


ParallelTaskRunner::ParallelTaskRunner(const uint8_t numParallelThreads)
{
    // TODO: Create a thread pool!
    mRunning.store(true);
    for (uint8_t i = 0; i < numParallelThreads; i++)
    {
        mThreads.emplace_back([this]{ this->Runner(); });
    }
}

ParallelTaskRunner::~ParallelTaskRunner()
{
}

void ParallelTaskRunner::Terminate()
{
    mRunning.store(false);
    mCV.notify_all();
    for (auto& t : mThreads) { t.join(); }
}

void ParallelTaskRunner::RunTask(const TimedTaskInfo& taskInfo)
{
    mSem.acquire();
    mQueue.push(taskInfo); // we must copy it
    mSem.release();
    mCV.notify_one();
}

void ParallelTaskRunner::Runner()
{
    std::cout << "Spawning task thread " << std::this_thread::get_id() << "\n";
    std::mutex local_mutex;

    while (mRunning.load())
    {
        std::unique_lock lk(local_mutex);
        mSem.acquire();
        if (mQueue.empty())
        {
            mSem.release();
            mCV.wait(lk); // spurious wakeups may also occur, but even then we still continue loop!
            continue;
        }
        TimedTaskInfo timedTask = mQueue.front();
        mQueue.pop();
        mSem.release();

        timedTask.callback();
    }

    std::cout << "Ending task thread " << std::this_thread::get_id() << "\n";
}


TaskContainer::TaskContainer(const TaskContainerInfo& info)
{
    mTaskRunner = new ParallelTaskRunner(info.numParallelThreads);
    mTaskList = new LinkedListArray<64>(); // TODO: Potentially should use maxSize here!
    mTimer = std::chrono::system_clock::now();
    mElapsed = {};
}

TaskContainer::~TaskContainer()
{
    mTaskRunner->Terminate();
    delete mTaskRunner;
    delete mTaskList;
}

void TaskContainer::ProcessTasks()
{
    auto now = std::chrono::system_clock::now();
    mElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mTimer);

    mTaskList->ForEach(std::bind(&TaskContainer::ForEachTask, this, std::placeholders::_1));
    mTaskList->PostIterate();

    mTimer = now;
}

bool TaskContainer::ForEachTask(TaskWithTimer& taskInfo)
{
    bool elapsed = (mElapsed >= taskInfo.duration);
    if (elapsed)
    {
        if (taskInfo.taskInfo.forceSynchronous)
        {
            taskInfo.taskInfo.callback();
        }
        else
        {
            // delegate to Task Runner
            mTaskRunner->RunTask(taskInfo.taskInfo);
        }
    }
    else
    {
        taskInfo.duration -= mElapsed;
    }
    return elapsed;
}

void TaskContainer::AddTimedTask(std::chrono::seconds duration, const TimedTaskInfo& taskInfo)
{
    if (taskInfo.callback == nullptr)
    {
        std::cerr << "[TaskContainer::AddTimedTask] callback is NULL!\n";
        return;
    }
    mTaskList->Insert({ taskInfo, duration });
}

void TaskContainer::Terminate(bool waitForSynchronousTasks, bool waitForParallelTasks)
{
}
