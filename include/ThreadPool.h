#pragma once 

namespace Mus {
    class TaskGroup {
    public:
        TaskGroup();
        void addTask();
        void completeTask();
        void wait();

    private:
        std::atomic<int> counter;
        std::mutex mtx;
        std::condition_variable cv;
    };

    class ThreadPool 
        : public IEventListener<FrameEvent>
    {
    public:
        ThreadPool(size_t numThreads);
        ~ThreadPool();

        static ThreadPool& GetSingleton();

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
            return res;
        }

        template<typename F, typename... Args>
        void submitSync(F&& f, Args&&... args)
        {
            auto fut = submitAsync(std::forward<F>(f), std::forward<Args>(args)...);
            fut.get();
        }

        template<typename F, typename... Args>
        void submit(TaskGroup& group, F&& f, Args&&... args)
        {
            group.addTask();
            submitAsync([&group, func = std::bind(std::forward<F>(f), std::forward<Args>(args)...)]() mutable {
                func();
                group.completeTask();
            });
        }

        template<typename TaskRange>
        void submitGroupAndWait(TaskRange&& tasks)
        {
            TaskGroup group;
            for (auto&& task : tasks)
                submit(group, std::move(task));
            group.wait();
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

        void mainWorkerLoop();
        void workerLoop();
    };
}