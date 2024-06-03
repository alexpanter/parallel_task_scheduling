// MIT License
//
// Copyright (c) 2024 Alexander Christensen
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

module;

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <semaphore>
#include <unordered_set>

export module TaskSchedulingModule;

export using namespace std::chrono_literals;


export struct TaskInfo
{
    std::function<void()> callback = nullptr;
    bool forceSynchronous = true; // true => run on main thread; false => run on parallel thread
};

struct TimedTaskInfo
{
    TaskInfo taskInfo;
    std::chrono::milliseconds duration;
};


struct ContainerItem // not exported
{
    TimedTaskInfo element {};
    ContainerItem& operator=(const TimedTaskInfo& other) { element = other; return *this; }
};

class TaskContainer
{
public:
    TaskContainer(uint16_t size);
    ~TaskContainer();
    bool Insert(const TimedTaskInfo& elem);
    void ForEach(const std::function<bool(TimedTaskInfo&)>& iterate); // iterate returns 'true' is element should be removed
    void PostIterate(); // cleanup any elements marked as so

private:
    // This data structure is a bit complicated. :)
    // Basically I want to avoid copying task objects around, so they are stored only in `mList`,
    // while `mFreeList` and `mRemovals` contain indices hereinto.
    //
    // The unordered_set `mAllocated` contains a set of indices into `mList` that are currently allocated.
    // When a task is executed it gets removed from this set and placed into `mFreeList`,
    // where the free-list is implemented in a linear array as a stack.
    //
    // These efforts are to ensure a good runtime performance, since the functions `ForEach` and
    // `PostIterate` are called _each_ frame, so they should do as little memory juggling as possible.
    // Insertion is always a constant-time operation.

    ContainerItem* mList;
    const uint16_t mSize; // space for max mSize tasks at any given time
    std::unordered_set<uint16_t> mAllocated;

    // free-list implemented as a stack (probably better cache performance)
    uint16_t* mFreeList;
    uint16_t mFreeCount;

    // elements marked for removal
    uint16_t* mRemovals;
    uint16_t mRemovalCount;
};


class ParallelTaskRunner // not exported
{
public:
    ParallelTaskRunner(const uint8_t numParallelThreads);
    ~ParallelTaskRunner();
    void Terminate();
    void RunTask(const TaskInfo& taskInfo);

private:
    void Runner();
    std::condition_variable mCV;
    std::vector<std::thread> mThreads;
    std::atomic_bool mRunning;
    std::binary_semaphore mSem {1}; // ready!
    std::queue<TaskInfo> mQueue;
};


export struct TaskSchedulerInfo // Yes, I'm a Vulkan programmer ^^
{
    uint16_t maxSize {64};
    uint8_t numParallelThreads {1U};
};

export class TaskScheduler
{
public:
    TaskScheduler(const TaskSchedulerInfo& info);
    ~TaskScheduler();
    void ProcessTasks();
    // In my IDE templates on std::chrono::duration does not work across a module boundary!
    void AddTimedTask(std::chrono::milliseconds duration, const TaskInfo& taskInfo);
    void AddTimedTask(std::chrono::seconds duration, const TaskInfo& taskInfo);
    void Terminate(bool finishTasks = false);

private:
    bool mRunning;
    bool mParallelExecutionAllowed;
    bool ForEachTask(TimedTaskInfo& timedTaskInfo);
    bool ForceRunEachTask(const TimedTaskInfo& timedTaskInfo);
    ParallelTaskRunner* mParallelRunner = nullptr;
    TaskContainer* mContainer = nullptr;

    std::chrono::time_point<std::chrono::steady_clock> mTimer;
    std::chrono::milliseconds mElapsed;
};


module :private;


TaskContainer::TaskContainer(uint16_t size) : mSize(size)
{
    mList = new ContainerItem[mSize];
    mFreeList = new uint16_t[mSize];
    mRemovals = new uint16_t[mSize];

    for (uint16_t i = 0; i < mSize; i++)
    {
        mFreeList[i] = i; // initially full free-list, so must contain all indices
    }
    mFreeCount = mSize;
    mRemovalCount = 0U;
    mRemovals[0] = 0; // IDE complains if not initialized, but really doesn't matter.
}

TaskContainer::~TaskContainer()
{
    // Sometimes RAII is a really nice pattern!
    delete[] mList;
    delete[] mFreeList;
    delete[] mRemovals;
    mFreeCount = 0; // insertion will fail
    mAllocated.clear(); // ForEach will have 0 iterations
    mRemovalCount = 0U; // PostIterate will have 0 iterations
}

