#include "ThreadPool.h"

namespace Mus {
    std::unique_ptr<ThreadPool_ParallelModule> actorThreads;
    std::unique_ptr<ThreadPool_ParallelModule> memoryManageThreads;
    std::unique_ptr<ThreadPool_ParallelModule> updateThreads;
    std::unique_ptr<ThreadPool_ParallelModule> processingThreads;

    ThreadPool_ParallelModule::ThreadPool_ParallelModule(std::uint32_t threadSize)
        : stop(false)
    {
        std::uint32_t coreCount = std::max(1u, threadSize);
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

    std::unique_ptr<ThreadPool_GPUTaskModule> gpuTask;

    ThreadPool_GPUTaskModule::ThreadPool_GPUTaskModule(std::uint32_t a_threadSize, std::clock_t a_taskQTick, bool a_directTaskQ, bool a_waitPreTask)
        : stop(false), taskQTick(a_taskQTick)
        , directTaskQ(a_directTaskQ)
		, waitPreTask(a_waitPreTask)
        , runTask(true), running(false)
    {
        std::uint32_t threadCount = std::max(1u, a_threadSize);
        for (std::uint8_t i = 0; i < threadCount; i++) {
            lastTickTime.push_back(currentTime);
            runTask.push_back(true);
            running.push_back(false);
            workers.emplace_back([this, i] { workerLoop(i); });
        }
    }

    ThreadPool_GPUTaskModule::~ThreadPool_GPUTaskModule() {
        stop.store(true);
        cv.notify_all();
        for (auto& t : workers)
            if (t.joinable()) t.join();
    }

    void ThreadPool_GPUTaskModule::workerLoop(std::uint32_t threadNum) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(queueMutex);
                cv.wait(lock, [this, threadNum] {
                    return stop.load() || (!tasks.empty() && (runTask[threadNum] || directTaskQ));
                });
                runTask[threadNum] = false;
                running[threadNum] = true;
                if (stop.load() && tasks.empty())
                    return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
			if (waitPreTask)
                WaitForGPU(Shader::ShaderManager::GetSingleton().GetDevice(), Shader::ShaderManager::GetSingleton().GetContext()).Wait();
            lastTickTime[threadNum] = currentTime;
            running[threadNum] = false;
        }
    }

    void ThreadPool_GPUTaskModule::onEvent(const FrameEvent& e)
    {
        if (tasks.empty())
            return;
        std::uint32_t notified = 0;
        for (std::uint32_t i = 0; i < GetThreads(); i++)
        {
            if (running[i])
                continue;
            if (!directTaskQ) 
            {
                if (lastTickTime[i] + taskQTick > currentTime)
                    continue;
            }
            runTask[i] = true;
            notified++;
        }
        if (notified == 0)
            return;
        cv.notify_all();
    }
}
