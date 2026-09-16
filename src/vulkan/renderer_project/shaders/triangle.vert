#version 450

layout(location = 0) out vec4 vertexColor;

layout(push_constant, std430) uniform DrawParameters
{
    vec4 positionAndSize;
    vec4 viewportSize;
    vec4 color;
} drawParameters;

// 삼각형 도형의 로컬 좌표계 기준
const vec2 positions[9] = vec2[](
    // 삼각형: index 0 ~ 2
        vec2(0.5, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0),

    // 사각형: index 3 ~ 8
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),

        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

void main()
{
    vec2 pixelPos = drawParameters.positionAndSize.xy + positions[gl_VertexIndex] * drawParameters.positionAndSize.zw;

    vec2 ndc = pixelPos / drawParameters.viewportSize.xy * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    vertexColor = drawParameters.color;
}
