#include "app.h"
#include "utils.h"
#include "assimp_scene.h"

NP_TRACER_NAMESPACE_BEGIN

constexpr uint32_t DEFAULT_FRAMES_IN_FLIGHT = 2u;

// for ease-of-use
#define USE_SWAPCHAIN mRendererConstants.executionMode == eExecutionMode::SWAPCHAIN

void App::create(const RendererConstants& rendererConstants)
{
    mRendererConstants = rendererConstants;

    // switch on scene type
    switch (mRendererConstants.sceneType)
    {
        case eSceneType::ASSIMP: mpScene = std::make_unique<AssimpScene>(); break;
        default: mpScene = std::make_unique<Scene>();
    }

    // create vulkan basics
    mContext.setFramesInFlight(DEFAULT_FRAMES_IN_FLIGHT);
    if (USE_SWAPCHAIN) mContext.createWindow(mpWindow, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    mContext.createInstance(gDEBUG);
    if (USE_SWAPCHAIN) mContext.createSurface(mpWindow);
    mContext.createPhysicalDevice();
    mContext.createLogicalDeviceAndQueues();
    mContext.createAllocator();
    if (USE_SWAPCHAIN) mContext.createSwapchain(mpWindow);

    // TODO: see if semaphores are needed in execution mode
    size_t numRenderingSemaphores = USE_SWAPCHAIN ? mContext.swapchainImages.size() : 0;
    mContext.createSyncAndFrameObjects(numRenderingSemaphores);

    // mContext.createDepthImage(kWIDTH, kHEIGHT);  // TODO: pass actual depth aov target
    mContext.createTextureSampler(mSampler);
    createSpecializationConstants();

    mContext.waitIdle();
}

// RESOURCE CREATION
void App::createRenderingResources(std::optional<WRAP_REF<RendererAovs>> aovsRef)
{
    mpScene->finalize();  // finalize state of scene before resource creation
    if (gDEBUG) mpScene->reportState();

    {  // this block is so very very TEMP oh god
        // recreate result images
        // in the future, this should be down before the call by the `App` 'owner'
        if (USE_SWAPCHAIN)
        {
            mContext.createResultImages(mContext.swapchainParams.extent.width,
                                        mContext.swapchainParams.extent.height);
        }
        else
        {
            RendererAovs& aovs = aovsRef.value();
            mContext.createResultImages(aovs.color->width, aovs.color->height);
        }
    }

    // MESHES
    const size_t meshCount = mpScene->getPrimCount<Mesh>();
    std::vector<MeshRecord> meshRecords;
    meshRecords.reserve(meshCount);

    std::vector<Vertex> globalVertices;
    std::vector<uint32_t> globalIndices;
    std::vector<FLOAT4x4> globalTransforms;
    for (size_t i = 0; i < meshCount; i++)
    {
        Mesh const* mesh = mpScene->getPrimAtIndex<Mesh>(i);

        MeshRecord meshRecord{ .vertexOffset = static_cast<uint32_t>(globalVertices.size()),
                               .indexOffset = static_cast<uint32_t>(globalIndices.size()),
                               .indexCount = static_cast<uint32_t>(mesh->indices.size()),
                               .vertexCount = static_cast<uint32_t>(mesh->vertices.size()),
                               .transformIndex = static_cast<uint32_t>(globalTransforms.size()),
                               .materialIndex = mesh->materialIndex };

        globalVertices.insert(globalVertices.end(), mesh->vertices.begin(), mesh->vertices.end());
        globalIndices.reserve(globalIndices.size() + mesh->indices.size());
        for (uint32_t idx : mesh->indices)
        {
            globalIndices.push_back(idx + meshRecord.vertexOffset);
        }

        // temp
        mIndexCounts.push_back(static_cast<uint32_t>(mesh->indices.size()));

        // transforms
        globalTransforms.push_back(mesh->transform);

        meshRecords.push_back(meshRecord);
    }

    VkDeviceSize meshRecordSize = sizeof(meshRecords[0]) * meshRecords.size();
    bool meshRecordBufferCreated
        = mContext.createDeviceLocalBuffer(mMeshRecordBuffer, meshRecords.data(), meshRecordSize,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    DEV_ASSERT(meshRecordBufferCreated, "mesh record buffer could not be created.\n");

    VkDeviceSize vertexBufferSize = sizeof(globalVertices[0]) * globalVertices.size();
    VkDeviceSize indexBufferSize = sizeof(globalIndices[0]) * globalIndices.size();
    VkDeviceSize transformBufferSize = sizeof(globalTransforms[0]) * globalTransforms.size();

    mContext.createDeviceLocalBuffer(
        mVertexBuffer, globalVertices.data(), vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    mContext.createDeviceLocalBuffer(
        mIndexBuffer, globalIndices.data(), indexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    mContext.createDeviceLocalBuffer(mMeshTransformsBuffer, globalTransforms.data(),
                                     transformBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // LIGHTS
    const size_t lightCount = mpScene->getPrimCount<Light>();
    mNumLights = static_cast<uint32_t>(lightCount);  // push constant
    std::vector<LightRecord> lightRecords;
    lightRecords.reserve(lightCount);

    if (lightCount > 0)
    {
        for (uint32_t i = 0; i < lightCount; i++)
        {
            Light const* light = mpScene->getPrimAtIndex<Light>(i);
            LightRecord lightRecord = light->toRecord();
            lightRecords.push_back(lightRecord);
        }
    }
    else DEV_ASSERT(false, "No lights were found in scene.");
    VkDeviceSize lightRecordBufferSize = sizeof(lightRecords[0]) * lightRecords.size();

    mContext.createDeviceLocalBuffer(mLightRecordBuffer, lightRecords.data(), lightRecordBufferSize,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // CAMERA
    VkDeviceSize cameraSize = sizeof(CameraRecord);
    bool cameraRecordBufferCreated
        = mContext.createDeviceLocalBuffer(mCameraRecordBuffer, mpScene->getCamera(), cameraSize,
                                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    DEV_ASSERT(cameraRecordBufferCreated, "camera record buffer could not be created.\n");

    // MATERIALS

    const size_t materialCount = mpScene->getPrimCount<Material>();
    std::vector<MaterialRecord> materialRecords;
    materialRecords.reserve(materialCount);

    if (materialCount > 0)
    {
        for (uint32_t i = 0; i < materialCount; i++)
        {
            // right now NPMaterial and record are identical so just use the same struct here (still
            // looping for easy modification in the future)
            Material const* material = mpScene->getPrimAtIndex<Material>(i);
            materialRecords.push_back(material->toRecord());
        }
    }
    else
    {
        // push back default material record   
        MaterialRecord materialRecord{};
        materialRecord.diffuse = FLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
        materialRecords.push_back(materialRecord);
    }


    VkDeviceSize materialRecordBufferSize = sizeof(materialRecords[0]) * materialRecords.size();
    mContext.createDeviceLocalBuffer(mMaterialRecordsBuffer, materialRecords.data(),
                                     materialRecordBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // TEXTURES
    size_t textureCount = mpScene->getPrimCount<Texture>();

    mTextures.reserve(textureCount);
    for (size_t i = 0; i < textureCount; i++)
    {
        Image textureImage;
        Texture const* texture = mpScene->getPrimAtIndex<Texture>(i);

        mContext.createTextureImage(textureImage, texture->pixels, texture->width, texture->height);

        mTextures.push_back(textureImage);
    }

    // RT
    VkDeviceAddress vertexAddress = mContext.getBufferDeviceAddress(mVertexBuffer);
    VkDeviceAddress indexAddress = mContext.getBufferDeviceAddress(mIndexBuffer);

    createAccelerationStructures(meshRecords, globalTransforms, vertexAddress, indexAddress);

    // SET 0: Mesh Records
    {
        DescriptorSetLayout descriptorSetLayout{};

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

        // mesh record buffer
        VkDescriptorSetLayoutBinding b0{ .binding = 0,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS
                                                       | VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

        // vertex ssbo
        VkDescriptorSetLayoutBinding b1{ .binding = 1,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

        // index ssbo
        VkDescriptorSetLayoutBinding b2{ .binding = 2,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

        // transform
        VkDescriptorSetLayoutBinding b3{ .binding = 3,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

        bindings[0] = b0;
        bindings[1] = b1;
        bindings[2] = b2;
        bindings[3] = b3;

        mContext.createDescriptorSetLayout(descriptorSetLayout, bindings);
        mDescriptorSetLayouts.push_back(descriptorSetLayout);

        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        mContext.allocateDesciptorSet(descriptorSet, descriptorSetLayout);

        std::unordered_map<uint32_t, Buffer*> bindingBufferMap;
        bindingBufferMap[0] = &mMeshRecordBuffer;
        bindingBufferMap[1] = &mVertexBuffer;
        bindingBufferMap[2] = &mIndexBuffer;
        bindingBufferMap[3] = &mMeshTransformsBuffer;

        mContext.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);

        mDescriptorSets.push_back(descriptorSet);
    }

    // SET 1 : LIGHTS
    {
        DescriptorSetLayout descriptorSetLayout{};

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

        VkDescriptorSetLayoutBinding b0{ .binding = 0,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                       | VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                         .pImmutableSamplers = nullptr };

        bindings[0] = b0;

        mContext.createDescriptorSetLayout(descriptorSetLayout, bindings);
        mDescriptorSetLayouts.push_back(descriptorSetLayout);

        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        mContext.allocateDesciptorSet(descriptorSet, descriptorSetLayout);

        std::unordered_map<uint32_t, Buffer*> bindingBufferMap;
        bindingBufferMap[0] = &mLightRecordBuffer;

        mContext.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);

        mDescriptorSets.push_back(descriptorSet);
    }

    // SET 2: CAMERA
    {
        DescriptorSetLayout descriptorSetLayout{};

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding b0{ .binding = 0,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                                                       | VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                         .pImmutableSamplers = nullptr };

        bindings[0] = b0;

        mContext.createDescriptorSetLayout(descriptorSetLayout, bindings);
        mDescriptorSetLayouts.push_back(descriptorSetLayout);

        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        mContext.allocateDesciptorSet(descriptorSet, descriptorSetLayout);

        // create descriptor set
        std::unordered_map<uint32_t, Buffer*> bindingBufferMap;
        bindingBufferMap[0] = &mCameraRecordBuffer;

        mContext.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);

        mDescriptorSets.push_back(descriptorSet);
    }

    // SET 3: MATERIALS AND TEXTURES
    {
        DescriptorSetLayout descriptorSetLayout{};

        // materials buffer
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding b0{ .binding = 0,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                         .pImmutableSamplers = nullptr };

        VkDescriptorSetLayoutBinding b1{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(
                mTextures.size()),  // TODO: this one SHOULD be a variable size descriptor
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .pImmutableSamplers = nullptr
        };

        bindings[0] = b0;
        bindings[1] = b1;

        mContext.createDescriptorSetLayout(descriptorSetLayout, bindings);
        mDescriptorSetLayouts.push_back(descriptorSetLayout);

        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        mContext.allocateDesciptorSet(descriptorSet, descriptorSetLayout);

        // create descriptor set
        std::unordered_map<uint32_t, Buffer*> bindingBufferMap;
        bindingBufferMap[0] = &mMaterialRecordsBuffer;

        mContext.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);
        mContext.writeDescriptorSetImages(descriptorSet, 1, mTextures,
                                          mSampler);  // write all textures

        mDescriptorSets.push_back(descriptorSet);
    }

    // SET 4: RT
    {
        DescriptorSetLayout descriptorSetLayout{};

        // acceleration structure
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding b0{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .pImmutableSamplers = nullptr
        };

        // result image
        VkDescriptorSetLayoutBinding b1{ .binding = 1,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                         .pImmutableSamplers = nullptr };

        // accumulation image
        VkDescriptorSetLayoutBinding b2{ .binding = 2,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                         .descriptorCount = 1,
                                         .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
                                                       | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                         .pImmutableSamplers = nullptr };

        bindings[0] = b0;
        bindings[1] = b1;
        bindings[2] = b2;

        mContext.createDescriptorSetLayout(descriptorSetLayout, bindings);
        mDescriptorSetLayouts.push_back(descriptorSetLayout);

        // allocate descriptors
        mContext.allocateDesciptorSet(mContext.rtDescriptorSet, descriptorSetLayout);

        // create descriptor set
        std::unordered_map<uint32_t, AccelerationStructure*> bindingASMap;
        bindingASMap[0] = &mTlas;

        mContext.writeDescriptorSetAccelerationStructures(mContext.rtDescriptorSet, bindingASMap,
                                                          bindings);

        std::vector<Image> resultImages{ mContext.resultImage, mContext.accumulationImage };
        mContext.writeDescriptorSetImages(mContext.rtDescriptorSet, 1, resultImages, mSampler,
                                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                          VK_IMAGE_LAYOUT_GENERAL);

        mDescriptorSets.push_back(mContext.rtDescriptorSet);
    }

    createRTPipeline();
}

void App::createGraphicsPipeline(uint32_t width, uint32_t height, VkFormat format)
{
    // shader creation
    VkShaderModule coreVertModule = mContext.createShaderModule(readFile(NPTRACER_SHADER_CORE_VERT));
    VkShaderModule coreFragModule = mContext.createShaderModule(readFile(NPTRACER_SHADER_CORE_FRAG));

    VkPipelineShaderStageCreateInfo vInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = coreVertModule,
        .pName = "vertMain"
    };

    VkPipelineShaderStageCreateInfo fInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = coreFragModule,
        .pName = "fragMain"
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = { vInfo, fInfo };

    // viewport
    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkPipelineVertexInputStateCreateInfo vertexInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };

    VkPipelineInputAssemblyStateCreateInfo inputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    VkViewport viewport{ 0.0f, 0.0f, 0, 0, 0.0f, 1.0f };
    viewport.width = width;
    viewport.height = height;

    VkExtent2D extent{ .width = width, .height = height };
    VkRect2D rect{ VkOffset2D{ 0, 0 }, extent };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    // color blending
    VkPipelineColorBlendAttachmentState blendInfo{ .blendEnable = VK_FALSE,
                                                   .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                                                     | VK_COLOR_COMPONENT_G_BIT
                                                                     | VK_COLOR_COMPONENT_B_BIT
                                                                     | VK_COLOR_COMPONENT_A_BIT };

    VkPipelineColorBlendStateCreateInfo blendStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blendInfo
    };

    // depth testing
    VkPipelineDepthStencilStateCreateInfo depthInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // pipeline layout
    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    vkDescriptorSetLayouts.reserve(mDescriptorSetLayouts.size());
    for (auto& descriptorSetLayout : mDescriptorSetLayouts)
    {
        vkDescriptorSetLayouts.push_back(descriptorSetLayout.layout);
    }

    VkPushConstantRange pushConstantRange{ .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
                                           .offset = 0,
                                           .size = static_cast<uint32_t>(kPushConstantCount)
                                                   * sizeof(uint32_t) };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(mDescriptorSetLayouts.size()),
        .pSetLayouts = vkDescriptorSetLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    vkCreatePipelineLayout(mContext.device, &pipelineLayoutInfo, nullptr, &mRasterPipeline.layout);

    VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &format,
        .depthAttachmentFormat = mContext.depthFormat
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInfo,
        .pInputAssemblyState = &inputInfo,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterInfo,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthInfo,
        .pColorBlendState = &blendStateInfo,
        .pDynamicState = &dynamicInfo,
        .layout = mRasterPipeline.layout,
        .renderPass = nullptr
    };

    VK_CHECK(vkCreateGraphicsPipelines(mContext.device, nullptr, 1, &pipelineInfo, nullptr,
                                       &mRasterPipeline.pipeline),
             "failed to create graphics pipeline");

    vkDestroyShaderModule(mContext.device, coreVertModule, nullptr);
    vkDestroyShaderModule(mContext.device, coreFragModule, nullptr);
}

void App::createRTPipeline()
{
    // pipeline layout
    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    vkDescriptorSetLayouts.reserve(mDescriptorSetLayouts.size());
    for (auto& descriptorSetLayout : mDescriptorSetLayouts)
    {
        vkDescriptorSetLayouts.push_back(descriptorSetLayout.layout);
    }

    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR
                      | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .offset = 0,
        .size = static_cast<uint32_t>(kPushConstantCount) * sizeof(uint32_t)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(mDescriptorSetLayouts.size()),
        .pSetLayouts = vkDescriptorSetLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkSpecializationMapEntry specializationEntry{
        .constantID = 0,
        .offset = static_cast<uint32_t>(offsetof(SpecializationConstants, kFlipUVY)),
        .size = sizeof(SpecializationConstants::kFlipUVY)
    };
    VkSpecializationInfo specializationInfo{ .mapEntryCount = 1,
                                             .pMapEntries = &specializationEntry,
                                             .dataSize = sizeof(SpecializationConstants),
                                             .pData = &mSpecializationConstants };

    vkCreatePipelineLayout(mContext.device, &pipelineLayoutInfo, nullptr, &mRtPipeline.layout);

    // shaders
    VkShaderModule rgenModule = mContext.createShaderModule(readFile(NPTRACER_SHADER_RT_RGEN));
    VkShaderModule missModule = mContext.createShaderModule(readFile(NPTRACER_SHADER_RT_MISS));
    VkShaderModule hitModule = mContext.createShaderModule(readFile(NPTRACER_SHADER_RT_HIT));
    VkShaderModule shadowHitModule = mContext.createShaderModule(
        readFile(NPTRACER_SHADER_RT_SHADOWHIT));
    VkShaderModule shadowMissModule = mContext.createShaderModule(
        readFile(NPTRACER_SHADER_RT_SHADOWMISS));

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    // STAGE 0: Raygen

    VkPipelineShaderStageCreateInfo rgenInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        .module = rgenModule,
        .pName = "rgenMain",
        .pSpecializationInfo = &specializationInfo,
    };
    shaderStages.push_back(rgenInfo);

    // STAGE 1: Miss
    VkPipelineShaderStageCreateInfo missInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
        .module = missModule,
        .pName = "missMain"
    };
    shaderStages.push_back(missInfo);

    // STAGE 2: Shadow Miss
    VkPipelineShaderStageCreateInfo shadowMissInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
        .module = shadowMissModule,
        .pName = "shadowmissMain"
    };
    shaderStages.push_back(shadowMissInfo);

    // STAGE 3: Hit
    VkPipelineShaderStageCreateInfo hitInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .module = hitModule,
        .pName = "hitMain"
    };
    shaderStages.push_back(hitInfo);

    // STAGE 4: Shadow Hit
    VkPipelineShaderStageCreateInfo shadowHitInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        .module = shadowHitModule,
        .pName = "shadowhitMain"
    };
    shaderStages.push_back(shadowHitInfo);

    // groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    // GROUP 0: Raygen
    VkRayTracingShaderGroupCreateInfoKHR rgenGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    groups.push_back(rgenGroup);

    // GROUP 1: Miss
    VkRayTracingShaderGroupCreateInfoKHR missGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 1,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    groups.push_back(missGroup);

    // GROUP 2: Shadow Miss
    VkRayTracingShaderGroupCreateInfoKHR shadowMissGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 2,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    groups.push_back(shadowMissGroup);

    // GROUP 3: Hit
    VkRayTracingShaderGroupCreateInfoKHR hitGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 3,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    groups.push_back(hitGroup);

    // GROUP 4: Shadow Hit
    VkRayTracingShaderGroupCreateInfoKHR shadowHitGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 4,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    };
    groups.push_back(shadowHitGroup);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 3,  // TODO: change accordingly
        .layout = mRtPipeline.layout
    };

    VK_CHECK(mContext.vkCreateRayTracingPipelinesKHR(mContext.device, VK_NULL_HANDLE,
                                                     VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                                     &mRtPipeline.pipeline),
             "Failed to create ray tracing pipeline!");

    // BINDING TABLE
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2KHR properties2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, .pNext = &properties
    };

    vkGetPhysicalDeviceProperties2(mContext.physicalDevice, &properties2);

    mSbt.handleSize = properties.shaderGroupHandleSize;
    mSbt.handleAlign = properties.shaderGroupHandleAlignment;
    mSbt.baseAlign = properties.shaderGroupBaseAlignment;

    uint32_t groupCount = static_cast<uint32_t>(groups.size());
    size_t groupHandleSize = groupCount * mSbt.handleSize;
    std::vector<uint8_t> shaderHandleStorage(groupHandleSize);

    mContext.vkGetRayTracingShaderGroupHandlesKHR(mContext.device, mRtPipeline.pipeline, 0,
                                                  groupCount, groupHandleSize,
                                                  shaderHandleStorage.data());

    VkDeviceSize rgenStride = alignUp(mSbt.handleSize, mSbt.handleAlign);
    VkDeviceSize missStride = alignUp(mSbt.handleSize, mSbt.handleAlign);
    VkDeviceSize hitStride = alignUp(mSbt.handleSize, mSbt.handleAlign);

    VkDeviceSize rgenSize = rgenStride * 1;  // TODO: writing explicitly here for clarity
    VkDeviceSize missSize = missStride * 2;
    VkDeviceSize hitSize = hitStride * 2;

    VkDeviceSize rgenOffset = 0;
    VkDeviceSize missOffset = alignUpVk(rgenOffset + rgenSize, mSbt.baseAlign);
    VkDeviceSize hitOffset = alignUpVk(missOffset + missSize, mSbt.baseAlign);
    VkDeviceSize sbtSize = hitOffset + hitSize;

    // create sbt buffer
    std::vector<uint8_t> sbtBlob(sbtSize, 0);

    const uint8_t* rgenHandle = shaderHandleStorage.data() + mSbt.handleSize * 0;
    const uint8_t* primaryMissHandle = shaderHandleStorage.data() + mSbt.handleSize * 1;
    const uint8_t* shadowMissHandle = shaderHandleStorage.data() + mSbt.handleSize * 2;
    const uint8_t* primaryHitHandle = shaderHandleStorage.data() + mSbt.handleSize * 3;
    const uint8_t* shadowHitHandle = shaderHandleStorage.data() + mSbt.handleSize * 4;

    std::memcpy(sbtBlob.data() + rgenOffset, rgenHandle, mSbt.handleSize);
    std::memcpy(sbtBlob.data() + missOffset + missStride * 0, primaryMissHandle, mSbt.handleSize);
    std::memcpy(sbtBlob.data() + missOffset + missStride * 1, shadowMissHandle, mSbt.handleSize);
    std::memcpy(sbtBlob.data() + hitOffset + hitStride * 0, primaryHitHandle, mSbt.handleSize);
    std::memcpy(sbtBlob.data() + hitOffset + hitStride * 1, shadowHitHandle, mSbt.handleSize);

    DEV_ASSERT(mContext.createDeviceLocalBuffer(mSbt.buffer, sbtBlob.data(), sbtSize,
                                                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
                                                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
               "Failed to create device local buffer!");

    mSbt.deviceAddress = mContext.getBufferDeviceAddress(mSbt.buffer);

    mSbt.rgen.deviceAddress = mSbt.deviceAddress + rgenOffset;
    mSbt.rgen.stride = rgenStride;
    mSbt.rgen.size = rgenSize;

    mSbt.miss.deviceAddress = mSbt.deviceAddress + missOffset;
    mSbt.miss.stride = missStride;
    mSbt.miss.size = missSize;

    mSbt.hit.deviceAddress = mSbt.deviceAddress + hitOffset;
    mSbt.hit.stride = hitStride;
    mSbt.hit.size = hitSize;

    mSbt.callable.deviceAddress = 0;
    mSbt.callable.stride = 0;
    mSbt.callable.size = 0;

    vkDestroyShaderModule(mContext.device, rgenModule, nullptr);
    vkDestroyShaderModule(mContext.device, missModule, nullptr);
    vkDestroyShaderModule(mContext.device, hitModule, nullptr);
    vkDestroyShaderModule(mContext.device, shadowHitModule, nullptr);
    vkDestroyShaderModule(mContext.device, shadowMissModule, nullptr);
}

