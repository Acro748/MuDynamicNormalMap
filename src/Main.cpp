
namespace {
    /**
     * Setup logging.
     *
     * <p>
     * Logging is important to track issues. CommonLibSSE bundles functionality for spdlog, a common C++ logging
     * framework. Here we initialize it, using values from the configuration file. This includes support for a debug
     * logger that shows output in your IDE when it has a debugger attached to Skyrim, as well as a file logger which
     * writes data to the standard SKSE logging directory at <code>Documents/My Games/Skyrim Special Edition/SKSE</code>
     * (or <code>Skyrim VR</code> if you are using VR).
     * </p>
     */
    void InitializeLogging() 
    {
        std::string LogDirectory = Mus::GetRuntimeSKSEDirectory();
        
        std::optional<std::filesystem::path> path(LogDirectory);
        *path /= "MuDynamicNormalMap";
        *path += L".log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) 
        {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } 
        else 
        {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }

        log->set_level(Mus::Config::GetSingleton().GetLogLevel());
        log->flush_on(Mus::Config::GetSingleton().GetFlushLevel());

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
    }

    /**
     * Initialize the SKSE cosave system for our plugin.
     *
     * <p>
     * SKSE comes with a feature called a <em>cosave</em>, an additional save file kept alongside the original Skyrim
     * save file. SKSE plugins can write their own data to this file, and load it again when the save game is loaded,
     * allowing them to keep custom data along with a player's save. Each plugin must have a unique ID, which is four
     * characters long (similar to the record names used by forms in ESP files). Note however this is little-endian, so
     * technically the 'SMPL' here ends up as 'LPMS' in the save file, unless we use a byte order swap.
     * </p>
     *
     * <p>
     * There can only be one serialization callback for save, revert (called on new game and before a load), and load
     * for the entire plugin.
     * </p>
     */
    void AllSave(SKSE::SerializationInterface* serde)
    {

    }

    void AllLoad(SKSE::SerializationInterface* serde)
    {
        logger::info("save data loading...");

        std::uint32_t type;
        std::uint32_t size;
        std::uint32_t version;
        while (serde->GetNextRecordInfo(type, version, size)) {

        }
        logger::info("save data loaded");
    }

    void InitializeSerialization() 
    {
        logger::trace("Initializing cosave serialization...");
        auto* serde = SKSE::GetSerializationInterface();
        serde->SetUniqueID(_byteswap_ulong('MDTT'));
        serde->SetSaveCallback(AllSave);
        serde->SetLoadCallback(AllLoad);
    }

    /**
     * Initialize our Papyrus extensions.
     *
     * <p>
     * A common use of SKSE is to add new Papyrus functions. You can call a registration callback to do this. This
     * callback will not necessarily be called immediately, if the Papyrus VM has not been initialized yet (in that case
     * it's execution is delayed until the VM is available).
     * </p>
     *
     * <p>
     * You can call the <code>Register</code> function as many times as you want and at any time you want to register
     * additional functions.
     * </p>
     */
    void InitializePapyrus() 
    {
        logger::trace("Initializing Papyrus binding...");
        if (SKSE::GetPapyrusInterface()->Register(Mus::Papyrus::RegisterPapyrusFunctions))
        {
            logger::debug("Papyrus functions bound.");
        } 
        else 
        {
            SKSE::stl::report_and_fail("Failure to register Papyrus bindings.");
        }
    }

    /**
     * Initialize the trampoline space for function hooks.
     *
     * <p>
     * Function hooks are one of the most powerful features available to SKSE developers, allowing you to replace
     * functions with your own, or replace a function call with a call to another function. However, to do this, we
     * need a code snippet that replicates the first instructions of the original code we overwrote, in order to be
     * able to call back to the original control flow with all the same functionality.
     * </p>
     *
     * <p>
     * CommonLibSSE comes with functionality to allocate trampoline space, including a common singleton space we can
     * access from anywhere. While this is not necessarily the most advanced use of trampolines and hooks, this will
     * suffice for our demo project.
     * </p>
     */
    void InitializeHooking() 
    {
        logger::trace("Building hook...");

        Mus::hook();
        Mus::TaskManager::GetSingleton().Init(false);
    }

