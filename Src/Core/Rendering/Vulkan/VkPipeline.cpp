#include "VkPipeline.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Utils/DescriptorSetLayoutConverters.h"
#include "Utils/VkEnumHelpers.h"
#include "VkGlobals.h"
#include "VkShader.h"
#include "VkTextureManager.h"

void PipelineVulkanBase::PrepareGraphicsBase(const ShaderCollection& shaders,
                                             const PipeVertInfo& vertexInputs,
                                             const PipelineInfo& pipeInfo,
                                             stltype::vector<VkPipelineShaderStageCreateInfo>& outShaderStages,
                                             VkPipelineDynamicStateCreateInfo& outDynamicState,
                                             VkPipelineVertexInputStateCreateInfo& outVertexInput,
                                             VkPipelineInputAssemblyStateCreateInfo& outInputAssembly,
                                             VkPipelineViewportStateCreateInfo& outViewport,
                                             VkPipelineRasterizationStateCreateInfo& outRasterizer,
                                             VkPipelineMultisampleStateCreateInfo& outMultisampling,
                                             VkPipelineColorBlendStateCreateInfo& outColorBlending,
                                             VkPipelineRenderingCreateInfo& outPipelineRenderingCreateInfo)
{
    // Assume we always have a vert and fragment shader here for now
    Shader& vertShader = *shaders.pVertShader;
    if (vertShader.GetDesc() == VK_NULL_HANDLE)
        g_pFileReader->FinishAllRequests();
    // At least one shader must be valid
    DEBUG_ASSERT(vertShader.GetDesc() != VK_NULL_HANDLE);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShader.GetDesc();
    vertShaderStageInfo.pName = vertShader.GetName().c_str();

    outShaderStages = stltype::vector{vertShaderStageInfo};

    // Don't always need fragment shader
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    if (shaders.pFragShader != nullptr)
    {
        Shader& fragShader = *shaders.pFragShader;
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShader.GetDesc();
        fragShaderStageInfo.pName = fragShader.GetName().c_str();

        outShaderStages.push_back(fragShaderStageInfo);
    }

    const auto attachmentCount = pipeInfo.attachmentInfos.colorAttachments.size();

    outDynamicState = CreateDynamicPipelineInfo(g_dynamicStates);
    outVertexInput = CreateVertexInputInfo(vertexInputs);
    outInputAssembly = CreateInputAssemblyInfo(pipeInfo.topology);
    outViewport = CreateViewportInfo(pipeInfo.viewPortExtents);
    outRasterizer = CreateRasterizerInfo(pipeInfo.viewPortExtents, pipeInfo.rasterizerInfo);
    outMultisampling = CreateMultisampleInfo(pipeInfo.multisampleInfo);
    const auto colorAttachmentInfo = CreateColorBlendAttachmentInfo(pipeInfo.colorBlendInfo);
    stltype::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    blendAttachments.assign(attachmentCount, colorAttachmentInfo);
    outColorBlending = CreateColorBlendInfo(pipeInfo.colorInfo, blendAttachments);
    m_pipelineLayout = CreatePipelineLayout(pipeInfo.descriptorSetLayout, pipeInfo.pushConstantInfo);

    outPipelineRenderingCreateInfo = {};
    outPipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    
    // Convert agnostic formats to Vulkan formats locally
    m_colorAttachmentFormats.clear();
    m_colorAttachmentFormats.reserve(pipeInfo.attachmentInfos.colorAttachments.size());
    for (auto fmt : pipeInfo.attachmentInfos.colorAttachments)
        m_colorAttachmentFormats.push_back(Conv(fmt));

    outPipelineRenderingCreateInfo.pColorAttachmentFormats = m_colorAttachmentFormats.empty() ? nullptr : m_colorAttachmentFormats.data();
    outPipelineRenderingCreateInfo.colorAttachmentCount = m_colorAttachmentFormats.size();
    outPipelineRenderingCreateInfo.depthAttachmentFormat = Conv(pipeInfo.attachmentInfos.depthAttachmentFormat);
    outPipelineRenderingCreateInfo.viewMask = pipeInfo.viewMask;
}