void App::createAccelerationStructures(std::vector<MeshRecord>& meshes,
                                       std::vector<FLOAT4x4>& transforms,
                                       VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress)
{
    VkCommandBuffer commandBuffer{};
    mContext.createCommandBuffer(commandBuffer, QueueType::GRAPHICS);
    mContext.beginCommandBuffer(commandBuffer);

    for (auto& mesh : meshes)
    {
        AccelerationStructure blas{};

        mContext.createBottomLevelAccelerationStructure(commandBuffer, blas, vertexAddress,
                                                        indexAddress, mesh.vertexOffset,
                                                        mesh.vertexCount, mesh.indexOffset,
                                                        mesh.indexCount);
        mBlasses.push_back(blas);
    }

    // barrier
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
                           | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
                            | VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    for (auto& blas : mBlasses)
    {
        VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        deviceAddressInfo.accelerationStructure = blas.accelerationStructure;

        blas.deviceAddress = mContext.vkGetAccelerationStructureDeviceAddressKHR(mContext.device,
                                                                                 &deviceAddressInfo);
    }

    // top level
    Buffer instanceBufferHandle{};
    mContext.createTopLevelAccelerationStructure(commandBuffer, mTlas, instanceBufferHandle,
                                                 transforms, mBlasses);

    // barrier
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    mContext.endCommandBuffer(commandBuffer, QueueType::GRAPHICS);

    instanceBufferHandle.destroy(mContext.allocator);
}

