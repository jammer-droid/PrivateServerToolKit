#version 450

layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 localPosition;
layout(location = 2) flat in uint shape;

layout(location = 0) out vec4 outColor;

const uint ShapeCircle = 1u;

void main()
{
    if (shape == ShapeCircle)
    {
        vec2 dist = localPosition - vec2(0.5);
        if (dot(dist, dist) > 0.25)
        {
            discard;
        }
    }
    outColor = vertexColor;
}
