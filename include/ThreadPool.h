#pragma once 

namespace Mus {
    class ThreadPool_ParallelModule
    {
    public:
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
    extern std::unique_ptr<ThreadPool_ParallelModule> bakingThreads;

    class ThreadPool_TaskModule
        : public IEventListener<FrameEvent>
    {
    public:
        ThreadPool_TaskModule();
        ~ThreadPool_TaskModule();

        static ThreadPool_TaskModule& GetSingleton();

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
                    currentTasks.emplace([task]() { (*task)(); });
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

    protected:
        void onEvent(const FrameEvent& e) override;

    private:
        std::unique_ptr<std::thread> mainWorker;
        std::unique_ptr<std::function<void()>> mainTask;
        std::mutex mainMutex;
        std::condition_variable maincv;

        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::queue<std::function<void()>> currentTasks;
        std::mutex queueMutex;
        std::condition_variable cv;

        std::atomic<bool> stop;

        std::clock_t lastTickTime = 0;
        int tasksProcessed = 0;

        const unsigned long priorityCoreMask = 0;
        const std::clock_t taskQTick = 0;
        const std::uint8_t taskQMaxCount = 1;
        const bool directTaskQ = false;

        void mainWorkerLoop();
        void workerLoop();
    };
}