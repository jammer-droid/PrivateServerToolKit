#include "WorldSandbox.h"

WorldSandbox::WorldSandbox()
{
    rectangle_.geometry.rect = {{50.0f, 50.0f}, {100.0f, 100.0f}};
    rectangle_.color = {1.0f, 0.0f, 0.0f, 0.75f};

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
    rectangle_.geometry.rect.position += glm::vec2{1.0f, 0.0f};
    const glm::vec2 center = rectangle_.geometry.rect.position + rectangle_.geometry.rect.size * 0.5f;
    if (trailPoints_.size() >= MaxTrailPoints)
    {
        trailPoints_.pop_front();
    }
    trailPoints_.push_back(center);

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

    drawItems_.push_back(rectangle_);
    drawItems_.push_back(line_);
    drawItems_.push_back(circle_);
}
