//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <chrono>
#include <thread>

using namespace VkEngine;

Engine* loadedEngine = nullptr;

Engine& Engine::Get() { return *loadedEngine; }
void Engine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);
    
    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();

    // everything went fine
    _isInitialized = true;
    #ifdef _DEBUG
      fmt::println("[ENGINE] successfully initialized");
      std::fflush(stdout);
    #endif
}

void Engine::cleanup()
{
    if (_isInitialized) {
        // wait for gpu to stop
        vkDeviceWaitIdle(_device);

        // destroy command pools
        // command buffers are destroyed when their parent pool is destroyed
        for(auto& f : _frames)
        {
            vkDestroyCommandPool(_device, f._cmdpool, nullptr);
            // destroy sync semaphores and fences
            vkDestroyFence(_device, f._renderFence, nullptr);
            vkDestroySemaphore(_device, f._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, f._swapchainSemaphore, nullptr);
            // destroy per-frame objects
            f._deletionQueue.flush();
        }

        // flush global deletion queue
        _mainDeletionQueue.flush();

        // destroy swapchain & image views
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
        for(auto& iv : _swapchainImgViews)
        {
            vkDestroyImageView(_device, iv, nullptr);
        }

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);
        #ifdef _DEBUG
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        #endif
        vkDestroyInstance(_instance, nullptr);
        
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;

    #ifdef _DEBUG
        fmt::println("[ENGINE] cleanup successful");
        std::fflush(stdout);
    #endif
}

void Engine::draw()
{
  #pragma region initializeFrame
  // get current frame data
  FrameData& current_frame = get_current_frame();
  // wait until gpu finishes rendering previous frame (1 second timeout)
  VK_CHECK(vkWaitForFences(_device, 1, &current_frame._renderFence, true, 1000000000));
  // VK_CHECK(vkGetFenceStatus(_device, current_frame._renderFence));
  // flush objects created for previous frame
  current_frame._deletionQueue.flush();
  // clear fence to begin current frame
  VK_CHECK(vkResetFences(_device, 1, &current_frame._renderFence));
  
  // get image from swapchain to render to
  uint32_t swapchain_img_idx;
  VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, current_frame._swapchainSemaphore, nullptr, &swapchain_img_idx));
  VkImage& current_swapchain_img = _swapchainImgs[swapchain_img_idx];
  // set draw extents
  _drawExtent = _swapchainExtent;
  
  // get current command buffer
  VkCommandBuffer& cmdbuf = current_frame._mainCmdBuf;
  // reset command buffer to ready it for recording commands of this frame
  VK_CHECK(vkResetCommandBuffer(cmdbuf, 0));
  // begin recording commands
  VkCommandBufferBeginInfo cmdBegin_info = 
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, // this cmd buffer will only be submitted and executed once
    .pInheritanceInfo = nullptr
  };
  VK_CHECK(vkBeginCommandBuffer(cmdbuf, &cmdBegin_info));
  // set draw image layout to writeable
  // (since it will be written over, previous layout is irrelevant)
  vkutil::transition_img_layout(cmdbuf, _drawImage.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  #pragma endregion

  #pragma region RenderCommands
  // clear background
  VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  vkCmdClearColorImage(cmdbuf, _drawImage.img, VK_IMAGE_LAYOUT_GENERAL, &_clearValue, 1, &clearRange);

  // --- submit other rendering commands here --- //
  #pragma endregion

  #pragma region present
  // copy the rendered draw image to swapchain
  vkutil::transition_img_layout(cmdbuf, _drawImage.img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_img_layout(cmdbuf, current_swapchain_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  vkutil::copy_image_to_image(cmdbuf, _drawImage.img, current_swapchain_img, _drawExtent, _swapchainExtent);
  // make swapchain image presentable
  vkutil::transition_img_layout(cmdbuf, current_swapchain_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  // finalize command buffer
  VK_CHECK(vkEndCommandBuffer(cmdbuf));
  // prepare submission to queue
  // wait on _presentSemaphore
  VkCommandBufferSubmitInfo cmdbuf_submit_info = vkinit::command_buffer_submit_info(cmdbuf);
  VkSemaphoreSubmitInfo wait_info = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, current_frame._swapchainSemaphore);
  VkSemaphoreSubmitInfo signal_info = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, current_frame._renderSemaphore);
  VkSubmitInfo2 submit_info = vkinit::submit_info(&cmdbuf_submit_info, &signal_info, &wait_info);
  // submit command buffer to queue and execute it
  // _renderFence will now block until execution is finished
  VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit_info, current_frame._renderFence));
  
  // prepare present
  VkPresentInfoKHR present_info = 
  {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    
    // wait on _renderSemaphore to signal that draw commands have completed
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &current_frame._renderSemaphore,

    .swapchainCount = 1,
    .pSwapchains = &_swapchain,
    .pImageIndices = &swapchain_img_idx
  };
  // present
  VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &present_info));
  // increase frame count
  _frameNumber++;
  
  #pragma endregion
}

void Engine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // resize window if necessary
        resize_surface();

        draw();
    }
}

