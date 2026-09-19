#pragma once

#include "runtime/app/IGame.h"
#include "runtime/renderer/DrawData2D.h"

#include "ecs/EntityManager.h"

#include <glm/glm.hpp>

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

    static constexpr std::size_t MaxTrailPoints = 64;

  private:
    EntityManager entityManager_;
    EntityId rectEntityId_{0};

    std::vector<EntityId> sandboxEntities;

    DrawItem2D rectangle_;
    DrawItem2D circle_;
    DrawItem2D line_;
    std::deque<glm::vec2> trailPoints_;
    std::vector<DrawItem2D> drawItems_;
};
