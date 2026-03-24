#pragma once

#include "vulkan/vulkan.h"
#include <pxr/imaging/hd/types.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace Np  // match casing of other Usd libraries
{
struct FormatTokens
{
    VkFormat vkFormat;
    VkImageUsageFlags usage;
    VkImageAspectFlags aspect;
    VkAccessFlags2 writeAccess;
    VkPipelineStageFlags2 writeStage;
    VkImageLayout attachmentLayout;
    uint64_t bytesPerPixel;
};

inline constexpr FormatTokens kColorFormatTokens = { VK_FORMAT_R8G8B8A8_UNORM,
                                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                         | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                     VK_IMAGE_ASPECT_COLOR_BIT,
                                                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                     4 };

inline constexpr FormatTokens kDepthFormatTokens = { VK_FORMAT_D32_SFLOAT,
                                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                         | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                     VK_IMAGE_ASPECT_DEPTH_BIT,
                                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                     VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                     4 };

inline constexpr FormatTokens kNormalFormatTokens = {
    VK_FORMAT_R32G32B32_SFLOAT,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    12
};

inline constexpr FormatTokens kIdFormatTokens = { VK_FORMAT_R32_SINT,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  4 };

const FormatTokens& GetFormatTokens(HdFormat fmt);
}  // namespace Np

PXR_NAMESPACE_CLOSE_SCOPE