ComputePipelineVulkan::ComputePipelineVulkan(const ShaderCollection& shaders, const PipelineInfo& pipeInfo)
{
    // Use pVertShader as compute shader slot
    Shader& compShader = *shaders.pComputeShader;
    if (compShader.GetDesc() == VK_NULL_HANDLE)
        g_pFileReader->FinishAllRequests();
    DEBUG_ASSERT(compShader.GetDesc() != VK_NULL_HANDLE);

    VkPipelineShaderStageCreateInfo compStage{};
    compStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compStage.module = compShader.GetDesc();
    compStage.pName = compShader.GetName().c_str();

    m_pipelineLayout = CreatePipelineLayout(pipeInfo.descriptorSetLayout, pipeInfo.pushConstantInfo);

    VkComputePipelineCreateInfo computeInfo{};
    computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computeInfo.stage = compStage;
    computeInfo.layout = m_pipelineLayout;

    DEBUG_ASSERT(vkCreateComputePipelines(
                     VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &computeInfo, VulkanAllocator(), &m_pipeline) ==
                 VK_SUCCESS);
}

ComputePipelineVulkan::~ComputePipelineVulkan()
{
    TRACKED_DESC_IMPL
}

void ComputePipelineVulkan::CleanUp()
{
    VK_FREE_IF(m_pipelineLayout, vkDestroyPipelineLayout(VK_LOGICAL_DEVICE, m_pipelineLayout, VulkanAllocator()));
    if (m_pipeline != VK_NULL_HANDLE)
    {
        g_pDeleteQueue->RegisterDeleteForNextFrame([pip = m_pipeline]() mutable
                                                   { vkDestroyPipeline(VK_LOGICAL_DEVICE, pip, VulkanAllocator()); });
    }
}

GraphicsPipelineVulkan::GraphicsPipelineVulkan(const ShaderCollection& shaders,
                                               const PipeVertInfo& vertexInputs,
                                               const PipelineInfo& pipeInfo)
{
    // Make sure we're not trying to create a graphics pipeline with a compute
    // shader
    DEBUG_ASSERT(shaders.pComputeShader == nullptr);

    stltype::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    VkPipelineDynamicStateCreateInfo dymState{};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineViewportStateCreateInfo viewport{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};

    // Prepare common graphics pieces
    PrepareGraphicsBase(shaders,
                        vertexInputs,
                        pipeInfo,
                        shaderStages,
                        dymState,
                        vertexInput,
                        inputAssembly,
                        viewport,
                        rasterizer,
                        multisampling,
                        colorBlending,
                        pipelineRenderingCreateInfo);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dymState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.subpass = 0;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    if (pipeInfo.hasDepth)
    {
        const auto depthStencil = CreateDepthStencilLayout();
        pipelineInfo.pDepthStencilState = &depthStencil;
        DEBUG_ASSERT(
            vkCreateGraphicsPipelines(
                VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VulkanAllocator(), &m_pipeline) ==
            VK_SUCCESS);
    }
    else
    {
        DEBUG_ASSERT(
            vkCreateGraphicsPipelines(
                VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VulkanAllocator(), &m_pipeline) ==
            VK_SUCCESS);
    }
}

GraphicsPipelineVulkan::~GraphicsPipelineVulkan()
{
    TRACKED_DESC_IMPL
}

void GraphicsPipelineVulkan::CleanUp()
{
    VK_FREE_IF(m_pipelineLayout, vkDestroyPipelineLayout(VK_LOGICAL_DEVICE, m_pipelineLayout, VulkanAllocator()));
    if (m_pipeline != VK_NULL_HANDLE)
    {
        g_pDeleteQueue->RegisterDeleteForNextFrame([pip = m_pipeline]() mutable
                                                   { vkDestroyPipeline(VK_LOGICAL_DEVICE, pip, VulkanAllocator()); });
    }
}

bool GraphicsPipelineVulkan::HasDynamicViewScissorState() const
{
    return m_info.dynamicViewScissor;
}

bool GraphicsPipelineVulkan::NeedsVertexBuffers() const
{
    return m_vertexInfo.bindingDescriptionCount > 0;
}

VkPipeline GraphicsPipelineVulkan::GetRef() const
{
    return m_pipeline;
}

VkPipelineDynamicStateCreateInfo PipelineVulkanBase::CreateDynamicPipelineInfo(
    const stltype::vector<VkDynamicState>& dynamicStates)
{
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    return dynamicState;
}

