#include "ThreadPool.h"

namespace Mus {
    TaskGroup::TaskGroup() : counter(0) {}

    void TaskGroup::addTask() {
        counter.fetch_add(1, std::memory_order_relaxed);
    }

    void TaskGroup::completeTask() {
        if (counter.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            std::unique_lock lock(mtx);
            cv.notify_all();
        }
    }

    void TaskGroup::wait() {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&] { return counter.load() == 0; });
    }

    ThreadPool& ThreadPool::GetSingleton() {
        static ThreadPool instance(Config::GetSingleton().GetPriorityCoreCount());
        return instance;
    }

    ThreadPool::ThreadPool(size_t numThreads)
        : stop(false)
    {
        mainWorker = std::make_unique<std::thread>([this] { mainWorkerLoop(); });

        for (size_t i = 0; i < numThreads; ++i) {
            workers.emplace_back([this] { workerLoop(); });
        }
    }

    ThreadPool::~ThreadPool() {
        stop.store(true);
        maincv.notify_all();
        mainWorker->join();
        cv.notify_all();
        for (auto& t : workers)
            if (t.joinable()) t.join();
    }

    void ThreadPool::mainWorkerLoop() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        SetThreadAffinityMask(GetCurrentThread(), Config::GetSingleton().GetPriorityCores());
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mainMutex);
                maincv.wait(lock, [this] { 
                    return stop.load() || mainTask != nullptr;
                });
                if (stop.load() && mainTask == nullptr)
                    return;
                task = std::move(*mainTask);
                mainTask = nullptr;
            }
            task();
        }
    }
    void ThreadPool::workerLoop() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        SetThreadAffinityMask(GetCurrentThread(), Config::GetSingleton().GetPriorityCores());
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

    void ThreadPool::onEvent(const FrameEvent& e)
    {
        auto now = std::clock();
        if (now - lastTickTime < Config::GetSingleton().GetTaskQTick())
            return;

        lastTickTime = now;
        tasksProcessed = 0;
        if (tasks.empty())
            return;

        mainTask = std::make_unique<std::function<void()>>([this]{
            std::lock_guard<std::mutex> lock(queueMutex);
            while (tasksProcessed < Config::GetSingleton().GetTaskQMaxCount() && !tasks.empty()) {
                currentTasks.push(std::move(tasks.front()));
                cv.notify_one();
                tasks.pop();
                tasksProcessed++;
            }
        });
        maincv.notify_one();
    }
}
