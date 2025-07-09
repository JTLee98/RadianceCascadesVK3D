
#pragma once 

#include <vk_initializers.h>

namespace vkutil {

  void transition_img_layout(VkCommandBuffer cmd, VkImage img, VkImageLayout current_layout, VkImageLayout new_layout);

  void copy_image_to_image (VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize);

};