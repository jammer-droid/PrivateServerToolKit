#pragma once

#include <cstdint>

static constexpr std::uint32_t kWorkGroupLocalSize = 16; // local_size_x = 16, local_size_y = 1, local_size_z = 1
static constexpr std::uint32_t kElementCount = 32;       // Compute Shader에서 처리할 원소 수
static constexpr std::uint32_t kBufferCapacity = 1024;   // Buffer에 저장할 원소 수
static constexpr std::uint32_t kMultiplier = 3;          // Compute Shader 내부 multiplier
