#include "ThreadPool.h"

namespace Mus {
    std::unique_ptr<ThreadPool_ParallelModule> actorThreads;
    std::unique_ptr<ThreadPool_ParallelModule> bakingThreads;

    ThreadPool_ParallelModule::ThreadPool_ParallelModule(std::uint32_t threadSize)
        : stop(false), priorityCoreMask(Config::GetSingleton().GetPriorityCores())
    {
        std::uint32_t coreCount = std::max(std::uint32_t(1), threadSize);
        for (std::uint32_t i = 0; i < coreCount; i++) {
            workers.emplace_back([this] { workerLoop(); });
        }
    }

    ThreadPool_ParallelModule::~ThreadPool_ParallelModule() {
        stop.store(true);
        cv.notify_all();
        for (auto& t : workers)
            if (t.joinable()) t.join();
    }

    void ThreadPool_ParallelModule::workerLoop() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        if (priorityCoreMask > 0)
            SetThreadAffinityMask(GetCurrentThread(), priorityCoreMask);
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(queueMutex);
                cv.wait(lock, [this] {
                    return stop.load() || !tasks.empty();
                });
                if (stop.load() && tasks.empty())
                    return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

    std::unique_ptr<ThreadPool_TaskModule> cpuTask;
    std::unique_ptr<ThreadPool_TaskModule> gpuTask;

    ThreadPool_TaskModule::ThreadPool_TaskModule(std::uint8_t a_taskQTick, bool a_directTaskQ, std::uint8_t a_taskQMax)
        : stop(false), taskQTick(a_taskQTick)
        , directTaskQ(a_directTaskQ)
        , priorityCoreMask(Config::GetSingleton().GetPriorityCores())
        , taskQMaxCount(std::max(std::uint8_t(1), a_taskQMax))
    {
        mainWorker = std::make_unique<std::thread>([this] { mainWorkerLoop(); });
        for (std::uint8_t i = 0; i < taskQMaxCount + 1; i++) {
            workers.emplace_back([this] { workerLoop(); });
        }
    }

    ThreadPool_TaskModule::~ThreadPool_TaskModule() {
        stop.store(true);
        maincv.notify_all();
        mainWorker->join();
        cv.notify_all();
        for (auto& t : workers)
            if (t.joinable()) t.join();
    }

    void ThreadPool_TaskModule::mainWorkerLoop() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        if (priorityCoreMask > 0)
            SetThreadAffinityMask(GetCurrentThread(), priorityCoreMask);
        while (true) {
            {
                std::unique_lock lock(mainMutex);
                maincv.wait(lock, [this] { 
                    return stop.load() || mainTask;
                });
                if (stop.load())
                    return;
                std::lock_guard<std::mutex> qlg(queueMutex);
                while (currentTasks.size() < taskQMaxCount && !tasks.empty()) {
                    currentTasks.push(std::move(tasks.front()));
                    cv.notify_one();
                    tasks.pop();
                }
                mainTask = false;
            }
        }
    }
    void ThreadPool_TaskModule::workerLoop() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        if (priorityCoreMask > 0)
            SetThreadAffinityMask(GetCurrentThread(), priorityCoreMask);
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(queueMutex);
                cv.wait(lock, [this] { 
                    return stop.load() || !currentTasks.empty(); 
                });
                if (stop.load() && currentTasks.empty())
                    return;
                task = std::move(currentTasks.front());
                currentTasks.pop();
            }
            task();
        }
    }

    void ThreadPool_TaskModule::onEvent(const FrameEvent& e)
    {
        if (tasks.empty())
            return;
        if (currentTasks.size() >= taskQMaxCount)
            return;

        auto now = std::clock();
        if (now - lastTickTime < taskQTick)
            return;
        lastTickTime = now;

        mainTask = true;
        maincv.notify_one();
    }
}