    void InitializeInterface()
    {
        if (!SKSE::GetMessagingInterface()->RegisterListener(NULL, [](SKSE::MessagingInterface::Message* message)
            {
                switch (message->type)
                {
                case MDNM::InterfaceExchangeMessage::kMessage_ExchangeInterface:
                {
                    MDNM::InterfaceExchangeMessage* exchangeMessage = (MDNM::InterfaceExchangeMessage*)message->data;
                    exchangeMessage->Interface = &MDNM::DNM;
                    break;
                }
                default:
                    break;
                }
            })) {
            logger::critical("Couldn't get MessagingInterface for API");
        }
    }
    void kPostLoadFunction()
    {
        InitializeInterface();
    }
    void kDataloadedFunction()
    {
        Mus::ConditionManager::GetSingleton().InitialConditionMap();
        static_cast<Mus::MultipleConfig*>(&Mus::Config::GetSingleton())->LoadConditionFile();
        Mus::ConditionManager::GetSingleton().SortConditions();

        if (Mus::Config::GetSingleton().GetAutoTaskQ() > 0)
        {
            float benchMarkResult = Mus::miniBenchMark();
            //55~high-end, 35~middle-end, 15~low-end
            logger::info("CPU bench mark score : {}", (std::uint32_t)benchMarkResult);
            float CPUPerformanceMult = std::max(1.0f, 55.0f / benchMarkResult);

            switch (Mus::Config::GetSingleton().GetAutoTaskQ()) {
            case Mus::Config::AutoTaskQList::Fastest:
                Mus::Config::GetSingleton().SetTaskQMax(8);
                Mus::Config::GetSingleton().SetTaskQTick(0);
                Mus::Config::GetSingleton().SetDirectTaskQ(true);
                Mus::Config::GetSingleton().SetDivideTaskQ(0);
                break;
            case Mus::Config::AutoTaskQList::Faster:
                Mus::Config::GetSingleton().SetTaskQMax(2);
                Mus::Config::GetSingleton().SetTaskQTick(Mus::TaskQTickBase * CPUPerformanceMult);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(0);
                break;
            case Mus::Config::AutoTaskQList::Balanced:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTick(Mus::TaskQTickBase * CPUPerformanceMult);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(0);
                break;
            case Mus::Config::AutoTaskQList::BetterPerformance:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTick(Mus::TaskQTickBase * CPUPerformanceMult);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(1);
                break;
            case Mus::Config::AutoTaskQList::BestPerformance:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTick(Mus::TaskQTickBase * CPUPerformanceMult);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(2);
                break;
            default:
                break;
            }
        }

        Mus::cpuTask = std::make_unique<Mus::ThreadPool_TaskModule>(Mus::Config::GetSingleton().GetTaskQTick(), Mus::Config::GetSingleton().GetDirectTaskQ(), Mus::Config::GetSingleton().GetTaskQMax());
        Mus::gpuTask = std::make_unique<Mus::ThreadPool_TaskModule>(0, Mus::Config::GetSingleton().GetDirectTaskQ(), Mus::Config::GetSingleton().GetTaskQMax());

        Mus::weldDistance = Mus::Config::GetSingleton().GetWeldDistance();
        Mus::weldDistanceMult = 1.0f / Mus::weldDistance;
		Mus::g_frameEventDispatcher.addListener(&Mus::TaskManager::GetSingleton());
		Mus::g_frameEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
		Mus::g_frameEventDispatcher.addListener(Mus::cpuTask.get());
        Mus::g_frameEventDispatcher.addListener(Mus::gpuTask.get());

        if (Mus::Config::GetSingleton().GetRealtimeDetectOnBackGround())
        {
            Mus::g_armorAttachEventEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
            Mus::g_facegenNiNodeEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
            Mus::g_actorChangeHeadPartEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
            Mus::ActorVertexHasher::GetSingleton().Init();
        }

		Mus::g_armorAttachEventEventDispatcher.addListener(&Mus::TaskManager::GetSingleton());
		Mus::g_facegenNiNodeEventDispatcher.addListener(&Mus::TaskManager::GetSingleton());
		Mus::g_actorChangeHeadPartEventDispatcher.addListener(&Mus::TaskManager::GetSingleton());
        Mus::TaskManager::GetSingleton().Init(true);

        auto coreCount = Mus::Config::GetSingleton().GetPriorityCoreCount();

        std::uint32_t actorThreads = std::max(2.0f, ceil((float)coreCount / 4.0f));
        Mus::actorThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(actorThreads);
        logger::info("set actorThreads {}", actorThreads);

        std::uint32_t bakingThreads = std::max(2.0f, ceil((float)coreCount / 4.0f * 3.0f));
        Mus::bakingThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(bakingThreads);
        logger::info("set bakingThreads {}", bakingThreads);
    }

