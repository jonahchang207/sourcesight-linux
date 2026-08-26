#pragma once

#define NOMINMAX
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// If you see dependency errors here, you didn't read README.md properly
#include <Logger.hpp>

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdint>
using DWORD = std::uint32_t;
using DWORD64 = std::uint64_t;
using WORD = std::uint16_t;
using byte = unsigned char;
using LPVOID = void*;
struct RECT { long left, top, right, bottom; };
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#endif

#include <map>
#include <array>
#include <memory>
#include <stack>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <future>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <functional>

#include <imgui.h>
#include <string_view>

#include <nlohmann/json.hpp>

using namespace al;

#include "core/logger/LogHelper.hpp"
#include "core/engine/types/Types.hpp"
#include "config/Current.hpp"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
