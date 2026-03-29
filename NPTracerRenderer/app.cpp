#include "app.h"
#include "utils.h"

#include <iostream>

void App::create()
{
    // define `scene`
    scene = std::make_unique<Scene>();
    
    // create vulkan basics
    context.setFrameCount(FRAME_COUNT);
    if (standalone) context.createWindow(window, WIDTH, HEIGHT);
    
    context.createInstance(enableDebug);
    
    if (standalone) context.createSurface(window);
    
    context.createPhysicalDevice();
    context.createLogicalDeviceAndQueues();
    context.createAllocator();
    
    if (standalone) context.createSwapchain(window);
    
    context.createSyncAndFrameObjects();
    context.createDepthImage(WIDTH, HEIGHT);  // TODO pass actual depth aov target
    context.waitIdle();
}

// RESOURCE CREATION
void App::createRenderingResources()
{
    // uint32_t vbCount = 0;
    // uint32_t ibCount = 0;
    //
    // const size_t meshCount = scene->getMeshCount();
    // std::vector<NPMeshRecord> meshRecords;
    // meshRecords.reserve(meshCount);
    // for (int i = 0; i < meshCount; i++)
    // {
    //     NPMesh const* mesh = scene->getMeshAtIndex(i);
    //     NPMeshRecord meshRecord{};
    //     meshRecord.vbIdx = vbCount++;
    //     meshRecord.ibIdx = ibCount++;
    //
    //     NPBuffer vertexBuffer;
    //     NPBuffer indexBuffer;
    //
    //     const std::vector<NPVertex>& vertices = mesh->vertices;
    //     VkDeviceSize vbSize = sizeof(vertices[0]) * vertices.size();
    //     context.createDeviceLocalBuffer(vertexBuffer, const_cast<NPVertex*>(vertices.data()),
    //                                     vbSize,
    //                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    //                                         | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    //     vertexBuffers.push_back(vertexBuffer);
    //
    //     VkDeviceSize ibSize = sizeof(mesh->indices[0]) * mesh->indices.size();
    //     context.createDeviceLocalBuffer(indexBuffer, (void*)mesh->indices.data(), ibSize,
    //                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    //                                         | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    //     indexBuffers.push_back(indexBuffer);
    //
    //     // temp
    //     indexCounts.push_back(static_cast<uint32_t>(mesh->indices.size()));
    //     meshRecords.push_back(meshRecord);
    // }
    //
    // bool meshRecordCreated = false;
    // VkDeviceSize meshRecordSize = sizeof(meshRecords[0]) * meshRecords.size();
    // meshRecordCreated = context.createDeviceLocalBuffer(meshRecordBuffer, meshRecords.data(),
    //                                                     meshRecordSize,
    //                                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    //
    // // create camera buffer
    // bool cameraRecordCreated = false;
    // VkDeviceSize cameraSize = sizeof(NPCameraRecord);
    // NPCameraRecord* cam = scene->getCamera();
    //
    // cameraRecordCreated = context.createDeviceLocalBuffer(cameraRecordBuffer, cam, cameraSize,
    //                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    //
    // // create light buffer
    // bool lightRecordCreated = false;
    // const size_t lightCount = scene->getLightCount();
    // std::vector<NPLightRecord> lightRecords;
    // lightRecords.reserve(lightCount);
    // for (int i = 0; i < lightCount; i++)
    // {
    //     NPLight const* light = scene->getLightAtIndex(i);
    //     NPLightRecord lightRecord;
    //     lightRecord.transform = light->transform;
    //     lightRecord.intensity = light->intensity;
    //     lightRecord.color = light->color;
    //     lightRecords.push_back(lightRecord);
    // }
    //
    // VkDeviceSize lightSize = sizeof(NPLightRecord) * lightRecords.size();
    // lightRecordCreated = context.createDeviceLocalBuffer(lightRecordBuffer, lightRecords.data(),
    //                                                      lightSize,
    //                                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    //
    // // CREATE EVERYTHING
    // if (meshRecordCreated)
    // {
    //     // layout
    //     NPDescriptorSetLayout meshDescriptorSetLayout;
    //     VkDescriptorSetLayoutBinding binding0{};
    //     binding0.binding = 0;
    //     binding0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     binding0.descriptorCount = 1;
    //     binding0.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    //     binding0.pImmutableSamplers = nullptr;
    //
    //     VkDescriptorSetLayoutBinding binding1{};
    //     binding1.binding = 1;
    //     binding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     binding1.descriptorCount = MAX_RESOURCE_COUNT;
    //     binding1.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    //     binding1.pImmutableSamplers = nullptr;
    //
    //     VkDescriptorSetLayoutBinding binding2{};
    //     binding2.binding = 2;
    //     binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     binding2.descriptorCount = MAX_RESOURCE_COUNT;
    //     binding2.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    //     binding2.pImmutableSamplers = nullptr;
    //
    //     std::vector<VkDescriptorSetLayoutBinding> meshBindings{ {
    //         binding0,
    //         binding1,
    //         binding2,
    //     } };
    //
    //     std::vector<VkDescriptorBindingFlags> meshBindingFlags{
    //         0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    //     };
    //
    //     VkDescriptorSetLayoutBindingFlagsCreateInfo meshFlagsInfo{};
    //     meshFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    //     meshFlagsInfo.bindingCount = static_cast<uint32_t>(meshBindings.size());
    //     meshFlagsInfo.pBindingFlags = meshBindingFlags.data();
    //
    //     VkDescriptorSetLayoutCreateInfo meshLayoutInfo{};
    //     meshLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //     meshLayoutInfo.pNext = &meshFlagsInfo;
    //     meshLayoutInfo.bindingCount = static_cast<uint32_t>(meshBindings.size());
    //     meshLayoutInfo.pBindings = meshBindings.data();
    //
    //     vkCreateDescriptorSetLayout(context.device, &meshLayoutInfo, nullptr,
    //                                 &meshDescriptorSetLayout.layout);
    //
    //     VkDescriptorPoolSize meshPoolSize{};
    //     meshPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     meshPoolSize.descriptorCount = 2 * MAX_RESOURCE_COUNT + 1;
    //
    //     std::vector<VkDescriptorPoolSize> meshPoolSizes{ {
    //         meshPoolSize,
    //     } };
    //
    //     VkDescriptorPoolCreateInfo meshPoolInfo{};
    //     meshPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    //     meshPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    //     meshPoolInfo.maxSets = 1;
    //     meshPoolInfo.poolSizeCount = static_cast<uint32_t>(meshPoolSizes.size());
    //     meshPoolInfo.pPoolSizes = meshPoolSizes.data();
    //
    //     vkCreateDescriptorPool(context.device, &meshPoolInfo, nullptr,
    //                            &meshDescriptorSetLayout.pool);
    //     descriptorSetLayouts.emplace_back(meshDescriptorSetLayout);
    //
    //     // allocation
    //     VkDescriptorSet meshDescriptorSet;
    //     VkDescriptorSetAllocateInfo meshAllocInfo{};
    //     meshAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    //     meshAllocInfo.descriptorPool = meshDescriptorSetLayout.pool;
    //     meshAllocInfo.descriptorSetCount = 1;
    //     meshAllocInfo.pSetLayouts = &meshDescriptorSetLayout.layout;
    //     vkAllocateDescriptorSets(context.device, &meshAllocInfo, &meshDescriptorSet);
    //
    //     // write
    //     // binding 0: mesh records
    //     VkDescriptorBufferInfo meshRecordInfo{};
    //     meshRecordInfo.buffer = meshRecordBuffer.buffer;
    //     meshRecordInfo.offset = 0;
    //     meshRecordInfo.range = VK_WHOLE_SIZE;
    //
    //     // binding 1: vertex buffers
    //     std::vector<VkDescriptorBufferInfo> vertexBufferInfos(vertexBuffers.size());
    //     for (size_t i = 0; i < vertexBuffers.size(); i++)
    //     {
    //         vertexBufferInfos[i].buffer = vertexBuffers[i].buffer;
    //         vertexBufferInfos[i].offset = 0;
    //         vertexBufferInfos[i].range = VK_WHOLE_SIZE;
    //     }
    //
    //     // binding 2: index buffers
    //     std::vector<VkDescriptorBufferInfo> indexBufferInfos(indexBuffers.size());
    //     for (size_t i = 0; i < indexBuffers.size(); i++)
    //     {
    //         indexBufferInfos[i].buffer = indexBuffers[i].buffer;
    //         indexBufferInfos[i].offset = 0;
    //         indexBufferInfos[i].range = VK_WHOLE_SIZE;
    //     }
    //
    //     // descriptor write 0: mesh records
    //     VkWriteDescriptorSet write0{};
    //     write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     write0.dstSet = meshDescriptorSet;
    //     write0.dstBinding = 0;
    //     write0.dstArrayElement = 0;
    //     write0.descriptorCount = 1;
    //     write0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     write0.pBufferInfo = &meshRecordInfo;
    //
    //     // descriptor write 1: vertex buffers
    //     VkWriteDescriptorSet write1{};
    //     write1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     write1.dstSet = meshDescriptorSet;
    //     write1.dstBinding = 1;
    //     write1.dstArrayElement = 0;
    //     write1.descriptorCount = static_cast<uint32_t>(vertexBufferInfos.size());
    //     write1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     write1.pBufferInfo = vertexBufferInfos.data();
    //
    //     // descriptor write 2: index buffers
    //     VkWriteDescriptorSet write2{};
    //     write2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     write2.dstSet = meshDescriptorSet;
    //     write2.dstBinding = 2;
    //     write2.dstArrayElement = 0;
    //     write2.descriptorCount = static_cast<uint32_t>(indexBufferInfos.size());
    //     write2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     write2.pBufferInfo = indexBufferInfos.data();
    //
    //     std::vector<VkWriteDescriptorSet> descriptorWrites{ write0, write1, write2 };
    //
    //     vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()),
    //                            descriptorWrites.data(), 0, nullptr);
    //
    //     descriptorSets.push_back(meshDescriptorSet);
    // }
    //
    // if (cameraRecordCreated)
    // {
    //     NPDescriptorSetLayout cameraDescriptorSetLayout;
    //     VkDescriptorSetLayoutBinding cameraBinding{};
    //     cameraBinding.binding = 0;
    //     cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //     cameraBinding.descriptorCount = 1;
    //     cameraBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    //     cameraBinding.pImmutableSamplers = nullptr;
    //
    //     std::vector<VkDescriptorSetLayoutBinding> cameraBindings{ cameraBinding };
    //
    //     VkDescriptorSetLayoutCreateInfo cameraLayoutInfo{};
    //     cameraLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //     cameraLayoutInfo.bindingCount = static_cast<uint32_t>(cameraBindings.size());
    //     cameraLayoutInfo.pBindings = cameraBindings.data();
    //
    //     vkCreateDescriptorSetLayout(context.device, &cameraLayoutInfo, nullptr,
    //                                 &cameraDescriptorSetLayout.layout);
    //
    //     VkDescriptorPoolSize cameraPoolSize{};
    //     cameraPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //     cameraPoolSize.descriptorCount = 1;
    //
    //     std::vector<VkDescriptorPoolSize> cameraPoolSizes{ {
    //         cameraPoolSize,
    //     } };
    //
    //     VkDescriptorPoolCreateInfo cameraPoolInfo{};
    //     cameraPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    //     cameraPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    //     cameraPoolInfo.maxSets = 1;
    //     cameraPoolInfo.poolSizeCount = static_cast<uint32_t>(cameraPoolSizes.size());
    //     cameraPoolInfo.pPoolSizes = cameraPoolSizes.data();
    //
    //     vkCreateDescriptorPool(context.device, &cameraPoolInfo, nullptr,
    //                            &cameraDescriptorSetLayout.pool);
    //     descriptorSetLayouts.emplace_back(cameraDescriptorSetLayout);
    //
    //     VkDescriptorSet cameraDescriptorSet;
    //     VkDescriptorSetAllocateInfo cameraAllocInfo{};
    //     cameraAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    //     cameraAllocInfo.descriptorPool = cameraDescriptorSetLayout.pool;
    //     cameraAllocInfo.descriptorSetCount = 1;
    //     cameraAllocInfo.pSetLayouts = &cameraDescriptorSetLayout.layout;
    //     vkAllocateDescriptorSets(context.device, &cameraAllocInfo, &cameraDescriptorSet);
    //
    //     // camera
    //     VkDescriptorBufferInfo cameraRecordInfo{};
    //     cameraRecordInfo.buffer = cameraRecordBuffer.buffer;
    //     cameraRecordInfo.offset = 0;
    //     cameraRecordInfo.range = VK_WHOLE_SIZE;
    //
    //     VkWriteDescriptorSet cameraWrite{};
    //     cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     cameraWrite.dstSet = cameraDescriptorSet;
    //     cameraWrite.dstBinding = 0;
    //     cameraWrite.dstArrayElement = 0;
    //     cameraWrite.descriptorCount = 1;
    //     cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    //     cameraWrite.pBufferInfo = &cameraRecordInfo;
    //
    //     vkUpdateDescriptorSets(context.device, 1, &cameraWrite, 0, nullptr);
    //     descriptorSets.push_back(cameraDescriptorSet);
    // }
    //
    // if (lightRecordCreated)
    // {
    //     NPDescriptorSetLayout lightDescriptorSetLayout;
    //     VkDescriptorSetLayoutBinding lightBinding{};
    //     lightBinding.binding = 0;
    //     lightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     lightBinding.descriptorCount = 1;
    //     lightBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    //     lightBinding.pImmutableSamplers = nullptr;
    //
    //     std::vector<VkDescriptorSetLayoutBinding> lightBindings{ lightBinding };
    //
    //     VkDescriptorSetLayoutCreateInfo lightLayoutInfo{};
    //     lightLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    //     lightLayoutInfo.bindingCount = static_cast<uint32_t>(lightBindings.size());
    //     lightLayoutInfo.pBindings = lightBindings.data();
    //
    //     vkCreateDescriptorSetLayout(context.device, &lightLayoutInfo, nullptr,
    //                                 &lightDescriptorSetLayout.layout);
    //
    //     VkDescriptorPoolSize lightPoolSize{};
    //     lightPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     lightPoolSize.descriptorCount = 1;
    //
    //     std::vector<VkDescriptorPoolSize> lightPoolSizes{ {
    //         lightPoolSize,
    //     } };
    //
    //     VkDescriptorPoolCreateInfo lightPoolInfo{};
    //     lightPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    //     lightPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    //     lightPoolInfo.maxSets = 1;
    //     lightPoolInfo.poolSizeCount = static_cast<uint32_t>(lightPoolSizes.size());
    //     lightPoolInfo.pPoolSizes = lightPoolSizes.data();
    //
    //     vkCreateDescriptorPool(context.device, &lightPoolInfo, nullptr,
    //                            &lightDescriptorSetLayout.pool);
    //     descriptorSetLayouts.emplace_back(lightDescriptorSetLayout);
    //
    //     VkDescriptorSet lightDescriptorSet;
    //     VkDescriptorSetAllocateInfo lightAllocInfo{};
    //     lightAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    //     lightAllocInfo.descriptorPool = lightDescriptorSetLayout.pool;
    //     lightAllocInfo.descriptorSetCount = 1;
    //     lightAllocInfo.pSetLayouts = &lightDescriptorSetLayout.layout;
    //
    //     vkAllocateDescriptorSets(context.device, &lightAllocInfo, &lightDescriptorSet);
    //
    //     // lights
    //     VkDescriptorBufferInfo lightBufferInfo;
    //     lightBufferInfo.buffer = lightRecordBuffer.buffer;
    //     lightBufferInfo.offset = 0;
    //     lightBufferInfo.range = VK_WHOLE_SIZE;
    //
    //     VkWriteDescriptorSet lightWrite{};
    //     lightWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    //     lightWrite.dstSet = lightDescriptorSet;
    //     lightWrite.dstBinding = 0;
    //     lightWrite.dstArrayElement = 0;
    //     lightWrite.descriptorCount = 1;
    //     lightWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //     lightWrite.pBufferInfo = &lightBufferInfo;
    //
    //     vkUpdateDescriptorSets(context.device, 1, &lightWrite, 0, nullptr);
    //     descriptorSets.push_back(lightDescriptorSet);
    // }
    
    createGraphicsPipeline();
}

