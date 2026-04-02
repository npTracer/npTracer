#include "app.h"
#include "utils.h"
#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <queue>
#include <stb_image.h>

void App::create()
{
    // define `scene`
    scene = std::make_unique<Scene>();

    // create vulkan basics
    context.setFrameCount(FRAME_COUNT);
    if (kSTANDALONE)
    {
        context.createWindow(window, WIDTH, HEIGHT);
    }

    context.createInstance(kDEBUG);

    if (kSTANDALONE)
    {
        context.createSurface(window);
    }

    context.createPhysicalDevice();
    context.createLogicalDeviceAndQueues();
    context.createAllocator();

    if (kSTANDALONE)
    {
        context.createSwapchain(window);
    }

    context.createSyncAndFrameObjects();
    context.createDepthImage(WIDTH, HEIGHT);  // TODO pass actual depth aov target
    context.createResultImages();
    context.createTextureSampler(sampler);
    context.waitIdle();
}

// RESOURCE CREATION
void App::createRenderingResources()
{
    // GEOMETRY
    const size_t meshCount = scene->getMeshCount();
    std::vector<NPMeshRecord> meshRecords;
    meshRecords.reserve(meshCount);

    std::vector<NPVertex> globalVertices;
    std::vector<uint32_t> globalIndices;
    std::vector<FLOAT4X4> globalTransforms;
    for (uint32_t i = 0; i < static_cast<uint32_t>(meshCount); i++)
    {
        const NPMesh* mesh = scene->getMeshAtIndex(i);

        NPMeshRecord meshRecord{};
        meshRecord.vertexOffset = static_cast<uint32_t>(globalVertices.size());
        meshRecord.indexOffset = static_cast<uint32_t>(globalIndices.size());
        meshRecord.indexCount = static_cast<uint32_t>(mesh->indices.size());
        meshRecord.vertexCount = static_cast<uint32_t>(mesh->vertices.size());
        meshRecord.transformIndex = static_cast<uint32_t>(globalTransforms.size());
        meshRecord.materialIndex = mesh->materialIndex;

        globalVertices.insert(globalVertices.end(), mesh->vertices.begin(), mesh->vertices.end());
        globalIndices.reserve(globalIndices.size() + mesh->indices.size());
        for (uint32_t idx : mesh->indices)
        {
            globalIndices.push_back(idx + meshRecord.vertexOffset);
        }

        // temp
        indexCounts.push_back(static_cast<uint32_t>(mesh->indices.size()));

        // transforms
        globalTransforms.push_back(mesh->objectToWorld);

        meshRecords.push_back(meshRecord);
    }

    VkDeviceSize meshRecordSize = sizeof(meshRecords[0]) * meshRecords.size();
    bool meshRecordBufferCreated
        = context.createDeviceLocalBuffer(meshRecordBuffer, meshRecords.data(), meshRecordSize,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    DEV_ASSERT(meshRecordBufferCreated, "mesh record buffer could not be created.");

    VkDeviceSize vertexBufferSize = sizeof(globalVertices[0]) * globalVertices.size();
    VkDeviceSize indexBufferSize = sizeof(globalIndices[0]) * globalIndices.size();
    VkDeviceSize transformBufferSize = sizeof(globalTransforms[0]) * globalTransforms.size();
    
    context.createDeviceLocalBuffer(vertexBuffer, globalVertices.data(), vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    context.createDeviceLocalBuffer(indexBuffer, globalIndices.data(), indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    context.createDeviceLocalBuffer(geometryTransformsBuffer, globalTransforms.data(), transformBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    
    // LIGHTS
    const size_t lightCount = scene->getLightCount();
    numLights = static_cast<uint32_t>(lightCount);  // push constant
    std::vector<NPLightRecord> lightRecords;
    meshRecords.reserve(meshCount);
    std::vector<FLOAT4X4> lightTransforms;

    if (lightCount > 0)
    {
        for (uint32_t i = 0; i < lightCount; i++)
        {
            const NPLight* light = scene->getLightAtIndex(i);

            NPLightRecord lightRecord;
            lightRecord.lightTransformIndex = static_cast<uint32_t>(lightTransforms.size());
            lightRecord.color = FLOAT4(light->color, 1.0);
            lightRecord.intensity = light->intensity;

            lightTransforms.push_back(light->transform);
            lightRecords.push_back(lightRecord);
        }
    }
    else if (kDEBUG)
    {
        numLights = 1;

        NPLightRecord defaultLightRecord;
        defaultLightRecord.lightTransformIndex = static_cast<uint32_t>(lightTransforms.size());
        defaultLightRecord.color = FLOAT4(1.0, 1.0, 1.0, 1.0);
        defaultLightRecord.intensity = static_cast<uint32_t>(1.0);

        auto transform = FLOAT4X4(1.0);
        transform[3] = FLOAT4(0.0f, 0.0f, 0.0f, 1.0f);  // written explicitly for debugging
        lightTransforms.push_back(transform);
        lightRecords.push_back(defaultLightRecord);
    }
    else DEV_ASSERT(false, "No lights were found in scene.");

    VkDeviceSize lightRecordBufferSize = sizeof(lightRecords[0]) * lightRecords.size();
    VkDeviceSize lightTransformsSize = sizeof(lightTransforms[0]) * lightTransforms.size();

    context.createDeviceLocalBuffer(lightRecordBuffer, lightRecords.data(), lightRecordBufferSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    context.createDeviceLocalBuffer(lightTransformsBuffer, lightTransforms.data(),
                                    lightTransformsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // CAMERA
    VkDeviceSize cameraSize = sizeof(NPCameraRecord);
    bool cameraRecordBufferCreated
        = context.createDeviceLocalBuffer(cameraRecordBuffer, scene->getCamera(), cameraSize,
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    DEV_ASSERT(cameraRecordBufferCreated, "camera record buffer could not be created.");

    // MATERIALS

    const size_t materialCount = scene->getMaterialCount();
    std::vector<NPMaterial> materialRecords;
    materialRecords.reserve(materialCount);

    for (uint32_t i = 0; i < materialCount; i++)
    {
        // right now NPMaterial and record are identical so just use the same struct here (still
        // looping for easy modification in the future)
        const NPMaterial* material = scene->getMaterialAtIndex(i);

        materialRecords.push_back(*material);
    }

    VkDeviceSize materialRecordBufferSize = sizeof(materialRecords[0]) * materialRecords.size();
    context.createDeviceLocalBuffer(materialRecordsBuffer, materialRecords.data(),
                                    materialRecordBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // TEXTURES
    uint32_t textureCount = static_cast<uint32_t>(scene->pendingTextures.size());
    textures.reserve(textureCount);
    for (uint32_t i = 0; i < textureCount; i++)
    {
        NPImage textureImage;
        auto texture = scene->pendingTextures[i].get();

        context.createTextureImage(textureImage, texture->pixels, texture->width, texture->height);

        textures.push_back(textureImage);
    }
    
    // RT
    VkDeviceAddress vertexAddress = context.getBufferDeviceAddress(vertexBuffer);
    VkDeviceAddress indexAddress = context.getBufferDeviceAddress(indexBuffer);
    
    createAccelerationStructures(meshRecords, globalTransforms, vertexAddress, indexAddress);
    
    // SET 0: Mesh Records
    {
        NPDescriptorSetLayout descriptorSetLayout{};

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        
        // mesh record buffer
        VkDescriptorSetLayoutBinding b0{};
        b0.binding = 0;
        b0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b0.descriptorCount = 1;
        b0.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        // vertex ssbo
        VkDescriptorSetLayoutBinding b1{};
        b1.binding = 1;
        b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b1.descriptorCount = 1;
        b1.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        // index ssbo
        VkDescriptorSetLayoutBinding b2{};
        b2.binding = 2;
        b2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b2.descriptorCount = 1;
        b2.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        
        bindings[0] = b0;
        bindings[1] = b1;
        bindings[2] = b2;
        
        context.createDescriptorSetLayout(descriptorSetLayout, bindings);
        descriptorSetLayouts.push_back(descriptorSetLayout);
        
        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        context.allocateDesciptorSet(descriptorSet, descriptorSetLayout);
        
        std::unordered_map<uint32_t, NPBuffer*> bindingBufferMap;
        bindingBufferMap[0] = &meshRecordBuffer;
        bindingBufferMap[1] = &vertexBuffer;
        bindingBufferMap[2] = &indexBuffer;
        
        context.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);
        
        descriptorSets.push_back(descriptorSet);
    }
    
    // SET 1 : TRANSFORMS
    {
        NPDescriptorSetLayout descriptorSetLayout{};

        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        
        // geometry transforms
        VkDescriptorSetLayoutBinding b0{}; 
        b0.binding = 0;
        b0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b0.descriptorCount = 1;
        b0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        // light transforms
        VkDescriptorSetLayoutBinding b1{};
        b1.binding = 1;
        b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b1.descriptorCount = 1;
        b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        bindings[0] = b0;
        bindings[1] = b1;
        
        context.createDescriptorSetLayout(descriptorSetLayout, bindings);
        descriptorSetLayouts.push_back(descriptorSetLayout);
        
        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        context.allocateDesciptorSet(descriptorSet, descriptorSetLayout);
        
        std::unordered_map<uint32_t, NPBuffer*> bindingBufferMap;
        bindingBufferMap[0] = &geometryTransformsBuffer;
        bindingBufferMap[1] = &lightTransformsBuffer;
        
        context.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);
        
        descriptorSets.push_back(descriptorSet);
    }
    
    // SET 2: CAMERA AND LIGHTS
    {
        NPDescriptorSetLayout descriptorSetLayout{};

        // camera buffer
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding b0{};
        b0.binding = 0;
        b0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b0.descriptorCount = 1;
        b0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        b0.pImmutableSamplers = nullptr;
        
        // lights
        VkDescriptorSetLayoutBinding b1{};
        b1.binding = 1;
        b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b1.descriptorCount = 1;
        b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        b1.pImmutableSamplers = nullptr;
        
        bindings[0] = b0;
        bindings[1] = b1;
        
        context.createDescriptorSetLayout(descriptorSetLayout, bindings);
        descriptorSetLayouts.push_back(descriptorSetLayout);
        
        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        context.allocateDesciptorSet(descriptorSet, descriptorSetLayout);
        
        // create descriptor set
        std::unordered_map<uint32_t, NPBuffer*> bindingBufferMap;
        bindingBufferMap[0] = &cameraRecordBuffer;
        bindingBufferMap[1] = &lightRecordBuffer;
        
        context.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);

        descriptorSets.push_back(descriptorSet);
    }
    
    // SET 3: MATERIALS AND TEXTURES
    {
        NPDescriptorSetLayout descriptorSetLayout{};

        // materials buffer
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding b0{};
        b0.binding = 0;
        b0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b0.descriptorCount = 1;
        b0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        b0.pImmutableSamplers = nullptr;
        
        VkDescriptorSetLayoutBinding b1{};
        b1.binding = 1;
        b1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b1.descriptorCount = static_cast<uint32_t>(textures.size());
        b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        b1.pImmutableSamplers = nullptr;
        
        bindings[0] = b0;
        bindings[1] = b1;
        
        context.createDescriptorSetLayout(descriptorSetLayout, bindings);
        descriptorSetLayouts.push_back(descriptorSetLayout);
        
        // allocate descriptors
        VkDescriptorSet descriptorSet{};
        context.allocateDesciptorSet(descriptorSet, descriptorSetLayout);
        
        // create descriptor set
        std::unordered_map<uint32_t, NPBuffer*> bindingBufferMap;
        bindingBufferMap[0] = &materialRecordsBuffer;
        
        context.writeDescriptorSetBuffers(descriptorSet, bindingBufferMap, bindings);
        context.writeDescriptorSetImages(descriptorSet, 1, textures, &sampler); // write all textures

        descriptorSets.push_back(descriptorSet);
    }
    
    // SET 4: RT
    {
        NPDescriptorSetLayout descriptorSetLayout{};
        
        // acceleration structure
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding b0{};
        b0.binding = 0;
        b0.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        b0.descriptorCount = 1;
        b0.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        b0.pImmutableSamplers = nullptr;
        
        // result image
        VkDescriptorSetLayoutBinding b1{};
        b1.binding = 1;
        b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b1.descriptorCount = 1;
        b1.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;;
        b1.pImmutableSamplers = nullptr;
        
        // accumulation image
        VkDescriptorSetLayoutBinding b2{};
        b2.binding = 2;
        b2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b2.descriptorCount = 1;
        b2.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;;
        b2.pImmutableSamplers = nullptr;
        
        bindings[0] = b0;
        bindings[1] = b1;
        bindings[2] = b2;
        
        context.createDescriptorSetLayout(descriptorSetLayout, bindings);
        descriptorSetLayouts.push_back(descriptorSetLayout);
        
        // allocate descriptors
        context.allocateDesciptorSet(context.rtDescriptorSet, descriptorSetLayout);
        
        // create descriptor set
        std::unordered_map<uint32_t, NPAccelerationStructure*> bindingASMap;
        bindingASMap[0] = &tlas;

        context.writeDescriptorSetAccelerationStructures(context.rtDescriptorSet, bindingASMap, bindings);
        
        std::vector<NPImage> resultImages{context.resultImage, context.accumulationImage};
        context.writeDescriptorSetImages(context.rtDescriptorSet, 1, resultImages, &sampler, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
        
        descriptorSets.push_back(context.rtDescriptorSet);
    }
    
    createRTPipeline();
    
    // createGraphicsPipeline(); // TODO: Make raster vs rt a macro
}

void App::createGraphicsPipeline()
{
    // shader creation
    VkShaderModule coreVertModule = context.createShaderModule(readFile(NPTRACER_SHADER_CORE_VERT));
    VkShaderModule coreFragModule = context.createShaderModule(readFile(NPTRACER_SHADER_CORE_FRAG));

    VkPipelineShaderStageCreateInfo vInfo{};
    vInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vInfo.module = coreVertModule;
    vInfo.pName = "vertMain";

    VkPipelineShaderStageCreateInfo fInfo{};
    fInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fInfo.module = coreFragModule;
    fInfo.pName = "fragMain";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vInfo, fInfo };

    // viewport
    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicInfo.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInfo{};
    vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInfo.vertexBindingDescriptionCount = 0;
    vertexInfo.pVertexBindingDescriptions = nullptr;
    vertexInfo.vertexAttributeDescriptionCount = 0;
    vertexInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputInfo{};
    inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{ 0.0f, 0.0f, 0, 0, 0.0f, 1.0f };
    viewport.width = m_aovs ? m_aovs->color->width : context.swapchainParams.extent.width;
    viewport.height = m_aovs ? m_aovs->color->height : context.swapchainParams.extent.height;

    VkExtent2D extent{};
    extent.width = m_aovs ? m_aovs->color->width : context.swapchainParams.extent.width;
    extent.height = m_aovs ? m_aovs->color->height : context.swapchainParams.extent.height;
    VkRect2D rect{ VkOffset2D{ 0, 0 }, extent };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterInfo{};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.depthClampEnable = VK_FALSE;
    rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;
    rasterInfo.depthBiasSlopeFactor = 1.0f;
    rasterInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    // color blending
    VkPipelineColorBlendAttachmentState blendInfo{};
    blendInfo.blendEnable = VK_FALSE;
    blendInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                               | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blendStateInfo{};
    blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendStateInfo.logicOpEnable = VK_FALSE;
    blendStateInfo.logicOp = VK_LOGIC_OP_COPY;
    blendStateInfo.attachmentCount = 1;
    blendStateInfo.pAttachments = &blendInfo;

    // depth testing
    VkPipelineDepthStencilStateCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthInfo.depthTestEnable = VK_TRUE;
    depthInfo.depthWriteEnable = VK_TRUE;
    depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthInfo.depthBoundsTestEnable = VK_FALSE;
    depthInfo.stencilTestEnable = VK_FALSE;

    // pipeline layout
    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    vkDescriptorSetLayouts.reserve(descriptorSetLayouts.size());
    for (auto& descriptorSetLayout : descriptorSetLayouts)
    {
        vkDescriptorSetLayouts.push_back(descriptorSetLayout.layout);
    }

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 3 * sizeof(uint32_t);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &rasterPipeline.layout);
    
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = m_aovs ? &m_aovs->color->format
                                                   : &context.swapchainParams.format.format;
    renderingInfo.depthAttachmentFormat = context.depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInfo;
    pipelineInfo.pInputAssemblyState = &inputInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterInfo;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthInfo;
    pipelineInfo.pColorBlendState = &blendStateInfo;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = rasterPipeline.layout;
    pipelineInfo.renderPass = nullptr;
    
    if (vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipelineInfo, nullptr,
                                  &rasterPipeline.pipeline)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline");
    }
    
    vkDestroyShaderModule(context.device, coreVertModule, nullptr);
    vkDestroyShaderModule(context.device, coreFragModule, nullptr);
}

void App::createRTPipeline()
{
    // pipeline layout
    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    vkDescriptorSetLayouts.reserve(descriptorSetLayouts.size());
    for (auto& descriptorSetLayout : descriptorSetLayouts)
    {
        vkDescriptorSetLayouts.push_back(descriptorSetLayout.layout);
    }
    
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = 
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 3 * sizeof(uint32_t);
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &rtPipeline.layout);
    
    // shaders
    VkShaderModule rgenModule = context.createShaderModule(readFile(NPTRACER_SHADER_RT_RGEN));
    VkShaderModule missModule = context.createShaderModule(readFile(NPTRACER_SHADER_RT_MISS));
    VkShaderModule hitModule = context.createShaderModule(readFile(NPTRACER_SHADER_RT_HIT));
    VkShaderModule shadowHitModule = context.createShaderModule(readFile(NPTRACER_SHADER_RT_SHADOWHIT));
    VkShaderModule shadowMissModule = context.createShaderModule(readFile(NPTRACER_SHADER_RT_SHADOWMISS));
    
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    
    // STAGE 0: Raygen
    VkPipelineShaderStageCreateInfo rgenInfo{};
    rgenInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rgenInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rgenInfo.module = rgenModule;
    rgenInfo.pName = "rgenMain";
    shaderStages.push_back(rgenInfo);
    
    // STAGE 1: Miss
    VkPipelineShaderStageCreateInfo missInfo{};
    missInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missInfo.module = missModule;
    missInfo.pName = "missMain";
    shaderStages.push_back(missInfo);
    
    // STAGE 2: Shadow Miss
    VkPipelineShaderStageCreateInfo shadowMissInfo{};
    shadowMissInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shadowMissInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shadowMissInfo.module = shadowMissModule;
    shadowMissInfo.pName = "shadowmissMain";
    shaderStages.push_back(shadowMissInfo);
    
    // STAGE 3: Hit
    VkPipelineShaderStageCreateInfo hitInfo{};
    hitInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    hitInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    hitInfo.module = hitModule;
    hitInfo.pName = "hitMain";
    shaderStages.push_back(hitInfo);
    
    // STAGE 4: Shadow Hit
    VkPipelineShaderStageCreateInfo shadowHitInfo{};
    shadowHitInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shadowHitInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    shadowHitInfo.module = shadowHitModule;
    shadowHitInfo.pName = "shadowhitMain";
    shaderStages.push_back(shadowHitInfo);
    
    // groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    
    // GROUP 0: Raygen
    VkRayTracingShaderGroupCreateInfoKHR rgenGroup{};
    rgenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    rgenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rgenGroup.generalShader = 0;
    rgenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    rgenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    rgenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(rgenGroup);
    
    // GROUP 1: Miss
    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 1;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(missGroup);
    
    // GROUP 2: Shadow Miss
    VkRayTracingShaderGroupCreateInfoKHR shadowMissGroup{};
    shadowMissGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shadowMissGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shadowMissGroup.generalShader = 2;
    shadowMissGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    shadowMissGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shadowMissGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(shadowMissGroup);
    
    // GROUP 3: Hit
    VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = 3;
    hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(hitGroup);
    
    // GROUP 4: Shadow Hit
    VkRayTracingShaderGroupCreateInfoKHR shadowHitGroup{};
    shadowHitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shadowHitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shadowHitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    shadowHitGroup.closestHitShader = 4;
    shadowHitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    shadowHitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(shadowHitGroup);
    
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 3; // TODO: change accordingly
    pipelineInfo.layout = rtPipeline.layout;
    
    if (context.vkCreateRayTracingPipelinesKHR(context.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rtPipeline.pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create ray tracing pipeline!");
    }
    
    // BINDING TABLE
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    
    VkPhysicalDeviceProperties2KHR properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    properties2.pNext = &properties;
    
    vkGetPhysicalDeviceProperties2(context.physicalDevice, &properties2);
    
    sbt.handleSize = properties.shaderGroupHandleSize;
    sbt.handleAlign = properties.shaderGroupHandleAlignment;
    sbt.baseAlign = properties.shaderGroupBaseAlignment;
    
    uint32_t groupCount = static_cast<uint32_t>(groups.size());
    size_t groupHandleSize = groupCount * sbt.handleSize;
    std::vector<uint8_t> shaderHandleStorage(groupHandleSize);
    
    context.vkGetRayTracingShaderGroupHandlesKHR(context.device, rtPipeline.pipeline, 0, groupCount, groupHandleSize, shaderHandleStorage.data());
    
    VkDeviceSize rgenStride = alignUp(sbt.handleSize, sbt.handleAlign);
    VkDeviceSize missStride = alignUp(sbt.handleSize, sbt.handleAlign);
    VkDeviceSize hitStride = alignUp(sbt.handleSize, sbt.handleAlign);
    
    VkDeviceSize rgenSize = rgenStride * 1; // TODO: writing explicitly here for clarity
    VkDeviceSize missSize = missStride * 2;
    VkDeviceSize hitSize = hitStride * 2;
    
    VkDeviceSize rgenOffset = 0;
    VkDeviceSize missOffset = alignUpVk(rgenOffset + rgenSize, sbt.baseAlign);
    VkDeviceSize hitOffset = alignUpVk(missOffset + missSize, sbt.baseAlign);
    VkDeviceSize sbtSize = hitOffset + hitSize;
    
    // create sbt buffer
    std::vector<uint8_t> sbtBlob(sbtSize, 0);
    
    const uint8_t* rgenHandle = shaderHandleStorage.data() + sbt.handleSize * 0;
    const uint8_t* primaryMissHandle = shaderHandleStorage.data() + sbt.handleSize * 1;
    const uint8_t* shadowMissHandle = shaderHandleStorage.data() + sbt.handleSize * 2;
    const uint8_t* primaryHitHandle = shaderHandleStorage.data() + sbt.handleSize * 3;
    const uint8_t* shadowHitHandle = shaderHandleStorage.data() + sbt.handleSize * 4;

    std::memcpy(sbtBlob.data() + rgenOffset, rgenHandle, sbt.handleSize);
    std::memcpy(sbtBlob.data() + missOffset + missStride * 0, primaryMissHandle, sbt.handleSize);
    std::memcpy(sbtBlob.data() + missOffset + missStride * 1, shadowMissHandle, sbt.handleSize);
    std::memcpy(sbtBlob.data() + hitOffset + hitStride * 0, primaryHitHandle, sbt.handleSize);
    std::memcpy(sbtBlob.data() + hitOffset + hitStride * 1, shadowHitHandle, sbt.handleSize);
    
    if (!context.createDeviceLocalBuffer(sbt.buffer, sbtBlob.data(), sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
    {
        throw std::runtime_error("Failed to create device local buffer!");
    }
    
    sbt.deviceAddress = context.getBufferDeviceAddress(sbt.buffer);
    
    sbt.rgen.deviceAddress = sbt.deviceAddress + rgenOffset;
    sbt.rgen.stride = rgenStride;
    sbt.rgen.size = rgenSize;
    
    sbt.miss.deviceAddress = sbt.deviceAddress + missOffset;
    sbt.miss.stride = missStride;
    sbt.miss.size = missSize;
    
    sbt.hit.deviceAddress = sbt.deviceAddress + hitOffset;
    sbt.hit.stride = hitStride;
    sbt.hit.size = hitSize;
    
    sbt.callable.deviceAddress = 0;
    sbt.callable.stride = 0;
    sbt.callable.size = 0;
    
    vkDestroyShaderModule(context.device, rgenModule, nullptr);
    vkDestroyShaderModule(context.device, missModule, nullptr);
    vkDestroyShaderModule(context.device, hitModule, nullptr);
    vkDestroyShaderModule(context.device, shadowHitModule, nullptr);
    vkDestroyShaderModule(context.device, shadowMissModule, nullptr);
}

void App::createAccelerationStructures(
    std::vector<NPMeshRecord>& meshes, 
    std::vector<FLOAT4X4>& transforms, 
    VkDeviceAddress vertexAddress, 
    VkDeviceAddress indexAddress)
{
    VkCommandBuffer commandBuffer{};
    context.createCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
    context.beginCommandBuffer(commandBuffer);
    
    for (auto& mesh : meshes)
    {
        NPAccelerationStructure blas{};
        
        context.createBottomLevelAccelerationStructure(
            commandBuffer, 
            blas, 
            vertexAddress, 
            indexAddress, 
            mesh.vertexOffset, 
            mesh.vertexCount, 
            mesh.indexOffset, 
            mesh.indexCount);
        blasses.push_back(blas);
    }
    
    // barrier
    VkMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    barrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstStageMask =
        VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
        VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
    barrier.dstAccessMask =
        VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
        VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    
    for (auto& blas : blasses)
    {
        VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        deviceAddressInfo.accelerationStructure = blas.accelerationStructure;
        
        blas.deviceAddress = context.vkGetAccelerationStructureDeviceAddressKHR(context.device, &deviceAddressInfo);
    }
    
    // top level
    NPBuffer instanceBufferHandle{};
    context.createTopLevelAccelerationStructure(commandBuffer, tlas, instanceBufferHandle, transforms, blasses);
    
    // barrier
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    
    context.endCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
    
    instanceBufferHandle.destroy(context.allocator);
}

// CALLABLE DRAW CALL
// WIP IGNORE FOR NOW WILL NEVER BE CALLED
void App::executeDrawCallCallable(NPRendererAovs* aovs)
{
    // grab a frame
    NPFrame& frame = context.getCurrentFrame(currentRingFrame);

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    NPImage* renderTarget = aovs->color;

    populateDrawCallCallable(frame, renderTarget);

    vkResetFences(context.device, 1,
                  &frame.doneExecutingFence);  // signal that fence is ready to be associated with a
    // new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    context.endCommandBuffer(frame.commandBuffer, NPQueueType::GRAPHICS, waitDestinationStageMask,
                             frame.doneExecutingFence);

    // increment frame (within ring)
    currentRingFrame = (currentRingFrame + 1) % FRAME_COUNT;
}

// WIP IGNORE FOR NOW WILL NEVER BE CALLED
void App::populateDrawCallCallable(NPFrame& frame, NPImage* renderTarget)
{
    VkCommandBuffer& commandBuffer = frame.commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);
    context.beginCommandBuffer(commandBuffer);

    /*context.transitionImageLayout(commandBuffer, renderTarget->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);*/

    VkClearValue clearColor{};
    clearColor.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkClearValue clearDepth{};
    clearDepth.depthStencil = { 1.0f, 0 };

    // TODO: pass in image params
    VkExtent2D extent{};
    extent.width = renderTarget->width;
    extent.height = renderTarget->height;

    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = renderTarget->view;
    colorAttachmentInfo.imageLayout = renderTarget->layout;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = clearColor;

    // VkRenderingAttachmentInfo depthAttachmentInfo{};
    // depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    // depthAttachmentInfo.imageView = context.depthImage.view;
    // depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    // depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // depthAttachmentInfo.clearValue = clearDepth;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.pipeline);

    VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f
    };

    VkRect2D scissor{ { 0, 0 }, extent };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.layout, 0, 2,
                            descriptorSets.data(), 0, nullptr);

    vkCmdBindDescriptorSets(
    commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    rasterPipeline.layout,
    0,
    1,
    descriptorSets.data(),
    0,
    nullptr);
    
    for (size_t i = 0; i < indexCounts.size(); i++)
    {
        vkCmdDraw(commandBuffer, indexCounts[i], 1, 0, static_cast<uint32_t>(i));
    }

    vkCmdEndRendering(commandBuffer);

    /*context.transitionImageLayout(commandBuffer, renderTarget->image,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);*/
}

// SWAPCHAIN DRAW CALL
void App::executeDrawCallSwapchain()
{
    // grab a frame
    NPFrame& frame = context.frames[currentRingFrame];

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    // acquire image when it is done being presented
    uint32_t imageIndex;
    if (vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX,
                              frame.donePresentingSemaphore, nullptr, &imageIndex)
        == VK_ERROR_OUT_OF_DATE_KHR)
    {
        context.recreateSwapchain(window);
        return;
    }

    // populateDrawCallRaster(frame.commandBuffer, imageIndex); // TODO: make raster vs rt a macro
    populateDrawCallRT(frame.commandBuffer, imageIndex);
    vkResetFences(context.device, 1, &frame.doneExecutingFence); // signal that fence is ready to be associated with a new queue submission
    
    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.donePresentingSemaphore; // wait until image is no longer being presented
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &context.doneRenderingSemaphores[imageIndex]; // signal when rendering finishes

    VkResult submitResult = vkQueueSubmit(context.queues[NPQueueType::GRAPHICS].queue, 1, &submitInfo, frame.doneExecutingFence);
    if (submitResult != VK_SUCCESS)
    {
        std::cout << "vkQueueSubmit failed with code: " << submitResult << "\n";
        throw std::runtime_error("vkQueueSubmit failed");
    }
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores
        = &context.doneRenderingSemaphores[imageIndex];  // present after rendering finishes
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &context.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(context.queues[NPQueueType::GRAPHICS].queue, &presentInfo);
    if ((result == VK_SUBOPTIMAL_KHR) || (result == VK_ERROR_OUT_OF_DATE_KHR)
        || context.framebufferResized)
    {
        context.framebufferResized = false;
        context.recreateSwapchain(window);
    }

    // increment frame (within ring)
    currentRingFrame = (currentRingFrame + 1) % FRAME_COUNT;
    context.frameIndex++;
}

