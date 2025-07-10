#pragma once

#include <vk_types.h>
namespace vkutil
{

struct DescriptorLayoutBuilder {

  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void add_binding(uint32_t _binding, VkDescriptorType _type);
  void clear();
  VkDescriptorSetLayout build(VkDevice _device, VkShaderStageFlags _shaderStages, void* _pNext = nullptr, VkDescriptorSetLayoutCreateFlags _flags = 0);
};

struct DescriptorAllocator 
{
  struct PoolSizeRatio
  {
    VkDescriptorType type;
    float ratio;
  };

  VkDescriptorPool pool;

  void init_pool(VkDevice _device, uint32_t _maxSets, std::span<PoolSizeRatio> _poolRatios);
  void clear_descriptors(VkDevice _device);
  void destroy_pool(VkDevice _device);

  VkDescriptorSet allocate(VkDevice _device, VkDescriptorSetLayout _layout);
};

}