void App::createGraphicsPipeline()
{
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

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicInfo.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInfo{};
    vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInfo.vertexBindingDescriptionCount = 0;
    vertexInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputInfo{};
    inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterInfo{};
    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.depthClampEnable = VK_FALSE;
    rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterInfo.cullMode = VK_CULL_MODE_NONE; // easier for fullscreen tri
    rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;
    rasterInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendInfo{};
    blendInfo.blendEnable = VK_FALSE;
    blendInfo.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blendStateInfo{};
    blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendStateInfo.logicOpEnable = VK_FALSE;
    blendStateInfo.attachmentCount = 1;
    blendStateInfo.pAttachments = &blendInfo;

    // For this test, disable depth entirely.
    VkPipelineDepthStencilStateCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthInfo.depthTestEnable = VK_FALSE;
    depthInfo.depthWriteEnable = VK_FALSE;
    depthInfo.depthBoundsTestEnable = VK_FALSE;
    depthInfo.stencilTestEnable = VK_FALSE;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipeline.layout);

    VkFormat colorFormat = m_aovs
        ? m_aovs->color->format
        : context.swapchainParams.format.format;

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED; // no depth in this test

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
    pipelineInfo.layout = pipeline.layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline.pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline");
    }

    vkDestroyShaderModule(context.device, coreVertModule, nullptr);
    vkDestroyShaderModule(context.device, coreFragModule, nullptr);
    
    // // shader creation
    // VkShaderModule coreVertModule = context.createShaderModule(readFile(NPTRACER_SHADER_CORE_VERT));
    // VkShaderModule coreFragModule = context.createShaderModule(readFile(NPTRACER_SHADER_CORE_FRAG));
    //
    // VkPipelineShaderStageCreateInfo vInfo{};
    // vInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    // vInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    // vInfo.module = coreVertModule;
    // vInfo.pName = "vertMain";
    //
    // VkPipelineShaderStageCreateInfo fInfo{};
    // fInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    // fInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    // fInfo.module = coreFragModule;
    // fInfo.pName = "fragMain";
    //
    // VkPipelineShaderStageCreateInfo shaderStages[] = { vInfo, fInfo };
    //
    // // viewport
    // std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
    //                                               VK_DYNAMIC_STATE_SCISSOR };
    //
    // VkPipelineDynamicStateCreateInfo dynamicInfo{};
    // dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // dynamicInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    // dynamicInfo.pDynamicStates = dynamicStates.data();
    //
    // VkPipelineVertexInputStateCreateInfo vertexInfo{};
    // vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // vertexInfo.vertexBindingDescriptionCount = 0;
    // vertexInfo.pVertexBindingDescriptions = nullptr;
    // vertexInfo.vertexAttributeDescriptionCount = 0;
    // vertexInfo.pVertexAttributeDescriptions = nullptr;
    //
    // VkPipelineInputAssemblyStateCreateInfo inputInfo{};
    // inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    //
    // VkViewport viewport{
    //     0.0f, 0.0f, 0, 0,
    //     0.0f, 1.0f
    // };
    // viewport.width = m_aovs ? m_aovs->color->width : context.swapchainParams.extent.width;
    // viewport.height = m_aovs ? m_aovs->color->height : context.swapchainParams.extent.height;
    //
    // VkExtent2D extent{};
    // extent.width = m_aovs ? m_aovs->color->width : context.swapchainParams.extent.width;
    // extent.height = m_aovs ? m_aovs->color->height : context.swapchainParams.extent.height;
    // VkRect2D rect{ VkOffset2D{ 0, 0 }, extent };
    //
    // VkPipelineViewportStateCreateInfo viewportState{};
    // viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    // viewportState.viewportCount = 1;
    // viewportState.pViewports = nullptr;
    // viewportState.scissorCount = 1;
    // viewportState.pScissors = nullptr;
    //
    // // rasterizer
    // VkPipelineRasterizationStateCreateInfo rasterInfo{};
    // rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // rasterInfo.depthClampEnable = VK_FALSE;
    // rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    // rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
    // rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    // rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // rasterInfo.depthBiasEnable = VK_FALSE;
    // rasterInfo.depthBiasSlopeFactor = 1.0f;
    // rasterInfo.lineWidth = 1.0f;
    //
    // VkPipelineMultisampleStateCreateInfo multisampling{};
    // multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    // multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // multisampling.sampleShadingEnable = VK_FALSE;
    //
    // // color blending
    // VkPipelineColorBlendAttachmentState blendInfo{};
    // blendInfo.blendEnable = VK_FALSE;
    // blendInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
    //                            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    //
    // VkPipelineColorBlendStateCreateInfo blendStateInfo{};
    // blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    // blendStateInfo.logicOpEnable = VK_FALSE;
    // blendStateInfo.logicOp = VK_LOGIC_OP_COPY;
    // blendStateInfo.attachmentCount = 1;
    // blendStateInfo.pAttachments = &blendInfo;
    //
    // // depth testing
    // VkPipelineDepthStencilStateCreateInfo depthInfo{};
    // depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // depthInfo.depthTestEnable = VK_TRUE;
    // depthInfo.depthWriteEnable = VK_TRUE;
    // depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    // depthInfo.depthBoundsTestEnable = VK_FALSE;
    // depthInfo.stencilTestEnable = VK_FALSE;
    //
    // // pipeline layout
    // std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    // vkDescriptorSetLayouts.reserve(descriptorSetLayouts.size());
    // for (auto& descriptorSetLayout : descriptorSetLayouts)
    // {
    //     vkDescriptorSetLayouts.push_back(descriptorSetLayout.layout);
    // }
    //
    // VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    // pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    // pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    // pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
    // pipelineLayoutInfo.pushConstantRangeCount = 0;
    //
    // vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipeline.layout);
    //
    // VkPipelineRenderingCreateInfo renderingInfo{};
    // renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    // renderingInfo.colorAttachmentCount = 1;
    // renderingInfo.pColorAttachmentFormats = m_aovs ? &m_aovs->color->format : &context.swapchainParams.format.format;
    // renderingInfo.depthAttachmentFormat = context.depthFormat;
    //
    // VkGraphicsPipelineCreateInfo pipelineInfo{};
    // pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    // pipelineInfo.pNext = &renderingInfo;
    // pipelineInfo.stageCount = 2;
    // pipelineInfo.pStages = shaderStages;
    // pipelineInfo.pVertexInputState = &vertexInfo;
    // pipelineInfo.pInputAssemblyState = &inputInfo;
    // pipelineInfo.pViewportState = &viewportState;
    // pipelineInfo.pRasterizationState = &rasterInfo;
    // pipelineInfo.pMultisampleState = &multisampling;
    // pipelineInfo.pDepthStencilState = &depthInfo;
    // pipelineInfo.pColorBlendState = &blendStateInfo;
    // pipelineInfo.pDynamicState = &dynamicInfo;
    // pipelineInfo.layout = pipeline.layout;
    // pipelineInfo.renderPass = nullptr;
    //
    // if (vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipelineInfo, nullptr,
    //                               &pipeline.pipeline)
    //     != VK_SUCCESS)
    // {
    //     throw std::runtime_error("failed to create pipeline");
    // }
    //
    // vkDestroyShaderModule(context.device, coreVertModule, nullptr);
    // vkDestroyShaderModule(context.device, coreFragModule, nullptr);
}

