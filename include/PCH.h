#pragma once
#define NOMINMAX

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <REL/Relocation.h>

#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cuchar>
#include <cwchar>
#include <cwctype>

#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <bitset>
#include <charconv>
#include <chrono>
#include <compare>
#include <complex>
#include <concepts>
#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>
#include <condition_variable>
#include <cstring>
#include <d3d11.h>
#include <deque>
#include <direct.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <exception>
#include <execution>
#include <filesystem>
#include <format>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <initializer_list>
#include <io.h>
#include <iomanip>
#include <iosfwd>
#include <ios>
#include <iostream>
#include <istream>
#include <iterator>
#include <latch>
#include <limits>
#include <locale>
#define MAGIC_ENUM_RANGE_MIN -128
#define MAGIC_ENUM_RANGE_MAX 2048
#include <magic_enum.hpp>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <numbers>
#include <numeric>
#include <optional>
#include <ostream>
#include <ppl.h>
#include <queue>
#include <random>
#include <ranges>
#include <regex>
#include <ratio>
#include <scoped_allocator>
#include <semaphore>
#include <set>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <sstream>
#include <stack>
#include <stddef.h>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
#include <syncstream>
#include <system_error>
#include <thread>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>
#include <version>
#include <wrl/client.h>
#include <xxhash.h>

#include <ShlObj_core.h>
#include <Windows.h>
#include <Psapi.h>
#undef cdecl // Workaround for Clang 14 CMake configure error.

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>

// Compatible declarations with other sample projects.
#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;
using namespace REL::literals;

namespace logger = SKSE::log;

namespace util {
    using SKSE::stl::report_and_fail;
}

#include "B/BSFaceGenBaseMorphExtraData.h"

#include "MiniBenchmark.h"

#include "RGBA.h"

#include "Store.h"
#include "Geometry.h"
#include "Common.h"
#include "InputManager.h"
#include "nifUtil.h"
#include "Utility.hpp"

#include "Condition.h"
#include "Config.h"

#include "Hook.h"
#include "ThreadPool.h"

#include "ShaderManager.h"

#include "ObjectNormalMapUpdater.h"
#include "ActorVertexHasher.h"
#include "TaskManager.h"
#include "PapyrusScripts.h"

#include "Interface/Interface.h"
#include "Interface/InterfaceImpl.h"

