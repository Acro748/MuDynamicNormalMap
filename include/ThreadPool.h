#pragma once 

namespace Mus {
    class ThreadPool_ParallelModule
    {
    public:
        ThreadPool_ParallelModule() = delete;
        ThreadPool_ParallelModule(std::uint32_t threadSize);
        ~ThreadPool_ParallelModule();

        template<typename F, typename... Args>
        auto submitAsync(F&& f, Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            using RetType = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<RetType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
            std::future<RetType> res = task->get_future();
            {
                std::unique_lock lock(queueMutex);
                tasks.emplace([task]() { (*task)(); });
            }
            cv.notify_one();
            return res;
        }

        inline std::size_t GetThreads() const { return workers.size(); };
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable cv;

        std::atomic<bool> stop;

        std::clock_t lastTickTime = 0;
        int tasksProcessed = 0;

        const unsigned long priorityCoreMask = 0;

        void workerLoop();
    };
    extern std::unique_ptr<ThreadPool_ParallelModule> actorThreads;
    extern std::unique_ptr<ThreadPool_ParallelModule> memoryManageThreads;
    extern std::unique_ptr<ThreadPool_ParallelModule> processingThreads;

    class ThreadPool_GPUTaskModule
        : public IEventListener<FrameEvent>
    {
    public:
        ThreadPool_GPUTaskModule() = delete;
        ThreadPool_GPUTaskModule(std::uint8_t a_taskQTick, bool a_directTaskQ, std::uint8_t a_taskQMax);
        ~ThreadPool_GPUTaskModule();

        template<typename F, typename... Args>
        auto submitAsync(F&& f, Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            using RetType = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<RetType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );
            bool isCurrentTask = false;
            std::future<RetType> res = task->get_future();
            {
                std::unique_lock lock(queueMutex);
                if (directTaskQ && taskQMaxCount > currentTasks.size())
                {
                    if (tasks.empty())
                    {
                        currentTasks.emplace([task]() { (*task)(); });
                    }
                    else
                    {
                        tasks.emplace([task]() { (*task)(); });
                        currentTasks.push(std::move(tasks.front()));
                        tasks.pop();
                    }
                    isCurrentTask = true;
                }
                else
                {
                    tasks.emplace([task]() { (*task)(); });
                }
            }
            if (isCurrentTask)
            {
                cv.notify_one();
                lastTickTime = std::clock();
            }
            return res;
        }

        inline std::size_t GetThreads() const { return workers.size(); };
    protected:
        void onEvent(const FrameEvent& e) override;

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::queue<std::function<void()>> currentTasks;
        std::mutex queueMutex;
        std::condition_variable cv;

        std::atomic<bool> stop;

        std::clock_t lastTickTime = 0;

        const unsigned long priorityCoreMask = 0;
        const std::clock_t taskQTick = 0;
        const std::uint8_t taskQMaxCount = 1;
        const bool directTaskQ = false;

        Microsoft::WRL::ComPtr<ID3D11Query> query;
        bool getQuery();
        bool isQueryDone();

        void workerLoop();
    };
    extern std::unique_ptr<ThreadPool_GPUTaskModule> gpuTask;
}