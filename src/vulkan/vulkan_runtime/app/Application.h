#pragma once

#include "VulkanRuntimeExport.h"

#include "common/ClassTraits.h"

class IGame;

struct ApplicationConfig
{
    // 생성자 호출 동안 유효한 경로. Application이 내부 저장소로 복사한다.
    const char *shaderDirectory{nullptr};
};

class Application
{
  public:
    // game은 Application보다 오래 살아야 한다. 소유권을 이전하지 않는다.
    VULKAN_RUNTIME_API Application(const ApplicationConfig &config, IGame &game);
    VULKAN_RUNTIME_API ~Application() noexcept;

    VK_NON_COPYABLE(Application)
    VK_NON_MOVABLE(Application)

    // 메인 스레드에서 호출하며, 반환할 때까지 해당 스레드에서 게임을 갱신한다.
    // 게임 예외는 GPU 사용 자원을 안전하게 정리한 뒤 호출자에게 전달한다.
    VULKAN_RUNTIME_API void Run();

  private:
    struct Impl;
    // runtime 내부에서 생성·파괴하는 소유 포인터다.
    Impl *impl_{nullptr};
};
