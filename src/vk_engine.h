// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_images.h>

constexpr unsigned int FRAME_OVERLAP = 2;

struct FrameData
{
	VkCommandPool _cmdpool;
	VkCommandBuffer _mainCmdBuf;
	VkSemaphore _swapchainSemaphore; // for getting image from swapchain
	VkSemaphore _renderSemaphore; // for presenting swapchain image to OS
	VkFence _renderFence; // for recording command buffers of next frame - signalled when cmd buffer is finished drawing
};

constexpr VkExtent2D DEFAULT_RES = {.width = 1280, .height = 720};

class VulkanEngine {
public:
	#ifdef _DEBUG
	VkDebugUtilsMessengerEXT _debug_messenger;
	#endif
	VkInstance _instance;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	
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
	
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

private:
	void init_vulkan();
	void init_swapchain();
	void resize_surface();
	void init_commands();
	void init_sync_structures();

public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent = DEFAULT_RES;

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();
};