bool TaskContainer::Insert(const TimedTaskInfo& elem)
{
    if (mFreeCount == 0) { return false; }
    const uint16_t index = mFreeList[--mFreeCount];
    mList[index] = elem; // insert at back
    mAllocated.insert(index);
    return true;
}

void TaskContainer::ForEach(const std::function<bool(TimedTaskInfo&)>& iterate)
{
    for (const uint16_t index : mAllocated)
    {
        TimedTaskInfo& elem = mList[index].element;
        if (iterate(elem))
        {
            mRemovals[mRemovalCount++] = index;
        }
    }
}

void TaskContainer::PostIterate()
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

void ParallelTaskRunner::RunTask(const TaskInfo& taskInfo)
{
    mSem.acquire();
    mQueue.push(taskInfo); // we must copy it
    mSem.release();
    mCV.notify_one();
}

void ParallelTaskRunner::Runner()
{
    // NOTE: std::println would be better, but that requires C++23 :(
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
        TaskInfo timedTask = mQueue.front();
        mQueue.pop();
        mSem.release();

        timedTask.callback();
    }

    std::cout << "Ending task thread " << std::this_thread::get_id() << "\n";
}


TaskScheduler::TaskScheduler(const TaskSchedulerInfo& info)
{
    mRunning = true;
    mParallelExecutionAllowed = info.numParallelThreads > 0U;
    if (mParallelExecutionAllowed)
    {
        mParallelRunner = new ParallelTaskRunner(info.numParallelThreads);
    }
    mContainer = new TaskContainer(info.maxSize);
    mTimer = std::chrono::steady_clock::now();
    mElapsed = {};
}

TaskScheduler::~TaskScheduler()
{
    mRunning = false;
    if (mParallelRunner != nullptr)
    {
        delete mParallelRunner;
    }
    delete mContainer;
}

void TaskScheduler::ProcessTasks()
{
    auto now = std::chrono::steady_clock::now();
    mElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mTimer);

    mContainer->ForEach(std::bind(&TaskScheduler::ForEachTask, this, std::placeholders::_1));
    mContainer->PostIterate();

    mTimer = now;
}

bool TaskScheduler::ForEachTask(TimedTaskInfo& timedTaskInfo)
{
    bool elapsed = (mElapsed >= timedTaskInfo.duration);
    if (elapsed)
    {
        // TODO: Possible semaphore contention! (may create temporary storage) [optimization]
        // TODO: Or maybe use a semaphore that is based on spinlock instead of mutex!
        // This is only an issue if many tasks need execution in the same frame!
        // Otherwise it is a non-issue.

        if (timedTaskInfo.taskInfo.forceSynchronous || !mParallelExecutionAllowed)
        {
            timedTaskInfo.taskInfo.callback();
        }
        else
        {
            mParallelRunner->RunTask(timedTaskInfo.taskInfo);
        }
    }
    else
    {
        timedTaskInfo.duration -= mElapsed;
    }
    return elapsed;
}

bool TaskScheduler::ForceRunEachTask(const TimedTaskInfo& timedTaskInfo)
{
    if (timedTaskInfo.taskInfo.forceSynchronous || !mParallelExecutionAllowed)
    {
        timedTaskInfo.taskInfo.callback();
    }
    else
    {
        mParallelRunner->RunTask(timedTaskInfo.taskInfo);
    }
    return true;
}

void TaskScheduler::AddTimedTask(std::chrono::milliseconds duration, const TaskInfo& taskInfo)
{
    if (taskInfo.callback == nullptr)
    {
        std::cerr << "[TaskScheduler::AddTimedTask] callback is NULL!\n";
        return;
    }
    mContainer->Insert({ taskInfo, duration });
}

void TaskScheduler::AddTimedTask(std::chrono::seconds duration, const TaskInfo& taskInfo)
{
    if (taskInfo.callback == nullptr)
    {
        std::cerr << "[TaskScheduler::AddTimedTask] callback is NULL!\n";
        return;
    }
    mContainer->Insert({ taskInfo, duration });
}

void TaskScheduler::Terminate(bool finishTasks)
{
    if (finishTasks)
    {
        mContainer->ForEach(std::bind(&TaskScheduler::ForceRunEachTask, this, std::placeholders::_1));
        mContainer->PostIterate();
    }

    if (mParallelRunner != nullptr)
    {
        mParallelRunner->Terminate();
    }
    mRunning = false;
}