// CALLABLE DRAW CALL
void App::executeDrawCallCallable(NPRendererAovs* aovs)
{
    // grab a frame
    NPFrame& frame = context.getCurrentFrame(currentFrame);

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    NPImage* renderTarget = aovs->color;

    populateDrawCallCallable(frame.commandBuffer, renderTarget);

    vkResetFences(context.device, 1,
                  &frame.doneExecutingFence);  // signal that fence is ready to be associated with a
    // new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.commandBuffer;

    // signal that rendering is finished once execution is finished
    vkQueueSubmit(context.queues[NPQueueType::GRAPHICS].queue, 1, &submitInfo,
                  frame.doneExecutingFence);  // signal that execution has been completed on frame
    // once it is done

    // increment frame (within ring)
    currentFrame = (currentFrame + 1) % FRAME_COUNT;
}

void App::populateDrawCallCallable(VkCommandBuffer& commandBuffer, NPImage* renderTarget)
{
    vkResetCommandBuffer(commandBuffer, 0);
    context.beginCommandBuffer(commandBuffer);

    context.transitionImageLayout(commandBuffer, renderTarget->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearColor{};
    clearColor.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkClearValue clearDepth{};
    clearDepth.depthStencil = { 1.0f, 0 };

    // TODO pass in image params
    VkExtent2D extent{};
    extent.width = WIDTH;
    extent.height = HEIGHT;

    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = renderTarget->view;
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
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    VkViewport viewport{ 0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f };

    VkRect2D scissor{ { 0, 0 }, extent };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 2,
                            descriptorSets.data(), 0, nullptr);

    for (size_t i = 0; i < indexCounts.size(); i++)
    {
        vkCmdDraw(commandBuffer, indexCounts[i], 1, 0, i);
    }

    vkCmdEndRendering(commandBuffer);

    context.transitionImageLayout(commandBuffer, renderTarget->image,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);
}

