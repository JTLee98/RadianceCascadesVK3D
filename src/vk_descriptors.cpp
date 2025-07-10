#include <vk_descriptors.h>

using namespace vkutil;

// DescriptorLayoutBuilder definitions

void DescriptorLayoutBuilder::add_binding(uint32_t _binding, VkDescriptorType _type)
{
  VkDescriptorSetLayoutBinding new_binding = 
  {
    .binding = _binding,
    .descriptorType = _type,
    .descriptorCount = 1
  };
  bindings.push_back(new_binding);
}

void DescriptorLayoutBuilder::clear()
{
  bindings.clear();
}

VkDescriptorSetLayout
DescriptorLayoutBuilder::build (VkDevice _device,
                                VkShaderStageFlags _shaderStages, 
                                void *_pNext,
                                VkDescriptorSetLayoutCreateFlags _flags)
{
  for(auto& b : bindings)
  {
    b.stageFlags |= _shaderStages;
  }

  VkDescriptorSetLayoutCreateInfo ci = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = _pNext,
    .flags = _flags,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data(),
  };

  VkDescriptorSetLayout set;
  VK_CHECK(vkCreateDescriptorSetLayout(_device, &ci, nullptr, &set));
  return set;
}

// DescriptorAllocator definitons

void DescriptorAllocator::init_pool(VkDevice _device, 
                                    uint32_t _maxSets,
                                    std::span<PoolSizeRatio> _poolRatios)
{
  std::vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(_poolRatios.size());
  for(const auto& _ratio : _poolRatios)
  {
    pool_sizes.emplace_back(VkDescriptorPoolSize
      {
        .type = _ratio.type,
        .descriptorCount = static_cast<uint32_t>(_ratio.ratio * _maxSets)
      });
  }

  VkDescriptorPoolCreateInfo ci = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .flags = 0,
    .maxSets = _maxSets,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data()
  };

  VK_CHECK(vkCreateDescriptorPool(_device, &ci, nullptr, &pool));
}

void DescriptorAllocator::clear_descriptors(VkDevice _device)
{
  vkResetDescriptorPool(_device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice _device)
{
  vkDestroyDescriptorPool(_device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice _device, VkDescriptorSetLayout _layout)
{
  VkDescriptorSetAllocateInfo alloc_info = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &_layout
  };

  VkDescriptorSet descriptor_set;
  VK_CHECK(vkAllocateDescriptorSets(_device, &alloc_info, &descriptor_set));

  return descriptor_set;
}