void App::populateDrawCallRaster(NPFrame& frame, uint32_t imageIndex)
{
    vkResetCommandBuffer(frame.commandBuffer, 0);
    context.beginCommandBuffer(frame.commandBuffer);

    context.transitionImageLayout(frame.commandBuffer, context.swapchainImages[imageIndex],
                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearColor{ { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkClearValue clearDepth{ { 1.0f, 0 } };

    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = context.swapchainImageViews[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue = clearColor;

    VkRenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachmentInfo.imageView = context.depthImage.view;
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentInfo.clearValue = clearDepth;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent = context.swapchainParams.extent;

    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    // record commands
    vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);
    vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.pipeline);

    VkViewport viewport{ 0.0f,
                         0.0f,
                         static_cast<float>(context.swapchainParams.extent.width),
                         static_cast<float>(context.swapchainParams.extent.height),
                         0.0f,
                         1.0f };

    VkRect2D scissor{ { 0, 0 }, context.swapchainParams.extent };

    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rasterPipeline.layout, 0, static_cast<uint32_t>(descriptorSets.size()),
                            descriptorSets.data(), 0, nullptr);
    
    for (size_t i = 0; i < indexCounts.size(); i++)
    {
        std::vector<uint32_t> pushConstants{static_cast<uint32_t>(i), numLights, context.frameIndex};
        vkCmdPushConstants(frame.commandBuffer, rasterPipeline.layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(uint32_t) * static_cast<uint32_t>(pushConstants.size()), pushConstants.data());
        vkCmdDraw(frame.commandBuffer, indexCounts[i], 1, 0, static_cast<uint32_t>(i));
    }

    vkCmdEndRendering(frame.commandBuffer);

    context.transitionImageLayout(frame.commandBuffer, context.swapchainImages[imageIndex],
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkResetFences(
        context.device, 1,
        &frame.doneExecutingFence);  // signal that fence is ready to be associated with a new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    context.endCommandBuffer(frame.commandBuffer, NPQueueType::GRAPHICS, waitDestinationStageMask,
                             frame.doneExecutingFence, frame.donePresentingSemaphore,
                             context.doneRenderingSemaphores[imageIndex]);
}

void App::populateDrawCallRT(VkCommandBuffer& commandBuffer, uint32_t imageIndex)
{
    vkResetCommandBuffer(commandBuffer, 0);
    context.beginCommandBuffer(commandBuffer);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline.layout, 0, static_cast<uint32_t>(descriptorSets.size()),
                        descriptorSets.data(), 0, nullptr);
    
    std::vector<uint32_t> pushConstants{static_cast<uint32_t>(0), numLights, context.frameIndex};
    vkCmdPushConstants(
        commandBuffer,
        rtPipeline.layout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(uint32_t) * static_cast<uint32_t>(pushConstants.size()),
        pushConstants.data());
    
    context.vkCmdTraceRaysKHR(
        commandBuffer, 
        &sbt.rgen, 
        &sbt.miss, 
        &sbt.hit, 
        &sbt.callable, 
        context.swapchainParams.extent.width, 
        context.swapchainParams.extent.height, 
        1);
    
    context.transitionImageLayout(
    commandBuffer,
    context.swapchainImages[imageIndex],
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    0,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    
    VkImageCopy region{};
    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.extent = {context.swapchainParams.extent.width, context.swapchainParams.extent.height, 1};
    
    vkCmdCopyImage(
        commandBuffer, 
        context.resultImage.image, 
        VK_IMAGE_LAYOUT_GENERAL, 
        context.swapchainImages[imageIndex], 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        1, 
        &region);

    context.transitionImageLayout(
    commandBuffer,
    context.swapchainImages[imageIndex],
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    0,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    
    vkEndCommandBuffer(commandBuffer);
}

void App::destroy()
{
    for (auto& descriptorSetLayout : descriptorSetLayouts)
    {
        descriptorSetLayout.destroy(context.device);
    }
    
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(context.device, sampler, nullptr);
    }
    
    // SET 0 : GEOMETRY
    if (meshRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        meshRecordBuffer.destroy(context.allocator);
    }

    if (vertexBuffer.buffer != VK_NULL_HANDLE)
    {
        vertexBuffer.destroy(context.allocator);
    }

    if (indexBuffer.buffer != VK_NULL_HANDLE)
    {
        indexBuffer.destroy(context.allocator);
    }
    
    // SET 1 : TRANSFOMRS
    if (geometryTransformsBuffer.buffer != VK_NULL_HANDLE)
    {
        geometryTransformsBuffer.destroy(context.allocator);
    }
    
    if (lightTransformsBuffer.buffer != VK_NULL_HANDLE)
    {
        lightTransformsBuffer.destroy(context.allocator);
    }
    
    // SET 2: CAMERA AND LIGHTS
    if (cameraRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        cameraRecordBuffer.destroy(context.allocator);
    }

    if (lightRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        lightRecordBuffer.destroy(context.allocator);
    }
    
    // SET 3: MATERIAL AND TEXTURES
    if (materialRecordsBuffer.buffer != VK_NULL_HANDLE)
    {
        materialRecordsBuffer.destroy(context.allocator);
    }
    
    for (auto& texture : textures)
    {
        texture.destroy(context.device, context.allocator);
    }
    
    // SET 4: RT
    sbt.destroy(context.allocator);
    
    for (auto& blas : blasses)
    {
        if (blas.accelerationStructure != VK_NULL_HANDLE)
        {
            context.vkDestroyAccelerationStructureKHR(context.device, blas.accelerationStructure, nullptr);
        }
        blas.destroyBuffers(context.device, context.allocator);
    }
    
    if (tlas.accelerationStructure != VK_NULL_HANDLE)
    {
        context.vkDestroyAccelerationStructureKHR(context.device, tlas.accelerationStructure, nullptr);
        tlas.destroyBuffers(context.device, context.allocator);
    }
    
    // PIPELINES
    if (rasterPipeline.pipeline != VK_NULL_HANDLE)
    {
        rasterPipeline.destroy(context.device);
    }
    
    if (rtPipeline.pipeline != VK_NULL_HANDLE)
    {
        rtPipeline.destroy(context.device);
    }
    
    context.destroy();

    if (window)
    {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

void App::loadScene(const char* path)
{
    scene->loadSceneAssimp(path);
}

void App::render()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        executeDrawCallSwapchain();
    }

    context.waitIdle();
}

void App::run()
{
    if (kSTANDALONE)
    {
        render();
    }

    destroy();
}
