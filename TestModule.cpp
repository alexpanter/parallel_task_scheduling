module;

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <unordered_set>

export module TestModule;

export using namespace std::chrono_literals;

export struct TimedTaskInfo
{
    std::function<void()> callback = nullptr;
    bool forceSynchronous = true; // true => run on main thread; false => run on parallel thread
};


class Constants16bit
{
public:
    static constexpr uint16_t Invalid  = 0x8000U; // 10000000 00000000
};

template<typename T>
requires std::is_default_constructible_v<T>
struct LinkedListNode // not exported
{
    T element {};
    LinkedListNode<T>& operator=(const T& other) { element = other; return *this; }
};

template<typename T, uint16_t Size>
class LinkedListArray
{
    static_assert(Size < Constants16bit::Invalid);
public:
    LinkedListArray();
    bool Insert(const T& elem);

    void ForEach(std::function<bool(const T&)> iterate); // iterate returns 'true' is element should be removed
    void PostIterate(); // cleanup any elements marked as so

private:
    LinkedListNode<T> mList[Size];
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
    ParallelTaskRunner();
    ~ParallelTaskRunner();
    void RunTasks();
    void Notify() {}
    void Terminate();
    void RunTask(const TimedTaskInfo& taskInfo);

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
    void AddTimedTask(std::chrono::seconds duration, const TimedTaskInfo& taskInfo);

private:
    bool ForEachTask(const TimedTaskInfo& taskInfo);
    ParallelTaskRunner* mTaskRunner = nullptr;
    LinkedListArray<TimedTaskInfo, 64>* mTaskList; // space for 64 tasks at any given time
};


module :private;


template <typename T, uint16_t Size>
LinkedListArray<T, Size>::LinkedListArray()
{
    for (uint16_t i = 0; i < Size; i++)
    {
        mFreeList[i] = i;
        mRemovals[i] = Constants16bit::Invalid;
    }
    mFreeCount = Size;
    mRemovalCount = 0U;
}

template <typename T, uint16_t Size>
bool LinkedListArray<T, Size>::Insert(const T& elem)
{
    if (mFreeCount == 0) { return false; }
    const uint16_t index = mFreeList[--mFreeCount];
    mList[index] = elem; // insert at back
    mAllocated.insert(index);
    return true;
}

template <typename T, uint16_t Size>
void LinkedListArray<T, Size>::ForEach(std::function<bool(const T&)> iterate)
{
    for (const uint16_t index : mAllocated)
    {
        const T& elem = mList[index].element; 
        if (iterate(elem))
        {
            mRemovals[mRemovalCount++] = index;
        }
    }
}

template <typename T, uint16_t Size>
void LinkedListArray<T, Size>::PostIterate()
{
    for (uint16_t i = 0; i < mRemovalCount; i++)
    {
        mAllocated.erase(mRemovals[i]);
        mFreeList[mFreeCount++] = mRemovals[i];
    }
    mRemovalCount = 0U;
}


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

void ParallelTaskRunner::RunTask(const TimedTaskInfo& taskInfo)
{
    // TODO: Probably something with condition variable
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
    mTaskList->ForEach(std::bind(&TaskContainer::ForEachTask, this, std::placeholders::_1)); // TODO: Will need std::bind?
    mTaskList->PostIterate();
}

bool TaskContainer::ForEachTask(const TimedTaskInfo& taskInfo)
{
    bool elapsed = true; // TODO: Check if timespan has elapsed
    if (elapsed)
    {
        if (taskInfo.forceSynchronous)
        {
            taskInfo.callback();
        }
        else
        {
            // delegate to Task Runner
            mTaskRunner->RunTask(taskInfo);
        }
    }
    return elapsed;
}

void TaskContainer::AddTimedTask(std::chrono::seconds duration, const TimedTaskInfo& taskInfo)
{
    if (taskInfo.callback == nullptr)
    {
        std::cerr << "[TaskContainer::AddTimedTask] callback is NULL!" << std::endl;
        return;
    }

    mTaskList->Insert(taskInfo); // TODO: Should insert together with duration!
}
