#pragma once

#include "app/IGame.h"

#include <array>
#include <cstddef>
#include <deque>
#include <vector>

class WorldSandbox final : public IGame
{
  public:
    WorldSandbox();
    virtual ~WorldSandbox() override = default;

    virtual void Update() override;

    virtual DrawData2DView GetDrawData() const override;

  private:
    void RebuildDrawData();

    using Point2D = std::array<float, 2>;
    static constexpr std::size_t MaxTrailPoints = 64;

    std::vector<DrawItem2D> baseItems_;
    std::deque<Point2D> trailPoints_;
    std::vector<DrawItem2D> drawItems_;
};
