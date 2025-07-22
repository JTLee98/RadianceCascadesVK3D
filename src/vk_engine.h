// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_images.h>
#include <vk_descriptors.h>
#include <vk_pipelines.h>

struct SDL_Window;

namespace VkEngine
{

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) 
	{
		deletors.push_back(function);
	}

	void flush() 
	{
		// reverse iterate the deletion queue to call all the functions
		for (auto it = deletors.crbegin(); it != deletors.crend(); it++) 
		{
			(*it)();
		}

		deletors.clear();
	}
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct FrameData
{
	VkCommandPool _cmdpool;
	VkCommandBuffer _mainCmdBuf;
	VkSemaphore _swapchainSemaphore; // for getting image from swapchain
	VkSemaphore _renderSemaphore; // for presenting swapchain image to OS
	VkFence _renderFence; // for recording command buffers of next frame - signalled when cmd buffer is finished drawing
	DeletionQueue _deletionQueue;
};

constexpr VkExtent2D DEFAULT_RES = {.width = 1280, .height = 720};

class Engine {
public:
	VmaAllocator _allocator;

	#ifdef _DEBUG
	VkDebugUtilsMessengerEXT _debug_messenger;
	#endif
	VkInstance _instance;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;

	// immediate submit 
	VkFence _imm_fence;
	VkCommandPool _imm_cmdpool;
	VkCommandBuffer _imm_cmdbuf;
	void imm_submit(std::function<void(VkCommandBuffer cmd)>&& func);

	// swapchain and draw surface
	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	std::vector<VkSurfaceFormatKHR> _swapchainImgFmts =
	{
		VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_UNORM, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR}
	};
	std::vector<VkPresentModeKHR> _swapchainpresentModes = {VK_PRESENT_MODE_FIFO_KHR};
	std::vector<VkImage> _swapchainImgs;
	std::vector<VkImageView> _swapchainImgViews;
	VkExtent2D _swapchainExtent = DEFAULT_RES;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() 
		{ return _frames[_frameNumber % FRAME_OVERLAP]; }
	
	// draw resources
	vkutil::AllocatedImg _drawImage;
	VkExtent2D _drawExtent;

	// descriptor sets
	vkutil::DescriptorAllocator _globalDescriptorAllocator;
	
	VkDescriptorSet _drawImgDescriptors;
	VkDescriptorSetLayout _drawImgDescriptorsLayout;

	// pipelines
	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;
	
	// graphics queue
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	// deletion queue
	DeletionQueue _mainDeletionQueue;

private:
	void init_vulkan();
	void init_swapchain();
	void resize_surface();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_background_pipelines();

public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent = DEFAULT_RES;
  VkClearColorValue _clearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	::SDL_Window* _window{ nullptr };

	static Engine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();
};

}
