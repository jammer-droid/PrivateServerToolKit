#include "WorldSandbox.h"

WorldSandbox::WorldSandbox()
{
    DrawItem2D rectangle{DrawShape2D::Rectangle, {}, {1.0f, 0.0f, 0.0f, 0.75f}, 1.0f};
    rectangle.geometry.rect = {50.0f, 50.0f, 100.0f, 100.0f};

    DrawItem2D line{DrawShape2D::Line, {}, {0.0f, 1.0f, 0.0f, 0.75f}, 20.0f};
    line.geometry.line = {100.0f, 300.0f, 150.0f, 50.0f};

    DrawItem2D circle{DrawShape2D::Circle, {}, {0.0f, 0.0f, 1.0f, 0.75f}, 1.0f};
    circle.geometry.rect = {300.0f, 100.0f, 50.0f, 150.0f};

    baseItems_ = {rectangle, line, circle};
    drawItems_.reserve(baseItems_.size() + MaxTrailPoints - 1);
    RebuildDrawData();
}

void WorldSandbox::Update()
{
    if (!baseItems_.empty())
    {
        GeometryData2D::RectData2D &rectangle = baseItems_.front().geometry.rect;
        rectangle.x += 1.0f;

        const Point2D center{rectangle.x + rectangle.width * 0.5f, rectangle.y + rectangle.height * 0.5f};
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

    if (baseItems_.empty())
    {
        return;
    }

    // 투명도 합성 순서를 유지하도록 궤적을 기본 도형보다 먼저 그린다.
    for (std::size_t index = 1; index < trailPoints_.size(); ++index)
    {
        const Point2D &start = trailPoints_[index - 1];
        const Point2D &end = trailPoints_[index];
        if (start == end)
        {
            continue;
        }

        DrawItem2D segment{DrawShape2D::Line, {}, {0.25f, 0.1f, 0.85f, 0.5f}, 8.0f};
        segment.geometry.line = {start[0], start[1], end[0], end[1]};
        drawItems_.push_back(segment);
    }

    drawItems_.insert(drawItems_.end(), baseItems_.begin(), baseItems_.end());
}
