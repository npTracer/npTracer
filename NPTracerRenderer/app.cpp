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
}

// -----------------------------------------------------------------------------
// RESOURCE CREATION
// -----------------------------------------------------------------------------

void App::createRenderingResources(RendererPayload& payload, VkRendererAovs& aovs)
{
    this->payload = payload;
    uint32_t vbCount = 0;
    uint32_t ibCount = 0;

    for (const auto& mesh : payload.meshes)
    {
        MeshRecord meshRecord{ .vbIdx = vbCount++, .ibIdx = ibCount++ };

        NPBuffer vertexBuffer;
        NPBuffer indexBuffer;

        std::vector<Vertex> vertices = mesh.getVertices();
        VkDeviceSize vbSize = sizeof(vertices[0]) * vertices.size();
        context.createDeviceLocalBuffer(vertexBuffer, vertices.data(), vbSize,
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
    VkDeviceSize cameraSize = sizeof(CameraRecord);
    CameraRecord cameraRecord = {
        .model = glm::mat4(1.0f),
        .view = lookAt(payload.cam.cameraPos, payload.cam.cameraPos + payload.cam.cameraForward,
                       payload.cam.cameraUp),
        .proj = glm::perspective(payload.cam.fov, payload.cam.aspect, 0.1f, 10.0f)
    };

    context.createDeviceLocalBuffer(cameraRecordBuffer, &cameraRecord, cameraSize,
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    // CREATE MESH DESCRIPTOR SET LAYOUT

    NPDescriptorSetLayout meshDescriptorSetLayout;
    std::vector<VkDescriptorSetLayoutBinding> meshBindings{ {
        { .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,  // one large mesh record buffer
          .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
          .pImmutableSamplers = nullptr },
        { .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = MAX_RESOURCE_COUNT,
          .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
          .pImmutableSamplers = nullptr },
        { .binding = 2,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = MAX_RESOURCE_COUNT,
          .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
          .pImmutableSamplers = nullptr },
    } };

    std::vector<VkDescriptorBindingFlags> meshBindingFlags{
        0, 0,
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
            | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo meshFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(meshBindings.size()),
        .pBindingFlags = meshBindingFlags.data()
    };

    VkDescriptorSetLayoutCreateInfo meshLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &meshFlagsInfo,
        .bindingCount = static_cast<uint32_t>(meshBindings.size()),
        .pBindings = meshBindings.data(),
    };
    vkCreateDescriptorSetLayout(context.device, &meshLayoutInfo, nullptr,
                                &meshDescriptorSetLayout.layout);

    std::vector<VkDescriptorPoolSize> meshPoolSizes{ {
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2 * MAX_RESOURCE_COUNT + 1 },
    } };

    VkDescriptorPoolCreateInfo meshPoolInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                         .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                         .maxSets = 1,
                                         .poolSizeCount = static_cast<uint32_t>(meshPoolSizes.size()),
                                         .pPoolSizes = meshPoolSizes.data() };

    vkCreateDescriptorPool(context.device, &meshPoolInfo, nullptr, &meshDescriptorSetLayout.pool);
    descriptorSetLayouts.emplace_back(meshDescriptorSetLayout);

    // CREATE CAMERA DESCRIPTOR SET LAYOUT

    NPDescriptorSetLayout cameraDescriptorSetLayout;
    std::vector<VkDescriptorSetLayoutBinding> cameraBindings{ {
        { .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,  // one camera uniform
          .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
          .pImmutableSamplers = nullptr }
    } };

    VkDescriptorSetLayoutCreateInfo cameraLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(cameraBindings.size()),
        .pBindings = cameraBindings.data(),
    };
    vkCreateDescriptorSetLayout(context.device, &cameraLayoutInfo, nullptr,
                                &cameraDescriptorSetLayout.layout);

    std::vector<VkDescriptorPoolSize> cameraPoolSizes{ {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 },
    } };

    VkDescriptorPoolCreateInfo cameraPoolInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                         .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                         .maxSets = 1,
                                         .poolSizeCount = static_cast<uint32_t>(cameraPoolSizes.size()),
                                         .pPoolSizes = cameraPoolSizes.data() };

    vkCreateDescriptorPool(context.device, &cameraPoolInfo, nullptr, &cameraDescriptorSetLayout.pool);
    descriptorSetLayouts.emplace_back(cameraDescriptorSetLayout);

    // DESCRIPTOR SET CREATION
    VkDescriptorSet meshDescriptorSet;
    VkDescriptorSet cameraDescriptorSet;
    VkDescriptorSetAllocateInfo meshAllocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = meshDescriptorSetLayout.pool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &meshDescriptorSetLayout.layout };

    VkDescriptorSetAllocateInfo cameraAllocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = cameraDescriptorSetLayout.pool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &cameraDescriptorSetLayout.layout };

    vkAllocateDescriptorSets(context.device, &meshAllocInfo, &meshDescriptorSet);
    vkAllocateDescriptorSets(context.device, &cameraAllocInfo, &cameraDescriptorSet);

    // binding 0: mesh records
    VkDescriptorBufferInfo meshRecordInfo{ .buffer = meshRecordBuffer.buffer,
                                           .offset = 0,
                                           .range = VK_WHOLE_SIZE };

    // binding 1: vertex buffers
    std::vector<VkDescriptorBufferInfo> vertexBufferInfos(vertexBuffers.size());
    for (size_t i = 0; i < vertexBuffers.size(); i++)
    {
        vertexBufferInfos[i] = { .buffer = vertexBuffers[i].buffer,
                                 .offset = 0,
                                 .range = VK_WHOLE_SIZE };
    }

    // binding 2: index buffers
    std::vector<VkDescriptorBufferInfo> indexBufferInfos(indexBuffers.size());
    for (size_t i = 0; i < indexBuffers.size(); i++)
    {
        indexBufferInfos[i] = { .buffer = indexBuffers[i].buffer,
                                .offset = 0,
                                .range = VK_WHOLE_SIZE };
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites{
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = meshDescriptorSet,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = &meshRecordInfo },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = meshDescriptorSet,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = static_cast<uint32_t>(vertexBufferInfos.size()),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = vertexBufferInfos.data() },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = meshDescriptorSet,
          .dstBinding = 2,
          .dstArrayElement = 0,
          .descriptorCount = static_cast<uint32_t>(indexBufferInfos.size()),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = indexBufferInfos.data() },
    };

    vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);

    // camera
    VkDescriptorBufferInfo cameraRecordInfo{ .buffer = cameraRecordBuffer.buffer,
                                             .offset = 0,
                                             .range = VK_WHOLE_SIZE };

    VkWriteDescriptorSet cameraWrite{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      .dstSet = cameraDescriptorSet,
                                      .dstBinding = 0,
                                      .dstArrayElement = 0,
                                      .descriptorCount = 1,
                                      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                      .pBufferInfo = &cameraRecordInfo };

    vkUpdateDescriptorSets(context.device, 1, &cameraWrite, 0, nullptr);

    // store
    descriptorSets.push_back(meshDescriptorSet);
    descriptorSets.push_back(cameraDescriptorSet);
}

