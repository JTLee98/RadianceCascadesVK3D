#include <vk_images.h>

void vkutil::transition_img_layout(VkCommandBuffer cmd, VkImage img, VkImageLayout current_layout, VkImageLayout new_layout)
{
  // only choose depth aspect if needed
  VkImageAspectFlags aspectMask 
    = (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) 
      ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  
  VkImageMemoryBarrier2 imageBarrier =
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .pNext = nullptr,
    
    .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
    
    .oldLayout = current_layout,
    .newLayout = new_layout,
    .image = img,
    .subresourceRange = vkinit::image_subresource_range(aspectMask)
  };

  VkDependencyInfo depInfo = 
  {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .pNext = nullptr,

    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &imageBarrier
  };

  vkCmdPipelineBarrier2(cmd, &depInfo);
}