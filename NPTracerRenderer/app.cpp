#include "app.h"

void App::create()
{
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

    createRenderingResources();
}

void App::createRenderingResources()
{
    // create descriptor layotuts
    NPDescriptorSetLayout descriptorSetLayout;
    std::vector<VkDescriptorSetLayoutBinding> bindings{
        { { .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .pImmutableSamplers = nullptr },
          { .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr } }
    };

    std::vector<VkDescriptorPoolSize> poolSizes{
        { { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = FRAME_COUNT },
          { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = FRAME_COUNT } }
    };
    context.createDescriptorSetLayout(descriptorSetLayout, bindings, poolSizes);
    descriptorSetLayouts.emplace_back(descriptorSetLayout);

    createGraphicsPipeline(pipeline, descriptorSetLayouts);

    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    context.createDeviceLocalBuffer(vertexBuffer, (void*)vertices.data(), vertexBufferSize,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
    context.createDeviceLocalBuffer(indexBuffer, (void*)indices.data(), indexBufferSize,
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    context.createTextureImage(textureImage);
    context.createTextureSampler(textureSampler);

    createDescriptorSets();
}

void App::createGraphicsPipeline(NPPipeline& pipeline,
                                 std::vector<NPDescriptorSetLayout>& descriptorSetLayouts)
{
    // shader creation
    VkShaderModule coreVertModule = context.createShaderModule(readFile(NPTRACER_SHADER_CORE_VERT));
    VkShaderModule coreFragModule = context.createShaderModule(readFile(NPTRACER_SHADER_CORE_FRAG));

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

    VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    VkPipelineInputAssemblyStateCreateInfo inputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkViewport viewport{ 0.0f,
                         0.0f,
                         static_cast<float>(context.swapchainParams.extent.width),
                         static_cast<float>(context.swapchainParams.extent.height),
                         0.0f,
                         1.0f };
    VkRect2D rect{ VkOffset2D{ 0, 0 }, context.swapchainParams.extent };
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
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // pipeline layout
    std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    vkDescriptorSetLayouts.reserve(descriptorSetLayouts.size());
    for (auto& descriptorSetLayout : descriptorSetLayouts)
    {
        vkDescriptorSetLayouts.push_back(descriptorSetLayout.layout);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = vkDescriptorSetLayouts.data(),
        .pushConstantRangeCount = 0
    };
    vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &pipeline.layout);

    VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context.swapchainParams.format.format,
        .depthAttachmentFormat = context.swapchainParams.depthFormat
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
        .layout = pipeline.layout,
        .renderPass = nullptr
    };

    if (vkCreateGraphicsPipelines(context.device, nullptr, 1, &pipelineInfo, nullptr, &pipeline.pipeline)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline");
    }

    vkDestroyShaderModule(context.device, coreVertModule, nullptr);
    vkDestroyShaderModule(context.device, coreFragModule, nullptr);
}

void App::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(FRAME_COUNT, descriptorSetLayouts[0].layout);
    VkDescriptorSetAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = descriptorSetLayouts[0].pool,
                                           .descriptorSetCount = static_cast<uint32_t>(
                                               layouts.size()),
                                           .pSetLayouts = layouts.data() };

    std::vector<VkDescriptorSet> sets(FRAME_COUNT);
    vkAllocateDescriptorSets(context.device, &allocInfo, sets.data());

    std::vector<VkDescriptorBufferInfo> bufferInfos(FRAME_COUNT);
    std::vector<VkDescriptorImageInfo> imageInfos(FRAME_COUNT);
    std::vector<VkWriteDescriptorSet> descriptorWrites(FRAME_COUNT * 2);
    for (uint32_t i = 0; i < FRAME_COUNT; i++)
    {
        Frame& frame = context.getCurrentFrame(i);

        frame.descriptorSet = sets[i];

        bufferInfos[i] = { .buffer = frame.uboBuffer.buffer,
                           .offset = 0,
                           .range = sizeof(UniformBufferObject) };

        imageInfos[i] = { .sampler = textureSampler,
                          .imageView = textureImage.view,
                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        descriptorWrites[2 * i + 0] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                        .dstSet = frame.descriptorSet,
                                        .dstBinding = 0,
                                        .dstArrayElement = 0,
                                        .descriptorCount = 1,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        .pBufferInfo = &bufferInfos[i] };

        descriptorWrites[2 * i + 1] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                        .dstSet = frame.descriptorSet,
                                        .dstBinding = 1,
                                        .dstArrayElement = 0,
                                        .descriptorCount = 1,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .pImageInfo = &imageInfos[i] };
    }

    vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
}

