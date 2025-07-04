
#pragma once 

#include <vk_initializers.h>

namespace vkutil {

  void transition_img_layout(VkCommandBuffer cmd, VkImage img, VkImageLayout current_layout, VkImageLayout new_layout);

};