// SWAPCHAIN DRAW CALL
void App::executeDrawCallSwapchain()
{
    // grab a frame
    NPFrame& frame = context.frames[currentFrame];

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);
    
    // acquire image when it is done being presented
    uint32_t imageIndex;
    if (vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX, frame.donePresentingSemaphore, nullptr,
        &imageIndex) == VK_ERROR_OUT_OF_DATE_KHR)
    {
        context.recreateSwapchain(window);
        return;
    }

    populateDrawCallSwapchain(frame.commandBuffer, imageIndex); // record commands into frame's command buffer 
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
    presentInfo.pWaitSemaphores = &context.doneRenderingSemaphores[imageIndex]; // present after rendering finishes
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &context.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(context.queues[NPQueueType::GRAPHICS].queue, &presentInfo);
    if ((result == VK_SUBOPTIMAL_KHR) || (result == VK_ERROR_OUT_OF_DATE_KHR) || context.framebufferResized)
    {
        context.framebufferResized = false;
        context.recreateSwapchain(window);
    }

    // increment frame (within ring)
    currentFrame = (currentFrame + 1) % FRAME_COUNT;
}

void App::populateDrawCallSwapchain(VkCommandBuffer& commandBuffer, uint32_t imageIndex)
{
    vkResetCommandBuffer(commandBuffer, 0);
    context.beginCommandBuffer(commandBuffer);

    context.transitionImageLayout(
        commandBuffer,
        context.swapchainImages[imageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
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
    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    VkViewport viewport{
        0.0f, 0.0f, (float)context.swapchainParams.extent.width, (float)context.swapchainParams.extent.height,
        0.0f, 1.0f
    };

    VkRect2D scissor{ { 0, 0 }, context.swapchainParams.extent };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // VkDeviceSize offset = 0;
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 2,
    //                         descriptorSets.data(), 0, nullptr);
    //
    // for (size_t i = 0; i < indexCounts.size(); i++)
    // {
    //     vkCmdDraw(commandBuffer, indexCounts[i], 1, 0, i);
    // }
    
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);

    context.transitionImageLayout(
        commandBuffer,
        context.swapchainImages[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);
}

void App::destroy()
{
    if (pipeline.pipeline != VK_NULL_HANDLE)
    {
        pipeline.destroy(context.device);
    }
    
    for (auto& descriptorSetLayout : descriptorSetLayouts)
    {
        descriptorSetLayout.destroy(context.device);
    }

    if (meshRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        meshRecordBuffer.destroy(context.allocator);
    }

    if (cameraRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        cameraRecordBuffer.destroy(context.allocator);
    }

    if (lightRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        lightRecordBuffer.destroy(context.allocator);
    }

    for (auto& buffer : vertexBuffers)
    {
        buffer.destroy(context.allocator);
    }

    for (auto& buffer : indexBuffers)
    {
        buffer.destroy(context.allocator);
    }

    if (pipeline.pipeline != VK_NULL_HANDLE)
    {
        pipeline.destroy(context.device);
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
    createRenderingResources();
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
    if (standalone) render();
    
    destroy();
}