// initialize vkInstance, vkPhysicalDevice, vkDevice
void Engine::init_vulkan()
{
    #pragma region instance
    vkb::InstanceBuilder inst_builder;
    inst_builder
        #ifdef _DEBUG
        .request_validation_layers(true)
        .use_default_debug_messenger()
        #endif
    .set_app_name("Vulkan Application")
    .require_api_version(1,3,0);
    // build instance and get handles
    vkb::Instance vkb_inst = inst_builder.build().value();
    _instance = vkb_inst.instance;
        #ifdef _DEBUG
        _debug_messenger = vkb_inst.debug_messenger;
        #endif
    #pragma endregion

    #pragma region device
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);
    // specify vk 1.3 features
    VkPhysicalDeviceVulkan13Features ft13 = 
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true
    };
    // specify vk 1.2 features
    VkPhysicalDeviceVulkan12Features ft12 = 
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .bufferDeviceAddress = true
    };
    // select gpu
    vkb::PhysicalDevice vkb_gpu = vkb::PhysicalDeviceSelector(vkb_inst)
        .set_minimum_version(1,3)
        .set_required_features_13(ft13)
        .set_required_features_12(ft12)
        .set_surface(_surface)
        .select()
        .value();
    _chosenGPU = vkb_gpu.physical_device;
    // build vkDevice
    auto vkb_device = vkb::DeviceBuilder(vkb_gpu).build().value();
    _device = vkb_device.device;
    
    // get graphics queue
    _graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
    #pragma endregion
    
    // initialize vma
    VmaAllocatorCreateInfo vma_CI = 
    {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = _chosenGPU,
        .device = _device,
        .instance = _instance
    };
    vmaCreateAllocator(&vma_CI, &_allocator);
    _mainDeletionQueue.push_function([&](){ vmaDestroyAllocator(_allocator); });

    #ifdef _DEBUG
    fmt::println("[ENGINE] init_vulkan(): success \n         GPU = {}", vkb_gpu.name);
    std::fflush(stdout);
    #endif
}

void Engine::init_swapchain()
{
    vkb::SwapchainBuilder vkb_swapchain_builder(_chosenGPU, _device, _surface);
    vkb_swapchain_builder
    .set_desired_extent(_swapchainExtent.width, _swapchainExtent.height)
    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    
    // image formats (HDR / SDR display outputs)
    for (const auto& f : _swapchainImgFmts)
    {
        vkb_swapchain_builder.add_fallback_format(f);
    }
    // present modes (vsync)
    for (const auto& p : _swapchainpresentModes)
    {
        vkb_swapchain_builder.add_fallback_present_mode(p);
    }
    
    // build & get handles
    auto vkb_swapchain = vkb_swapchain_builder.build().value();
    _swapchain = vkb_swapchain.swapchain;
    _swapchainImgs = vkb_swapchain.get_images().value();
    _swapchainImgViews = vkb_swapchain.get_image_views().value();

    // create draw image
    #pragma region draw_image
    
    _drawImage.fmt = VK_FORMAT_R16G16B16A16_SFLOAT; // hardcoded
    _drawImage.extent = // set to window size
    {
        .width  = _swapchainExtent.width,
        .height = _swapchainExtent.height,
        .depth = 1
    };
    VkImageUsageFlags drawImgUses = 
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ;
    VkImageCreateInfo rimg_CI = vkinit::image_create_info(_drawImage.fmt, drawImgUses, _drawImage.extent);
    VmaAllocationCreateInfo rimg_alloc_CI = 
    {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    // allocate & create image
    vmaCreateImage(_allocator, &rimg_CI, &rimg_alloc_CI, &_drawImage.img, &_drawImage.alloc, nullptr);

    // build imagview for the draw image to use for rendering
    VkImageViewCreateInfo rview_CI = vkinit::imageview_create_info(_drawImage.fmt, _drawImage.img, VK_IMAGE_ASPECT_COLOR_BIT);
    vkCreateImageView(_device, &rview_CI, nullptr, &_drawImage.imgview);

    // add to deletion queue
    _mainDeletionQueue.push_function([=]()
        {
            vkDestroyImageView(_device, _drawImage.imgview, nullptr);
            vmaDestroyImage(_allocator, _drawImage.img, _drawImage.alloc);
        });

    #pragma endregion
    
    #ifdef _DEBUG
      fmt::println("[ENGINE] init_swapchain(): success \n         framebuffs = {} | format = {}", 
        vkb_swapchain.image_count, string_VkFormat(vkb_swapchain.image_format));
      std::fflush(stdout);
    #endif
}

void Engine::resize_surface()
{
    
}

void Engine::init_commands()
{
    // create command pool
    {
        VkCommandPoolCreateInfo cmdpooCI =
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = _graphicsQueueFamily
        };
        for(FrameData& f : _frames)
        {
            VK_CHECK(vkCreateCommandPool(_device, &cmdpooCI, nullptr, &f._cmdpool));
            // allocate default framebuffer for rendering
            VkCommandBufferAllocateInfo alloc_CI = 
            {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = f._cmdpool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };
            VK_CHECK(vkAllocateCommandBuffers(_device, &alloc_CI, &f._mainCmdBuf));
        }
    }
    #ifdef _DEBUG
      fmt::println("[ENGINE] init_commands(): success");
      std::fflush(stdout);
    #endif
}

void Engine::init_sync_structures()
{
    // fence to check when GPU finishes rendering a frame
    VkFenceCreateInfo _fenceCI = 
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    // semaphores to sync with swapchain
    VkSemaphoreCreateInfo _semaphoreCI = 
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr
    };

    for(auto& f : _frames)
    {
        VK_CHECK(vkCreateFence(_device , &_fenceCI, nullptr, &f._renderFence));
        VK_CHECK(vkCreateSemaphore(_device, &_semaphoreCI, nullptr, &f._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &_semaphoreCI, nullptr, &f._renderSemaphore));
    }
    
    #ifdef _DEBUG
      fmt::println("[ENGINE] init_sync_structures(): success");
      std::fflush(stdout);
    #endif
}