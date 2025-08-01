#include "ThreadPool.h"

namespace Mus {
    std::unique_ptr<ThreadPool_ParallelModule> actorThreads;
    std::unique_ptr<ThreadPool_ParallelModule> memoryManageThreads;
    std::unique_ptr<ThreadPool_ParallelModule> processingThreads;

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
            std::this_thread::yield();
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

    ThreadPool_GPUTaskModule::ThreadPool_GPUTaskModule(std::uint8_t a_taskQTick, bool a_directTaskQ, std::uint8_t a_taskQMax)
        : stop(false), taskQTick(a_taskQTick)
        , directTaskQ(a_directTaskQ)
        , priorityCoreMask(Config::GetSingleton().GetPriorityCores())
        , taskQMaxCount(std::max(std::uint8_t(1), a_taskQMax))
    {
        if (taskQMaxCount == 1)
        {
            if (auto device = Shader::ShaderManager::GetSingleton().GetDevice(); device)
            {
                D3D11_QUERY_DESC queryDesc = {};
                queryDesc.Query = D3D11_QUERY_EVENT;
                queryDesc.MiscFlags = 0;

                HRESULT hr = device->CreateQuery(&queryDesc, &query);
                if (FAILED(hr)) {
                    query = nullptr;
                }
            }
        }
        for (std::uint8_t i = 0; i < taskQMaxCount + 1; i++) {
            workers.emplace_back([this] { workerLoop(); });
        }
    }

    ThreadPool_GPUTaskModule::~ThreadPool_GPUTaskModule() {
        stop.store(true);
        cv.notify_all();
        for (auto& t : workers)
            if (t.joinable()) t.join();
    }

    bool ThreadPool_GPUTaskModule::getQuery()
    {
        if (!query)
            return false;
        Shader::ShaderManager::GetSingleton().ShaderContextLock();
        Shader::ShaderManager::GetSingleton().GetContext()->End(query.Get());
        Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
        return true;
    }
    bool ThreadPool_GPUTaskModule::isQueryDone()
    {
        if (!query)
            return true;
        Shader::ShaderManager::GetSingleton().ShaderContextLock();
        HRESULT hr = Shader::ShaderManager::GetSingleton().GetContext()->GetData(query.Get(), nullptr, 0, 0);
        Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
        return FAILED(hr) || hr == S_OK;
    }

    void ThreadPool_GPUTaskModule::workerLoop() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        if (priorityCoreMask > 0)
            SetThreadAffinityMask(GetCurrentThread(), priorityCoreMask);
        while (true) {
            std::this_thread::yield();
            std::function<void()> task;
            {
                std::unique_lock lock(queueMutex);
                cv.wait(lock, [this] { 
                    return stop.load() || (!currentTasks.empty() && isQueryDone());
                });
                if (stop.load() && currentTasks.empty())
                    return;
                task = std::move(currentTasks.front());
                currentTasks.pop();
            }
            task();
            getQuery();
        }
    }

    void ThreadPool_GPUTaskModule::onEvent(const FrameEvent& e)
    {
        if (!isQueryDone())
        {
            lastTickTime = currentTime;
            return;
        }

        if (!currentTasks.empty())
            cv.notify_one();
        if (tasks.empty())
            return;
        if (currentTasks.size() >= taskQMaxCount)
            return;

        if (currentTime - lastTickTime < taskQTick)
            return;
        lastTickTime = currentTime;

        std::lock_guard<std::mutex> qlg(queueMutex);
        while (currentTasks.size() < taskQMaxCount && !tasks.empty()) {
            currentTasks.push(std::move(tasks.front()));
            tasks.pop();
            cv.notify_one();
        }
    }
}
