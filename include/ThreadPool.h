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

        void workerLoop();
    };
    extern std::unique_ptr<ThreadPool_ParallelModule> actorThreads;
    extern std::unique_ptr<ThreadPool_ParallelModule> memoryManageThreads;
    extern std::unique_ptr<ThreadPool_ParallelModule> updateThreads;
    extern std::unique_ptr<ThreadPool_ParallelModule> processingThreads;

    class ThreadPool_GPUTaskModule
        : public IEventListener<FrameEvent>
    {
    public:
        ThreadPool_GPUTaskModule() = delete;
        ThreadPool_GPUTaskModule(std::uint32_t a_threadSize, std::clock_t a_taskQTick, bool a_directTaskQ);
        ~ThreadPool_GPUTaskModule();

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
    protected:
        void onEvent(const FrameEvent& e) override;

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable cv;
        std::atomic<bool> stop;

        const std::clock_t taskQTick = 0;
        const bool directTaskQ = false;

        std::vector<std::clock_t> lastTickTime;
        std::vector<bool> runTask;
        std::vector<bool> running;

        void workerLoop(std::uint32_t threadNum);
    };
    extern std::unique_ptr<ThreadPool_GPUTaskModule> gpuTask;
}