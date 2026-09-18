#pragma once

class IGame
{
  public:
    virtual ~IGame() = default;

    // Application::Run에서 사용
    virtual void Update() = 0;
};