VkPipelineVertexInputStateCreateInfo PipelineVulkanBase::CreateVertexInputInfo(const PipeVertInfo& vertexInputs)
{
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = vertexInputs.bindingDescriptionCount;
    vertexInputInfo.pVertexBindingDescriptions = &vertexInputs.m_vertexInputDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = vertexInputs.m_attributeDescriptions.size();
    vertexInputInfo.pVertexAttributeDescriptions = vertexInputs.m_attributeDescriptions.data();

    m_vertexInfo = vertexInputs;

    return vertexInputInfo;
}

VkPipelineVertexInputStateCreateInfo PipelineVulkanBase::CreateEmptyVertexInputInfo()
{
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo PipelineVulkanBase::CreateInputAssemblyInfo(const Topology& topology)
{
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = Conv(topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    return inputAssembly;
}

VkPipelineViewportStateCreateInfo PipelineVulkanBase::CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents)
{
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    return viewportState;
}

VkPipelineRasterizationStateCreateInfo PipelineVulkanBase::CreateRasterizerInfo(
    const DirectX::XMFLOAT4& viewportExtents, const RasterizerInfo& info)
{
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = info.enableDepthClamp;
    rasterizer.rasterizerDiscardEnable = info.discardEverythingBeforeRasterizer;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = info.lineWidth;
    rasterizer.cullMode = Conv(info.cullmode);
    rasterizer.frontFace = Conv(info.frontMode);
    rasterizer.depthBiasEnable = info.depthBias;
    rasterizer.depthBiasConstantFactor = info.depthBiasConstantFactor;
    rasterizer.depthBiasClamp = info.depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = info.depthBiasSlopeFactor;

    return rasterizer;
}

VkPipelineMultisampleStateCreateInfo PipelineVulkanBase::CreateMultisampleInfo(const MultisampleInfo& info)
{
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;          // Optional
    multisampling.pSampleMask = nullptr;            // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    return multisampling;
}

VkPipelineColorBlendAttachmentState PipelineVulkanBase::CreateColorBlendAttachmentInfo(
    const ColorBlendAttachmentInfo& info)
{
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    return colorBlendAttachment;
}

VkPipelineColorBlendStateCreateInfo PipelineVulkanBase::CreateColorBlendInfo(
    const ColorBlendInfo& info, const stltype::vector<VkPipelineColorBlendAttachmentState>& colorBlendAttachments)
{
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = colorBlendAttachments.size();
    colorBlending.pAttachments = colorBlendAttachments.data();
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    return colorBlending;
}

VkPipelineDepthStencilStateCreateInfo GraphicsPipelineVulkan::CreateDepthStencilLayout()
{
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {};  // Optional
    return depthStencil;
}

VkPipelineLayout PipelineVulkanBase::CreatePipelineLayout(const DescriptorSetLayoutInfo& layoutInfo,
                                                          const PushConstantInfo& pushConstantInfo)
{
    m_sharedDescriptorSetLayouts =
        DescriptorLaytoutUtils::CreateOneDescriptorSetLayoutPerSet(layoutInfo.sharedDescriptors);

    m_descriptorSetLayout =
        DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(layoutInfo.pipelineSpecificDescriptors);

    stltype::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    descriptorSetLayouts.reserve(m_sharedDescriptorSetLayouts.size());
    for (const auto& layout : m_sharedDescriptorSetLayouts)
    {
        descriptorSetLayouts.emplace_back(layout.GetRef());
    }

    // Only add pipeline-specific layout if there are bindings defined
    if (layoutInfo.pipelineSpecificDescriptors.empty() == false)
    {
        descriptorSetLayouts.emplace_back(m_descriptorSetLayout.GetRef());
    }

    VkPushConstantRange pushConstants{};
    for (const auto& constant : pushConstantInfo.constants)
    {
        pushConstants.stageFlags = Conv(constant.shaderUsage);
        pushConstants.offset = constant.offset;
        pushConstants.size = constant.size;
    }

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (pushConstantInfo.constants.size() > 0)
    {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstants;
    }

    DEBUG_ASSERT(vkCreatePipelineLayout(
                     VkGlobals::GetLogicalDevice(), &pipelineLayoutInfo, VulkanAllocator(), &pipelineLayout) ==
                 VK_SUCCESS);
    return pipelineLayout;
}
