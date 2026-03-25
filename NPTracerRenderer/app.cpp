#include "app.h"
#include "utils.h"

void App::create()
{
    // define `scene`
    scene = std::make_unique<Scene>();

    // create vulkan basics
    context.setFrameCount(FRAME_COUNT);
    context.createWindow(window, WIDTH, HEIGHT);
    context.createInstance(enableDebug);
    context.createSurface(window);
    context.createPhysicalDevice();
    context.createLogicalDeviceAndQueues();
    context.createAllocator();
    context.createSwapchain(window);
    context.createSyncAndFrameObjects();
    context.createDepthImage();
}

// RESOURCE CREATION
void App::createRenderingResources(NPRendererAovs& aovs)
{
    uint32_t vbCount = 0;
    uint32_t ibCount = 0;

    for (const NPMesh& mesh : scene->getMeshes())
    {
        NPMeshRecord meshRecord{};
        meshRecord.vbIdx = vbCount++;
        meshRecord.ibIdx = ibCount++;

        NPBuffer vertexBuffer;
        NPBuffer indexBuffer;

        const std::vector<NPVertex>& vertices = mesh.vertices;
        VkDeviceSize vbSize = sizeof(vertices[0]) * vertices.size();
        context.createDeviceLocalBuffer(vertexBuffer, const_cast<NPVertex*>(vertices.data()),
                                        vbSize,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        vertexBuffers.push_back(vertexBuffer);

        VkDeviceSize ibSize = sizeof(mesh.indices[0]) * mesh.indices.size();
        context.createDeviceLocalBuffer(indexBuffer, (void*)mesh.indices.data(), ibSize,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        indexBuffers.push_back(indexBuffer);

        meshRecords.push_back(meshRecord);
    }

    VkDeviceSize meshRecordSize = sizeof(meshRecords[0]) * meshRecords.size();
    context.createDeviceLocalBuffer(meshRecordBuffer, meshRecords.data(), meshRecordSize,
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    // create camera buffer
    VkDeviceSize cameraSize = sizeof(NPCameraRecord);
    NPCameraRecord* cam = scene->getCamera();

    context.createDeviceLocalBuffer(cameraRecordBuffer, cam, cameraSize,
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // CREATE MESH DESCRIPTOR SET LAYOUT

    NPDescriptorSetLayout meshDescriptorSetLayout;

    VkDescriptorSetLayoutBinding binding0{};
    binding0.binding = 0;
    binding0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding0.descriptorCount = 1;
    binding0.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    binding0.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding binding1{};
    binding1.binding = 1;
    binding1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding1.descriptorCount = MAX_RESOURCE_COUNT;
    binding1.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    binding1.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding binding2{};
    binding2.binding = 2;
    binding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding2.descriptorCount = MAX_RESOURCE_COUNT;
    binding2.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    binding2.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> meshBindings{ {
        binding0,
        binding1,
        binding2,
    } };

    std::vector<VkDescriptorBindingFlags> meshBindingFlags{
        0, 0,
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
            | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo meshFlagsInfo{};
    meshFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    meshFlagsInfo.bindingCount = static_cast<uint32_t>(meshBindings.size());
    meshFlagsInfo.pBindingFlags = meshBindingFlags.data();

    VkDescriptorSetLayoutCreateInfo meshLayoutInfo{};
    meshLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    meshLayoutInfo.pNext = &meshFlagsInfo;
    meshLayoutInfo.bindingCount = static_cast<uint32_t>(meshBindings.size());
    meshLayoutInfo.pBindings = meshBindings.data();

    vkCreateDescriptorSetLayout(context.device, &meshLayoutInfo, nullptr,
                                &meshDescriptorSetLayout.layout);

    VkDescriptorPoolSize meshPoolSize{};
    meshPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    meshPoolSize.descriptorCount = 2 * MAX_RESOURCE_COUNT + 1;

    std::vector<VkDescriptorPoolSize> meshPoolSizes{ {
        meshPoolSize,
    } };

    VkDescriptorPoolCreateInfo meshPoolInfo{};
    meshPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    meshPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    meshPoolInfo.maxSets = 1;
    meshPoolInfo.poolSizeCount = static_cast<uint32_t>(meshPoolSizes.size());
    meshPoolInfo.pPoolSizes = meshPoolSizes.data();

    vkCreateDescriptorPool(context.device, &meshPoolInfo, nullptr, &meshDescriptorSetLayout.pool);
    descriptorSetLayouts.emplace_back(meshDescriptorSetLayout);

    // CREATE CAMERA DESCRIPTOR SET LAYOUT

    NPDescriptorSetLayout cameraDescriptorSetLayout;
    VkDescriptorSetLayoutBinding cameraBinding{};
    cameraBinding.binding = 0;
    cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBinding.descriptorCount = 1;
    cameraBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    cameraBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> cameraBindings{ cameraBinding };

    VkDescriptorSetLayoutCreateInfo cameraLayoutInfo{};
    cameraLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    cameraLayoutInfo.bindingCount = static_cast<uint32_t>(cameraBindings.size());
    cameraLayoutInfo.pBindings = cameraBindings.data();

    vkCreateDescriptorSetLayout(context.device, &cameraLayoutInfo, nullptr,
                                &cameraDescriptorSetLayout.layout);

    VkDescriptorPoolSize cameraPoolSize{};
    cameraPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraPoolSize.descriptorCount = 1;

    std::vector<VkDescriptorPoolSize> cameraPoolSizes{ {
        cameraPoolSize,
    } };

    VkDescriptorPoolCreateInfo cameraPoolInfo{};
    cameraPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    cameraPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    cameraPoolInfo.maxSets = 1;
    cameraPoolInfo.poolSizeCount = static_cast<uint32_t>(cameraPoolSizes.size());
    cameraPoolInfo.pPoolSizes = cameraPoolSizes.data();

    vkCreateDescriptorPool(context.device, &cameraPoolInfo, nullptr,
                           &cameraDescriptorSetLayout.pool);
    descriptorSetLayouts.emplace_back(cameraDescriptorSetLayout);

    // DESCRIPTOR SET CREATION
    VkDescriptorSet meshDescriptorSet;
    VkDescriptorSet cameraDescriptorSet;

    VkDescriptorSetAllocateInfo meshAllocInfo{};
    meshAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    meshAllocInfo.descriptorPool = meshDescriptorSetLayout.pool;
    meshAllocInfo.descriptorSetCount = 1;
    meshAllocInfo.pSetLayouts = &meshDescriptorSetLayout.layout;

    VkDescriptorSetAllocateInfo cameraAllocInfo{};
    cameraAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    cameraAllocInfo.descriptorPool = cameraDescriptorSetLayout.pool;
    cameraAllocInfo.descriptorSetCount = 1;
    cameraAllocInfo.pSetLayouts = &cameraDescriptorSetLayout.layout;

    vkAllocateDescriptorSets(context.device, &meshAllocInfo, &meshDescriptorSet);
    vkAllocateDescriptorSets(context.device, &cameraAllocInfo, &cameraDescriptorSet);

    // binding 0: mesh records
    VkDescriptorBufferInfo meshRecordInfo{};
    meshRecordInfo.buffer = meshRecordBuffer.buffer;
    meshRecordInfo.offset = 0;
    meshRecordInfo.range = VK_WHOLE_SIZE;

    // binding 1: vertex buffers
    std::vector<VkDescriptorBufferInfo> vertexBufferInfos(vertexBuffers.size());
    for (size_t i = 0; i < vertexBuffers.size(); i++)
    {
        vertexBufferInfos[i].buffer = vertexBuffers[i].buffer;
        vertexBufferInfos[i].offset = 0;
        vertexBufferInfos[i].range = VK_WHOLE_SIZE;
    }

    // binding 2: index buffers
    std::vector<VkDescriptorBufferInfo> indexBufferInfos(indexBuffers.size());
    for (size_t i = 0; i < indexBuffers.size(); i++)
    {
        indexBufferInfos[i].buffer = indexBuffers[i].buffer;
        indexBufferInfos[i].offset = 0;
        indexBufferInfos[i].range = VK_WHOLE_SIZE;
    }

    // descriptor write 0: mesh records
    VkWriteDescriptorSet write0{};
    write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write0.dstSet = meshDescriptorSet;
    write0.dstBinding = 0;
    write0.dstArrayElement = 0;
    write0.descriptorCount = 1;
    write0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write0.pBufferInfo = &meshRecordInfo;

    // descriptor write 1: vertex buffers
    VkWriteDescriptorSet write1{};
    write1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write1.dstSet = meshDescriptorSet;
    write1.dstBinding = 1;
    write1.dstArrayElement = 0;
    write1.descriptorCount = static_cast<uint32_t>(vertexBufferInfos.size());
    write1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write1.pBufferInfo = vertexBufferInfos.data();

    // descriptor write 2: index buffers
    VkWriteDescriptorSet write2{};
    write2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write2.dstSet = meshDescriptorSet;
    write2.dstBinding = 2;
    write2.dstArrayElement = 0;
    write2.descriptorCount = static_cast<uint32_t>(indexBufferInfos.size());
    write2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write2.pBufferInfo = indexBufferInfos.data();

    std::vector<VkWriteDescriptorSet> descriptorWrites{ write0, write1, write2 };

    vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);

    // camera
    VkDescriptorBufferInfo cameraRecordInfo{};
    cameraRecordInfo.buffer = cameraRecordBuffer.buffer;
    cameraRecordInfo.offset = 0;
    cameraRecordInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet cameraWrite{};
    cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraWrite.dstSet = cameraDescriptorSet;
    cameraWrite.dstBinding = 0;
    cameraWrite.dstArrayElement = 0;
    cameraWrite.descriptorCount = 1;
    cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWrite.pBufferInfo = &cameraRecordInfo;

    vkUpdateDescriptorSets(context.device, 1, &cameraWrite, 0, nullptr);

    // store
    descriptorSets.push_back(meshDescriptorSet);
    descriptorSets.push_back(cameraDescriptorSet);
}

void App::createGraphicsPipeline(NPPipeline& pipeline,
                                 std::vector<NPDescriptorSetLayout>& descriptorSetLayouts,
                                 NPRendererAovs& aovs)
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

    VkVertexInputBindingDescription bindingDescription = NPVertex::getBindingDescription();
    auto attributeDescriptions = NPVertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInfo{};
    vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInfo.vertexBindingDescriptionCount = 1;
    vertexInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputInfo{};
    inputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(aovs.color->width), static_cast<float>(aovs.color->height),
        0.0f, 1.0f
    };

    VkExtent2D extent{};
    extent.width = aovs.color->width;
    extent.height = aovs.color->height;
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

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipeline.layout);

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &aovs.color->format;
    renderingInfo.depthAttachmentFormat = context.swapchainParams.depthFormat;

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
    pipelineInfo.renderPass = nullptr;

    if (vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipelineInfo, nullptr,
                                  &pipeline.pipeline)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline");
    }

    vkDestroyShaderModule(context.device, coreVertModule, nullptr);
    vkDestroyShaderModule(context.device, coreFragModule, nullptr);
}

