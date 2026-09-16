#include "GraphicsPipeline.h"

#include "common/VulkanException.h"

#include "core/ShaderModule.h"

namespace
{

VkPipelineShaderStageCreateInfo CreatePipelineShaderStageCI(VkShaderStageFlagBits stage, VkShaderModule module,
                                                            const char *name)
{
    VkPipelineShaderStageCreateInfo shaderStageCI{};
    shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCI.stage = stage;
    shaderStageCI.module = module;
    shaderStageCI.pName = name;

    return shaderStageCI;
};

}; // namespace

GraphicsPipeline::GraphicsPipeline(VkDevice device, VkFormat colorFormat, const std::filesystem::path &shaderPath)
{
    // 1. PipelineLayout
    //      - Shader에 전달할 Descriptor Set과 Push Constants 정의
    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 0;         // descriptor set layout 개수
    pipelineLayoutCI.pushConstantRangeCount = 0; // push constant range 개수

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    pipelineLayoutHandle_.Adopt(pipelineLayout, deleter::VkPipelineLayoutDeleter{device});

    // 2. Pipeline
    //      - 셰이더와 렌더링 상태 구성
    ShaderModule vertexShader(device, shaderPath / "triangle.vert.spv");
    ShaderModule fragmentShader(device, shaderPath / "triangle.frag.spv");

    VkPipelineShaderStageCreateInfo shaderStageCI[2] = {
        CreatePipelineShaderStageCI(VK_SHADER_STAGE_VERTEX_BIT, vertexShader.Get(), "main"),
        CreatePipelineShaderStageCI(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader.Get(), "main")};

    // 버텍스 버퍼 데이터를 어떤 구조로 읽어 VertexShader에 전달할지 결정
    VkPipelineVertexInputStateCreateInfo vertexInputStateCI{};
    vertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCI.vertexBindingDescriptionCount = 0;   // 버텍스 버퍼를 읽는 규칙
    vertexInputStateCI.vertexAttributeDescriptionCount = 0; // 버텍스 셰이더의 입력 속성

    // 버텍스를 어떤 규칙으로 묶어 점/선/삼각형을 만들지 결정. 이렇게 만드는 도형을 Primitiv라고 부름
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
    inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // 특수 인덱스를 만나면 연결을 끊고 새로 시작
    inputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

    // 도형을 화면 어디에, 어떤 크기로 배치할지, 어느 영역까지 그릴지 결정
    // - Viewport: NDC를 framebuffer 좌표로 변환(위치, 크기, 깊이 지정)
    // - Scissor: 그리고자 하는 사각형 영역 지정. 영역 밖은 버림
    VkPipelineViewportStateCreateInfo viewportCI{};
    viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportCI.viewportCount = 1;
    viewportCI.scissorCount = 1;

    // 래스터화 설정
    // framebuffer 좌표계 기준으로 frontface 판정
    VkPipelineRasterizationStateCreateInfo rasterCI{};
    rasterCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterCI.cullMode = VK_CULL_MODE_NONE;
    rasterCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterCI.lineWidth = 1.0f;
    rasterCI.depthClampEnable = VK_FALSE;
    rasterCI.rasterizerDiscardEnable = VK_FALSE;
    rasterCI.depthBiasEnable = VK_FALSE;

    // 멀티샘플링, sample shading 설정
    VkPipelineMultisampleStateCreateInfo multisampleCI{};
    multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 픽셀당 샘플 1개 사용
    multisampleCI.sampleShadingEnable = VK_FALSE;               // 샘플별 fragment shader 계산 수행 여부

    // 뎁스 스텐실 설정
    VkPipelineDepthStencilStateCreateInfo depthStencilCI{};
    depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilCI.depthWriteEnable = VK_FALSE;
    depthStencilCI.stencilTestEnable = VK_FALSE;

    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicCI{};
    dynamicCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicCI.pDynamicStates = dynamicStates;
    dynamicCI.dynamicStateCount = 2;

    VkPipelineColorBlendAttachmentState colorBlendAttach{};
    colorBlendAttach.blendEnable = VK_FALSE;
    colorBlendAttach.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendCI{};
    colorBlendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendCI.logicOpEnable = VK_FALSE; // 새로운 색, 기존 색에 논리 연산 사용하지 않음
    colorBlendCI.attachmentCount = 1;
    colorBlendCI.pAttachments = &colorBlendAttach;

    // 출력 대상으로 사용할 attachment들의 format 계약
    VkPipelineRenderingCreateInfo renderingCI{};
    renderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCI.colorAttachmentCount = 1;
    renderingCI.pColorAttachmentFormats = &colorFormat;
    renderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    renderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.layout = pipelineLayout;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStageCI;
    pipelineCI.pVertexInputState = &vertexInputStateCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pViewportState = &viewportCI;
    pipelineCI.pRasterizationState = &rasterCI;
    pipelineCI.pMultisampleState = &multisampleCI;
    pipelineCI.pDepthStencilState = &depthStencilCI;
    pipelineCI.pColorBlendState = &colorBlendCI;
    pipelineCI.pDynamicState = &dynamicCI;
    pipelineCI.pTessellationState = nullptr;

    // 어떤 렌더링 환경에서 사용할지
    // 현재 Dynamic Rendering을 사용하기 때문에 RenderPass를 연결하지 않음
    pipelineCI.renderPass = VK_NULL_HANDLE;
    pipelineCI.subpass = 0; // RenderPass 안에서 몇 번째 subpass에 사용할 Pipeline인지 결정하는 인덱스

    // 이미 생성된 Pipeline을 기반으로 파생 Pipeline을 만드는 설정
    pipelineCI.basePipelineIndex = -1; // 생성할 Pipeline들 중, 기반으로 삼을 Pipeline의 인덱스
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE; // 기반 Pipeline Handle
    pipelineCI.pNext = &renderingCI;

    VkPipeline pipeline = VK_NULL_HANDLE;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    pipelineHandle_.Adopt(pipeline, deleter::VkPipelineDeleter{device});
}