// -----------------------------------------------------------------------------
// DRAW CALL
// -----------------------------------------------------------------------------

void App::executeDrawCall(GLFWwindow* window)
{
    // grab a frame
    Frame& frame = context.getCurrentFrame(currentFrame);

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    // acquire image when it is done being presented
    uint32_t imageIndex;
    if (vkAcquireNextImageKHR(context.device, context.swapchain, UINT64_MAX,
                              frame.donePresentingSemaphore,
                              nullptr,
                              &imageIndex)
        == VK_ERROR_OUT_OF_DATE_KHR)
    {
        context.recreateSwapchain(window);
        return;
    }

    populateDrawCall(frame.commandBuffer,
                     imageIndex);  // record commands into frame's command buffer
    vkResetFences(context.device, 1,
                  &frame.doneExecutingFence);  // signal that fence is ready to be
                                                          // associated with a new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores
        = &frame.donePresentingSemaphore,  // wait to submit work until image is done being presented
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &context.doneRenderingSemaphores[imageIndex]
    };  // signal that rendering is finished once execution is finished

    vkQueueSubmit(context.queues[QueueFamily::GRAPHICS].queue, 1, &submitInfo,
                  frame.doneExecutingFence);  // signal that execution has been completed on frame
                                              // once it is done

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores
        = &context.doneRenderingSemaphores[imageIndex],  // submit for presentation once rendering is finished
        .swapchainCount = 1,
        .pSwapchains = &context.swapchain,
        .pImageIndices = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(context.queues[QueueFamily::GRAPHICS].queue, &presentInfo);
    if ((result == VK_SUBOPTIMAL_KHR) || (result == VK_ERROR_OUT_OF_DATE_KHR) || context.framebufferResized)
    {
        context.framebufferResized = false;
        context.recreateSwapchain(window);
    }

    // increment frame (within ring)
    currentFrame = (currentFrame + 1) % FRAME_COUNT;
}

void App::populateDrawCall(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    vkResetCommandBuffer(commandBuffer, 0);
    context.beginCommandBuffer(commandBuffer);

    context.transitionImageLayout(commandBuffer, context.swapchainImages[imageIndex].image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearColor{ .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkClearValue clearDepth{ .depthStencil = { 1.0f, 0 } };

    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = context.swapchainImages[imageIndex].view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor
    };

    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = context.depthImage.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = clearDepth
    };

    VkRenderingInfo renderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                   .renderArea = { .offset = { 0, 0 },
                                                   .extent = context.swapchainParams.extent },
                                   .layerCount = 1,
                                   .colorAttachmentCount = 1,
                                   .pColorAttachments = &colorAttachmentInfo,
                                   .pDepthAttachment = &depthAttachmentInfo };

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

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1,
                            &context.getCurrentFrame(currentFrame).descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, indices.size(), 1, 0, 0, 0);

    vkCmdEndRendering(commandBuffer);

    context.transitionImageLayout(commandBuffer, context.swapchainImages[imageIndex].image,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);
}

void App::updateUniformBuffer()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime)
                     .count();

    UniformBufferObject ubo{
        .model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        .view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                       glm::vec3(0.0f, 0.0f, 1.0f)),
        .proj = glm::perspective(glm::radians(45.0f),
                                 static_cast<float>(context.swapchainParams.extent.width)
                                     / static_cast<float>(context.swapchainParams.extent.height),
                                 0.1f, 10.0f)
    };
    ubo.proj[1][1] *= -1;

    memcpy(context.getCurrentFrame(currentFrame).uboBuffer.allocInfo.pMappedData, &ubo,
           sizeof(UniformBufferObject));
}

void App::render()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        executeDrawCall(window);
        updateUniformBuffer();
    }

    context.waitIdle();
}

void App::run()
{
    create();
    render();
    destroy();
}

void App::destroy()
{
    textureImage.destroy(context.device, context.allocator);
    vkDestroySampler(context.device, textureSampler, nullptr);

    for (auto& descriptorSetLayout : descriptorSetLayouts)
    {
        descriptorSetLayout.destroy(context.device);
    }

    pipeline.destroy(context.device);

    vertexBuffer.destroy(context.allocator);
    indexBuffer.destroy(context.allocator);

    context.destroy();

    if (window)
    {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}