// DRAW CALL
void App::executeDrawCall(NPRendererAovs& aovs)
{
    createRenderingResources(aovs);
    createGraphicsPipeline(pipeline, descriptorSetLayouts, aovs);

    // grab a frame
    NPFrame& frame = context.getCurrentFrame(currentFrame);

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    NPImage* renderTarget = aovs.color;

    populateDrawCall(frame.commandBuffer, renderTarget);

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

void App::populateDrawCall(VkCommandBuffer& commandBuffer, NPImage* renderTarget)
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
    renderingInfo.renderArea.extent = context.swapchainParams.extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    VkViewport viewport{ 0.0f,
                         0.0f,
                         (float)context.swapchainParams.extent.width,
                         (float)context.swapchainParams.extent.height,
                         0.0f,
                         1.0f };

    VkRect2D scissor{ { 0, 0 }, context.swapchainParams.extent };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 2,
                            descriptorSets.data(), 0, nullptr);

    const std::vector<NPMesh>& meshes = scene->getMeshes();
    for (size_t i = 0; i < meshRecords.size(); i++)
    {
        const NPMesh& mesh = meshes[i];
        vkCmdDraw(commandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, i);
    }

    vkCmdEndRendering(commandBuffer);

    context.transitionImageLayout(commandBuffer, renderTarget->image,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
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

    if (meshRecordBuffer.buffer != VK_NULL_HANDLE)
    {
        meshRecordBuffer.destroy(context.allocator);
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

void App::run()
{
    create();
    destroy();
}
