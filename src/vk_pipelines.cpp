#include <vk_pipelines.h>

#include <fstream>

bool vkutil::load_shader_module (const char *filepath, VkDevice _device, VkShaderModule *output)
{
  // open the shader source file, with cursor at the end
  std::ifstream file(filepath, std::ios::ate | std::ios::binary );
  if(!file.is_open())
  {
    fmt::println("could not open shader source file: {}", filepath);
    return false;
  }
  // load file into a buffer
  size_t file_sz = static_cast<size_t>(file.tellg()); // get file size
  std::vector<uint32_t> buff(file_sz / sizeof(uint32_t)); // spir-v expects uint32 buffer
  file.seekg(0); // place cursor at start
  file.read(reinterpret_cast<char*>(buff.data()), file_sz); // read into buffer
  file.close();

  // create shader module
  VkShaderModuleCreateInfo ci = 
  {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = buff.size() * sizeof(uint32_t),
    .pCode = buff.data()
  };
  VkShaderModule shader_module;
  VkResult res = vkCreateShaderModule(_device, &ci, nullptr, &shader_module);
  VK_CHECK(res);
  if (res != VK_SUCCESS)
  {
    return false;
  }
  *output = shader_module;
  return true;
}
