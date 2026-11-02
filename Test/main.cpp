#include "IRenderBackend.hpp"
#include "Vulkan/VulkanBackend.hpp"
#include <GLFW/glfw3.h>

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* Window = glfwCreateWindow(640, 480, "Title", nullptr, nullptr);
    auto Backend = arhi::IRenderBackend::CreateBackend<arhi::CVulkanBackend>();
    Backend->Initialize();

    Backend->RegisterWindow(Window);

    while(!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();
    }

    Backend->DeregisterWindow(Window);
    glfwTerminate();
}