    void kNewGameFunction()
    {
        logger::info("detected new game...");
    }

    void InitializeMessaging() 
    {
        if (!SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) 
        {
            switch (message->type) 
            {
                // Skyrim lifecycle events.
                case SKSE::MessagingInterface::kPostLoad: // Called after all plugins have finished running SKSEPlugin_Load.
                    // It is now safe to do multithreaded operations, or operations against other plugins.
                    kPostLoadFunction();
                    break;
                case SKSE::MessagingInterface::kPostPostLoad: // Called after all kPostLoad message handlers have run.
                    break;
                case SKSE::MessagingInterface::kInputLoaded: // Called when all game data has been found.
                    break;
                case SKSE::MessagingInterface::kDataLoaded: // All ESM/ESL/ESP plugins have loaded, main menu is now active.
                    // It is now safe to access form data.
                    kDataloadedFunction();
                    break;

                // Skyrim game events.
                case SKSE::MessagingInterface::kNewGame: // Player starts a new game from main menu.
                    kNewGameFunction();
                    break;
                case SKSE::MessagingInterface::kPreLoadGame: // Player selected a game to load, but it hasn't loaded yet.
                    Mus::IsSaveLoading.store(true);
                    // Data will be the name of the loaded save.
                    break;
                case SKSE::MessagingInterface::kPostLoadGame: // Player's selected save game has finished loading.
                    Mus::IsSaveLoading.store(false);
                    // Data will be a boolean indicating whether the load was successful.
                    break;
                case SKSE::MessagingInterface::kSaveGame: // The player has saved a game.
                    // Data will be the save name.
                    break;
                case SKSE::MessagingInterface::kDeleteGame: // The player deleted a saved game from within the load menu.
                    break;
            }
        })) {
            SKSE::stl::report_and_fail("Unable to register message listener.");
        }
    }
}

/**
 * This if the main callback for initializing your SKSE plugin, called just before Skyrim runs its main function.
 *
 * <p>
 * This is your main entry point to your plugin, where you should initialize everything you need. Many things can't be
 * done yet here, since Skyrim has not initialized and the Windows loader lock is not released (so don't do any
 * multithreading). But you can register to listen for messages for later stages of Skyrim startup to perform such
 * tasks.
 * </p>
 */


SKSEPluginLoad(const SKSE::LoadInterface* skse) 
{
    Mus::Config::GetSingleton().LoadLogging();
    InitializeLogging();

    auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    auto version = plugin->GetVersion();
    logger::info("{} {} is loading...", plugin->GetName(), version);

    auto runtime = REL::Module::get().version();
    logger::info("Working on skyrim version : {}.{}.{}.{}", runtime.major(), runtime.minor(), runtime.patch(), runtime.build());

    Mus::Config::GetSingleton().LoadConfig();

    Init(skse);
    InitializeMessaging();
    InitializeSerialization();
    InitializePapyrus();
    InitializeHooking();

    logger::info("{} has finished loading.", plugin->GetName());
    return true;
}
