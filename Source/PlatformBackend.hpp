#pragma once

#ifdef ARHI_BACKEND_GLFW

    #ifdef ARHI_BUILD_VULKAN
        #define GLFW_INCLUDE_VULKAN
    #endif

    #include <GLFW/glfw3.h>

    #ifdef ARHI_BUILD_VULKAN
        #define PlatformIsVulkanSupported glfwVulkanSupported
        #define PlatformGetInstanceExtensions(OutCount) glfwGetRequiredInstanceExtensions(OutCount)
        #define PlatformCreateSurface(Instance, Window, Allocator, OutSurface) glfwCreateWindowSurface(Instance, Window, Allocator, OutSurface) == VK_SUCCESS
    #endif

    #define PlatformWindowType GLFWwindow

#endif

#ifdef ARHI_BACKEND_SDL3

    #include <SDL3/SDL.h>

    #ifdef ARHI_BUILD_VULKAN
        #include <SDL3/SDL_vulkan.h>

        #define PlatformIsVulkanSupported SDL_Vulkan_LoadLibrary(NULL) == 0
        #define PlatformGetInstanceExtensions(OutCount) SDL_Vulkan_GetInstanceExtensions(OutCount)
        #define PlatformCreateSurface(Instance, Window, Allocator, OutSurface) SDL_Vulkan_CreateSurface(Window, Instance, Allocator, OutSurface)
        #define PlatformDestroySurface glfwCreateWindowSurface
    #endif

    #define PlatformWindowType SDL_Window

#endif