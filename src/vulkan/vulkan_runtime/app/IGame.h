#pragma once

#include "renderer/DrawData2D.h"

class IGame
{
  public:
    virtual ~IGame() = default;

    // Application::Run에서 사용
    virtual void Update() = 0;

    // Game이 소유한 DrawData View를 반환.
    // 다음 Update 호출 전까지 유효해야 한다.
    virtual DrawData2DView GetDrawData() const = 0;
};