void App::executeDrawCall(RendererAovs& aovs)
{
    DEV_ASSERT(aovs.color, "aovs not created properly");
    Image* colorAov = aovs.color;

    // grab a frame
    Frame& frame = mContext.frames[mCurrentFrameInFlight];

    // wait until this frame has finished executing its commands
    vkWaitForFences(mContext.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    // populateDrawCallRaster(frame.commandBuffer, imageIndex); // TODO: make raster vs rt a render setting
    VkExtent2D extent = { colorAov->width, colorAov->height };
    populateDrawCallRT(frame.commandBuffer, colorAov->image, extent,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkResetFences(
        mContext.device, 1,
        &frame.doneExecutingFence);  // signal that fence is ready to be associated with a new queue submission

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;

    VK_CHECK(vkQueueSubmit(mContext.queues[QueueType::GRAPHICS].queue, 1, &submitInfo,
                           frame.doneExecutingFence),
             "vk queue submit failed");

    // increment frame (within ring)
    mCurrentFrameInFlight = (mCurrentFrameInFlight + 1u) % DEFAULT_FRAMES_IN_FLIGHT;
    mContext.frameIndex.fetch_add(1u);  // increment atomic frame index
}

// SWAPCHAIN DRAW CALL
void App::executeDrawCallSwapchain()
{
    // grab a frame
    Frame& frame = mContext.frames[mCurrentFrameInFlight];

    // wait until this frame has finished executing its commands
    vkWaitForFences(mContext.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    // acquire image when it is done being presented
    uint32_t imageIndex;
    if (vkAcquireNextImageKHR(mContext.device, mContext.swapchain, UINT64_MAX,
                              frame.donePresentingSemaphore, nullptr, &imageIndex)
        == VK_ERROR_OUT_OF_DATE_KHR)
    {
        mContext.recreateSwapchain(mpWindow);
        return;
    }

    // populateDrawCallRaster(frame.commandBuffer, imageIndex); // TODO: make raster vs rt a render setting
    populateDrawCallRT(frame.commandBuffer, mContext.swapchainImages[imageIndex],
                       mContext.swapchainParams.extent, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkResetFences(
        mContext.device, 1,
        &frame.doneExecutingFence);  // signal that fence is ready to be associated with a new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores
        = &frame.donePresentingSemaphore;  // wait until image is no longer being presented
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores
        = &mContext.doneRenderingSemaphores[imageIndex];  // signal when rendering finishes

    VK_CHECK(vkQueueSubmit(mContext.queues[QueueType::GRAPHICS].queue, 1, &submitInfo,
                           frame.doneExecutingFence),
             "vk queue submit failed");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores
        = &mContext.doneRenderingSemaphores[imageIndex];  // present after rendering finishes
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mContext.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(mContext.queues[QueueType::GRAPHICS].queue, &presentInfo);
    if ((result == VK_SUBOPTIMAL_KHR) || (result == VK_ERROR_OUT_OF_DATE_KHR)
        || mContext.framebufferResized)
    {
        mContext.framebufferResized = false;
        mContext.recreateSwapchain(mpWindow);
    }

    // increment frame (within ring)
    mCurrentFrameInFlight = (mCurrentFrameInFlight + 1u) % DEFAULT_FRAMES_IN_FLIGHT;
    mContext.frameIndex.fetch_add(1u);  // increment atomic frame index
}

void App::populateDrawCallRaster(Frame& frame, uint32_t imageIndex)
{
    vkResetCommandBuffer(frame.commandBuffer, 0);
    mContext.beginCommandBuffer(frame.commandBuffer);

    mContext.transitionImageLayout(frame.commandBuffer, mContext.swapchainImages[imageIndex],
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearColor{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkClearValue clearDepth{ { 1.0f, 0 } };

    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = mContext.swapchainImageViews[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = clearColor;

    VkRenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachmentInfo.imageView = mContext.depthImage.view;
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentInfo.clearValue = clearDepth;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent = mContext.swapchainParams.extent;

    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    // record commands
    vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);
    vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      mRasterPipeline.pipeline);

    VkViewport viewport{ 0.0f,
                         0.0f,
                         static_cast<float>(mContext.swapchainParams.extent.width),
                         static_cast<float>(mContext.swapchainParams.extent.height),
                         0.0f,
                         1.0f };

    VkRect2D scissor{ { 0, 0 }, mContext.swapchainParams.extent };

    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            mRasterPipeline.layout, 0,
                            static_cast<uint32_t>(mDescriptorSets.size()), mDescriptorSets.data(),
                            0, nullptr);

    for (size_t i = 0; i < mIndexCounts.size(); i++)
    {
        std::array<uint32_t, kPushConstantCount> pushConstants{ static_cast<uint32_t>(i),
                                                                mNumLights, mContext.frameIndex };
        vkCmdPushConstants(frame.commandBuffer, mRasterPipeline.layout,
                           VK_SHADER_STAGE_ALL_GRAPHICS, 0,
                           sizeof(uint32_t) * static_cast<uint32_t>(kPushConstantCount),
                           pushConstants.data());
        vkCmdDraw(frame.commandBuffer, mIndexCounts[i], 1, 0, static_cast<uint32_t>(i));
    }

    vkCmdEndRendering(frame.commandBuffer);

    mContext.transitionImageLayout(frame.commandBuffer, mContext.swapchainImages[imageIndex],
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    // signal that fence is ready to be associated with a new queue submission
    vkResetFences(mContext.device, 1, &frame.doneExecutingFence);

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    mContext.endCommandBuffer(frame.commandBuffer, QueueType::GRAPHICS, waitDestinationStageMask,
                              frame.doneExecutingFence, frame.donePresentingSemaphore,
                              mContext.doneRenderingSemaphores[imageIndex]);
}

void App::populateDrawCallRT(VkCommandBuffer& commandBuffer, VkImage colorAov, VkExtent2D& extent,
                             VkImageLayout dstImageLayout)
{
    vkResetCommandBuffer(commandBuffer, 0);
    mContext.beginCommandBuffer(commandBuffer);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mRtPipeline.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            mRtPipeline.layout, 0, static_cast<uint32_t>(mDescriptorSets.size()),
                            mDescriptorSets.data(), 0, nullptr);

    std::array<uint32_t, kPushConstantCount> pushConstants{ 0, mNumLights, mContext.frameIndex };
    vkCmdPushConstants(commandBuffer, mRtPipeline.layout,
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR
                           | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                       0, sizeof(uint32_t) * static_cast<uint32_t>(kPushConstantCount),
                       pushConstants.data());

    mContext.vkCmdTraceRaysKHR(commandBuffer, &mSbt.rgen, &mSbt.miss, &mSbt.hit, &mSbt.callable,
                               extent.width, extent.height, 1);

    mContext.transitionImageLayout(commandBuffer, colorAov, VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkImageCopy region{};
    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.extent = { extent.width, extent.height, 1 };

    vkCmdCopyImage(commandBuffer, mContext.resultImage.image, VK_IMAGE_LAYOUT_GENERAL, colorAov,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    mContext.transitionImageLayout(commandBuffer, colorAov, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   dstImageLayout, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);
}

void App::createSpecializationConstants()
{
    // so simple for now
    mSpecializationConstants = { .kFlipUVY = mRendererConstants.bFlipUVY ? 1u : 0u };
}

void App::destroy()
{
    mContext.waitIdle();

    for (auto& descriptorSetLayout : mDescriptorSetLayouts)
    {
        descriptorSetLayout.destroy(mContext.device);
    }

    if (mSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(mContext.device, mSampler, nullptr);
    }

    // SET 0 : MESHES
    if (mMeshRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        mMeshRecordBuffer.destroy(mContext.allocator);
    }

    if (mVertexBuffer.buffer != VK_NULL_HANDLE)
    {
        mVertexBuffer.destroy(mContext.allocator);
    }

    if (mIndexBuffer.buffer != VK_NULL_HANDLE)
    {
        mIndexBuffer.destroy(mContext.allocator);
    }

    if (mMeshTransformsBuffer.buffer != VK_NULL_HANDLE)
    {
        mMeshTransformsBuffer.destroy(mContext.allocator);
    }

    // SET 1: LIGHTS
    if (mLightRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        mLightRecordBuffer.destroy(mContext.allocator);
    }

    // SET 2: CAMERA
    if (mCameraRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        mCameraRecordBuffer.destroy(mContext.allocator);
    }

    // SET 3: MATERIAL AND TEXTURES
    if (mMaterialRecordsBuffer.buffer != VK_NULL_HANDLE)
    {
        mMaterialRecordsBuffer.destroy(mContext.allocator);
    }

    for (auto& texture : mTextures)
    {
        texture.destroy(mContext.device, mContext.allocator);
    }

    // SET 4: RT
    mSbt.destroy(mContext.allocator);

    for (auto& blas : mBlasses)
    {
        if (blas.accelerationStructure != VK_NULL_HANDLE)
        {
            mContext.vkDestroyAccelerationStructureKHR(mContext.device, blas.accelerationStructure,
                                                       nullptr);
        }
        blas.destroyBuffers(mContext.device, mContext.allocator);
    }

    if (mTlas.accelerationStructure != VK_NULL_HANDLE)
    {
        mContext.vkDestroyAccelerationStructureKHR(mContext.device, mTlas.accelerationStructure,
                                                   nullptr);
        mTlas.destroyBuffers(mContext.device, mContext.allocator);
    }

    // PIPELINES
    if (mRasterPipeline.pipeline != VK_NULL_HANDLE)
    {
        mRasterPipeline.destroy(mContext.device);
    }

    if (mRtPipeline.pipeline != VK_NULL_HANDLE)
    {
        mRtPipeline.destroy(mContext.device);
    }

    mContext.destroy();

    if (mpWindow)
    {
        glfwDestroyWindow(mpWindow);
        mpWindow = nullptr;
        glfwTerminate();
    }
}

void App::loadSceneFromPath(const char* path)
{
    mpScene->loadSceneFromPath(path);
}

void App::render()
{
    while (!glfwWindowShouldClose(mpWindow))
    {
        glfwPollEvents();
        executeDrawCallSwapchain();
    }
}

NP_TRACER_NAMESPACE_END