void App::createGraphicsPipeline(NPPipeline& pipeline,
                                 std::vector<NPDescriptorSetLayout>& descriptorSetLayouts,
                                 VkRendererAovs& aovs)
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

    VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(aovs.color.width), static_cast<float>(aovs.color.height),
        0.0f, 1.0f
    };

    VkExtent2D extent{ .width = aovs.color.width, .height = aovs.color.height };
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
        .pColorAttachmentFormats = &aovs.color.format,
        .depthAttachmentFormat = context.swapchainParams.depthFormat // replace with aov depth format?
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

// -----------------------------------------------------------------------------
// DRAW CALL
// -----------------------------------------------------------------------------

void App::executeDrawCall(RendererPayload& payload, VkRendererAovs& aovs)
{   
    createRenderingResources(payload, aovs);
    createGraphicsPipeline(pipeline, descriptorSetLayouts, aovs);

    // grab a frame
    Frame& frame = context.getCurrentFrame(currentFrame);

    // wait until this frame has finished executing its commands
    vkWaitForFences(context.device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    Image renderTarget{ .image = aovs.color.image, .view = aovs.color.view };
    populateDrawCall(frame.commandBuffer, renderTarget);

    vkResetFences(context.device, 1, &frame.doneExecutingFence);  // signal that fence is ready to be associated with a new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.commandBuffer,
    };  // signal that rendering is finished once execution is finished

    vkQueueSubmit(context.queues[QueueFamily::GRAPHICS].queue, 1, &submitInfo,
                  frame.doneExecutingFence);  // signal that execution has been completed on frame
                                              // once it is done
    
    // increment frame (within ring)
    currentFrame = (currentFrame + 1) % FRAME_COUNT;
}

void App::populateDrawCall(VkCommandBuffer& commandBuffer, Image& renderTarget)
{
    vkResetCommandBuffer(commandBuffer, 0);
    context.beginCommandBuffer(commandBuffer);

    context.transitionImageLayout(commandBuffer, renderTarget.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearColor{ .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkClearValue clearDepth{ .depthStencil = { 1.0f, 0 } };

    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = renderTarget.view,
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

    for (size_t i = 0; i < meshRecords.size(); i++)
    {
        const auto& mesh = payload.meshes[i];
        vkCmdDraw(commandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, i);
    }

    vkCmdEndRendering(commandBuffer);

    context.transitionImageLayout(commandBuffer, renderTarget.image,
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
