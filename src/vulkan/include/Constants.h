#pragma once

#include <cstdint>

static constexpr std::uint32_t kWorkGroupLocalSize = 16; // local_size_x = 16, local_size_y = 1, local_size_z = 1
static constexpr std::uint32_t kElementCount = 33;       // ComputeShader로 전달할 버퍼의 원소 개수
