#pragma once

#include "framework.h"
#include "vulkan/vulkan.h"

NP_TRACER_NAMESPACE_BEGIN

enum class eAovType : uint8_t
{
    INVALID,
    RGB,
    DEPTH,
};

struct AovTokens
{
    VkFormat format;
    VkImageUsageFlags imageUsage;
    VkImageAspectFlags imageAspect;
    VkAccessFlags2 writeAccess;
    VkPipelineStageFlags2 writeStage;
    size_t bytesPerPixel;
};

inline constexpr AovTokens kInvalidFormatTokens = {
    VK_FORMAT_UNDEFINED, 0, 0, 0, 0, SIZE_MAX
};  // default value denotes invalid

inline constexpr AovTokens kRGBTokens = { VK_FORMAT_R8G8B8A8_UNORM,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                              | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                              | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                          4 };

inline constexpr AovTokens kDepthTokens = { VK_FORMAT_D32_SFLOAT,
                                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                                | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                            VK_IMAGE_ASPECT_DEPTH_BIT,
                                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                            4 };

inline const AovTokens& getAovTokens(const eAovType aovType)
{
    switch (aovType)
    {
        case eAovType::RGB: return kRGBTokens;
        case eAovType::DEPTH: return kDepthTokens;
        default: return kInvalidFormatTokens;
    }
}

NP_TRACER_NAMESPACE_END
