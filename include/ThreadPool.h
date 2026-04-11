#pragma once 
#include <tbb/task_arena.h>
#include <tbb/task_scheduler_observer.h>

namespace Mus {
    class TBB_CoreMasking : public tbb::task_scheduler_observer {
        DWORD_PTR mask;

    public:
        TBB_CoreMasking(tbb::task_arena& arena, DWORD_PTR m)
            : tbb::task_scheduler_observer(arena), mask(m) {
            observe(true);
        }

        void on_scheduler_entry(bool worker) override {
            if (worker) {
                static thread_local bool pinned = false;
                if (!pinned) {
                    SetThreadAffinityMask(GetCurrentThread(), mask);
                    pinned = true;
                }
            }
        }
    };

    class TBB_ThreadPool
    {
    public:
        TBB_ThreadPool() = delete;
        TBB_ThreadPool(std::uint32_t a_threadSize, std::uint64_t a_coreMask)
            : workers(std::make_unique<tbb::task_arena>(a_threadSize)) {
            if (a_coreMask != 0)
                observer = std::make_unique<TBB_CoreMasking>(*workers, a_coreMask);
        }

        template <typename F>
        void Execute(F&& f) {
            workers->execute(std::forward<F>(f));
        }

        template <typename F>
        void Enqueue(F&& f) {
            workers->enqueue(std::forward<F>(f));
        }

        std::int32_t GetThreadSize() const { return workers->max_concurrency(); }
    private:
        std::unique_ptr<tbb::task_arena> workers;
        std::unique_ptr<TBB_CoreMasking> observer;
    };
    extern std::atomic<std::shared_ptr<TBB_ThreadPool>> currentProcessingThreads;
    extern std::shared_ptr<TBB_ThreadPool> processingThreads;
    extern std::shared_ptr<TBB_ThreadPool> processingThreadsFull;

    class ThreadPool_ParallelModule
    {
    public:
        ThreadPool_ParallelModule() = delete;
        ThreadPool_ParallelModule(std::uint32_t threadSize, std::uint64_t a_coreMask);
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
        inline std::int32_t GetThreadIndex(const std::uint64_t threadID) const {
            const auto it = std::find(threadIDs.cbegin(), threadIDs.cend(), threadID);
            return it != threadIDs.cend() ? std::distance(threadIDs.cbegin(), it) : -1;
        };
    private:
        const std::uint64_t coreMask = 0;

        std::vector<std::thread> workers;
        std::vector<std::uint64_t> threadIDs;
        std::queue<std::function<void()>> tasks;
        std::mutex queueMutex;
        std::condition_variable cv;
        std::atomic<bool> stop;

        void workerLoop();
    };
    extern std::atomic<std::shared_ptr<ThreadPool_ParallelModule>> currentActorThreads;
    extern std::shared_ptr<ThreadPool_ParallelModule> actorThreads;
    extern std::shared_ptr<ThreadPool_ParallelModule> actorThreadsFull;

    extern std::unique_ptr<ThreadPool_ParallelModule> backGroundWorkerThreads;

    class ThreadPool_GPUTaskModule
        : public IEventListener<FrameEvent>
    {
    public:
        ThreadPool_GPUTaskModule() = delete;
        ThreadPool_GPUTaskModule(std::uint32_t a_threadSize, std::uint64_t a_coreMask, std::clock_t a_taskQTick, bool a_directTaskQ);
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
        const std::uint64_t coreMask = 0;

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