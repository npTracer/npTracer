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
    const size_t meshCount = scene->getMeshCount();
    std::vector<NPMeshRecord> meshRecords;
    meshRecords.reserve(meshCount);
    for (int i = 0; i < meshCount; i++)
    {
        NPMesh const* mesh = scene->getMeshAtIndex(i);
    
        NPBuffer vertexBuffer;
        NPBuffer indexBuffer;
    
        const std::vector<NPVertex>& vertices = mesh->vertices;
        VkDeviceSize vbSize = sizeof(vertices[0]) * vertices.size();
        bool vbCreated = context.createDeviceLocalBuffer(vertexBuffer, const_cast<NPVertex*>(vertices.data()), vbSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        vertexBuffers.push_back(vertexBuffer);
    
        if (!vbCreated)
        {
            throw std::runtime_error("failed to create vertex buffer");
        }
        
        VkDeviceSize ibSize = sizeof(mesh->indices[0]) * mesh->indices.size();
        bool ibCreated = context.createDeviceLocalBuffer(indexBuffer, (void*)mesh->indices.data(), ibSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        indexBuffers.push_back(indexBuffer);
    
        if (!ibCreated)
        {
            throw std::runtime_error("failed to create vertex buffer");
        }
        
        // temp
        indexCounts.push_back(static_cast<uint32_t>(mesh->indices.size()));
        
        NPMeshRecord meshRecord{};
        meshRecord.vbAddress = context.getBufferDeviceAddress(vertexBuffer);
        meshRecord.ibAddress = context.getBufferDeviceAddress(indexBuffer);
        meshRecord.indexCount = static_cast<uint32_t>(mesh->indices.size());
        meshRecord.pad0 = 0;
        
        meshRecords.push_back(meshRecord);
    }

    VkDeviceSize meshRecordSize = sizeof(meshRecords[0]) * meshRecords.size();
    bool meshRecordBufferCreated = context.createDeviceLocalBuffer(meshRecordBuffer, meshRecords.data(), meshRecordSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    
    if (!meshRecordBufferCreated)
    {
        throw std::runtime_error("failed to create mesh record buffer");
    }
    
    VkDeviceSize cameraSize = sizeof(NPCameraRecord);
    bool cameraRecordBufferCreated = context.createDeviceLocalBuffer(cameraRecordBuffer, scene->getCamera(), cameraSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    
    if (!cameraRecordBufferCreated)
    {
        throw std::runtime_error("failed to create camera record buffer");
    }
    
    // SET 0: Mesh Records
    {
        NPDescriptorSetLayout meshDescriptorSetLayout{};

    VkDescriptorSetLayoutBinding binding0{};
    binding0.binding = 0;
    binding0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding0.descriptorCount = 1;
    binding0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding0.pImmutableSamplers = nullptr;
        
    VkDescriptorSetLayoutCreateInfo meshLayoutInfo{};
    meshLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    meshLayoutInfo.bindingCount = 1;
    meshLayoutInfo.pBindings = &binding0;
    
    if (vkCreateDescriptorSetLayout(
        context.device,
        &meshLayoutInfo,
        nullptr,
        &meshDescriptorSetLayout.layout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create mesh descriptor set layout");
    }

    VkDescriptorPoolSize meshPoolSize{};
    meshPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    meshPoolSize.descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo meshPoolInfo{};
    meshPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    meshPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    meshPoolInfo.maxSets = 1;
    meshPoolInfo.poolSizeCount = 1;
    meshPoolInfo.pPoolSizes = &meshPoolSize;
    
    if (vkCreateDescriptorPool(
        context.device,
        &meshPoolInfo,
        nullptr,
        &meshDescriptorSetLayout.pool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create mesh descriptor pool");
    }
    
    //descriptorSetLayouts.push_back(meshDescriptorSetLayout);
    
    VkDescriptorSet meshDescriptorSet{};
    VkDescriptorSetAllocateInfo meshAllocInfo{};
    meshAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    meshAllocInfo.descriptorPool = meshDescriptorSetLayout.pool;
    meshAllocInfo.descriptorSetCount = 1;
    meshAllocInfo.pSetLayouts = &meshDescriptorSetLayout.layout;
    
    if (vkAllocateDescriptorSets(
        context.device,
        &meshAllocInfo,
        &meshDescriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate mesh descriptor set");
    }
    
    VkDescriptorBufferInfo meshRecordInfo{};
    meshRecordInfo.buffer = meshRecordBuffer.buffer;
    meshRecordInfo.offset = 0;
    meshRecordInfo.range = VK_WHOLE_SIZE;
    
    VkWriteDescriptorSet write0{};
    write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write0.dstSet = meshDescriptorSet;
    write0.dstBinding = 0;
    write0.dstArrayElement = 0;
    write0.descriptorCount = 1;
    write0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write0.pBufferInfo = &meshRecordInfo;

    vkUpdateDescriptorSets(context.device, 1, &write0, 0, nullptr);

    //descriptorSets.push_back(meshDescriptorSet);
    }
    
    
    // SET 1: Camera
    {
        NPDescriptorSetLayout descriptorSetLayout{};

        VkDescriptorSetLayoutBinding binding0{};
        binding0.binding = 0;
        binding0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding0.descriptorCount = 1;
        binding0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binding0.pImmutableSamplers = nullptr;
            
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding0;
        
        if (vkCreateDescriptorSetLayout(
            context.device,
            &layoutInfo,
            nullptr,
            &descriptorSetLayout.layout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create mesh descriptor set layout");
        }

        VkDescriptorPoolSize meshPoolSize{};
        meshPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        meshPoolSize.descriptorCount = 1;
        
        VkDescriptorPoolCreateInfo meshPoolInfo{};
        meshPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        meshPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        meshPoolInfo.maxSets = 1;
        meshPoolInfo.poolSizeCount = 1;
        meshPoolInfo.pPoolSizes = &meshPoolSize;
        
        if (vkCreateDescriptorPool(
            context.device,
            &meshPoolInfo,
            nullptr,
            &descriptorSetLayout.pool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create mesh descriptor pool");
        }
        
        descriptorSetLayouts.push_back(descriptorSetLayout);
        
        VkDescriptorSet descriptorSet{};
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorSetLayout.pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout.layout;
        
        if (vkAllocateDescriptorSets(
            context.device,
            &allocInfo,
            &descriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate mesh descriptor set");
        }
        
        VkDescriptorBufferInfo recordInfo{};
        recordInfo.buffer = cameraRecordBuffer.buffer;
        recordInfo.offset = 0;
        recordInfo.range = VK_WHOLE_SIZE;
        
        VkWriteDescriptorSet write0{};
        write0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write0.dstSet = descriptorSet;
        write0.dstBinding = 0;
        write0.dstArrayElement = 0;
        write0.descriptorCount = 1;
        write0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write0.pBufferInfo = &recordInfo;

        vkUpdateDescriptorSets(context.device, 1, &write0, 0, nullptr);

        descriptorSets.push_back(descriptorSet);
    }
    
    createGraphicsPipeline();
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
    
    VkViewport viewport{
        0.0f, 0.0f, 0, 0,
        0.0f, 1.0f
    };
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
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipeline.layout);
    
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = m_aovs ? &m_aovs->color->format : &context.swapchainParams.format.format;
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

    vkCmdBindDescriptorSets(
    commandBuffer,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipeline.layout,
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

    VkDeviceSize offset = 0;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, static_cast<uint32_t>(descriptorSets.size()),
                            descriptorSets.data(), 0, nullptr);
    
    // for (size_t i = 0; i < indexCounts.size(); i++)
    // {
    //     vkCmdDraw(commandBuffer, indexCounts[i], 1, 0, static_cast<uint32_t>(i));
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
