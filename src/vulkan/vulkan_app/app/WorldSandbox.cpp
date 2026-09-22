#include "WorldSandbox.h"

#include "ecs/MovementSystem.h"

constexpr float SimulationTimeStep = 1.0f / 60.0f;

WorldSandbox::WorldSandbox()
{
    rectangle_.geometry.rect.size = {100.0f, 100.0f};
    rectangle_.color = {1.0f, 0.5f, 0.0f, 0.75f};

    SparseHandle id = entityManager_.CreateEntity();
    Entity *entity = entityManager_.FindEntity(id);

    entity->transform = Transform2D{glm::vec2{50.0f, 50.0f}};
    entity->velocity = Velocity2D{glm::vec2{60.0f, 0.0f}};
    rectEntityId_ = id;

    line_.shape = DrawShape2D::Line;
    line_.geometry = GeometryData2D{GeometryData2D::LineData2D{{100.0f, 300.0f}, {150.0f, 50.0f}}};
    line_.color = {0.0f, 1.0f, 0.0f, 0.75f};
    line_.thickness = 20.0f;

    circle_.shape = DrawShape2D::Circle;
    circle_.geometry.rect = {{300.0f, 100.0f}, {50.0f, 150.0f}};
    circle_.color = {0.0f, 0.0f, 1.0f, 0.75f};

    drawItems_.reserve(3 + MaxTrailPoints - 1);
    RebuildDrawData();
}

void WorldSandbox::Update()
{
    // 실제 프레임 시간이 아니라 Update 한 번에 적용하는 임시 시뮬레이션 간격이다.
    UpdateMovement(entityManager_, SimulationTimeStep);
    entityManager_.FlushDestroyed();

    const Entity *entity = entityManager_.FindEntity(rectEntityId_);
    if (entity != nullptr && entity->transform.has_value())
    {
        const glm::vec2 center = entity->transform->position + rectangle_.geometry.rect.size * 0.5f;
        if (trailPoints_.size() >= MaxTrailPoints)
        {
            trailPoints_.pop_front();
        }
        trailPoints_.push_back(center);
    }

    RebuildDrawData();
}

DrawData2DView WorldSandbox::GetDrawData() const
{
    return {drawItems_.data(), drawItems_.size()};
}

void WorldSandbox::RebuildDrawData()
{
    drawItems_.clear();

    // 투명도 합성 순서를 유지하도록 궤적을 기본 도형보다 먼저 그린다.
    for (std::size_t index = 1; index < trailPoints_.size(); ++index)
    {
        const glm::vec2 &start = trailPoints_[index - 1];
        const glm::vec2 &end = trailPoints_[index];
        if (start == end)
        {
            continue;
        }

        DrawItem2D segment{};
        segment.shape = DrawShape2D::Line;
        segment.geometry = GeometryData2D{GeometryData2D::LineData2D{start, end}};
        segment.color = {0.25f, 0.1f, 0.85f, 0.5f};
        segment.thickness = 8.0f;
        drawItems_.push_back(segment);
    }

    const Entity *entity = entityManager_.FindEntity(rectEntityId_);
    if (entity != nullptr && !entity->pendingDestroy && entity->transform.has_value())
    {
        DrawItem2D rectangle = rectangle_;
        rectangle.geometry.rect.position = entity->transform->position;
        drawItems_.push_back(rectangle);
    }
    drawItems_.push_back(line_);
    drawItems_.push_back(circle_);
}
