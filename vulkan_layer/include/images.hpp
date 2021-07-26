#pragma once

#include <vulkan/vulkan_core.h>
#include <map>

extern std::map<VkImageView, VkImage> imageViews;
extern std::map<VkImage, VkImageCreateInfo> images;
