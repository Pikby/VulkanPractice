#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>


#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <fstream>



const int MAX_FRAMES_IN_FLIGHT = 2;

#include "VulkanHelpers.h"



class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:

    std::unique_ptr<VulkanSyncObjects> vulkanSyncObjects;
    std::unique_ptr<VulkanInstance> vulkanInstance;
    std::unique_ptr<VulkanDebugMessenger> vulkanDebugMessenger;
    std::unique_ptr<VulkanSurface> vulkanSurface;

    std::unique_ptr<VulkanLogicalDevice> vulkanLogicalDevice;
    std::unique_ptr<VulkanCommandPool> vulkanCommandPool;
    std::unique_ptr<VulkanSwapChain> vulkanSwapChain;

    GLFWwindow* window;
   
    size_t currentFrame = 0;


    bool framebufferResized = false;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(800, 600, "Vulkan Testing", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    }

    void initVulkan() {

        VulkanHelper::setWindow(window);

           
        vulkanInstance          = std::make_unique<VulkanInstance>();
        vulkanDebugMessenger    = std::make_unique<VulkanDebugMessenger>(vulkanInstance->getInstance());
        vulkanSurface           = std::make_unique<VulkanSurface>(vulkanInstance->getInstance());
        vulkanLogicalDevice     = std::make_unique<VulkanLogicalDevice>(vulkanInstance->getInstance(), vulkanSurface->getSurface());
        vulkanCommandPool       = std::make_unique<VulkanCommandPool>(vulkanLogicalDevice->getDevice(), vulkanLogicalDevice->getPhysicalDevice(),vulkanSurface->getSurface());
        vulkanSwapChain         = std::make_unique<VulkanSwapChain>(vulkanLogicalDevice->getDevice(), vulkanSurface->getSurface(), vulkanLogicalDevice->getPhysicalDevice(), vulkanCommandPool->getCommandPool());
        vulkanSyncObjects       = std::make_unique<VulkanSyncObjects>(vulkanLogicalDevice->getDevice(), vulkanSwapChain->getSwapChainImages());
        
    
    } 

    void mainLoop() {

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(vulkanLogicalDevice->getDevice());
    }

    void drawFrame() {
        std::vector<VkSemaphore>& imageAvailableSemaphores = vulkanSyncObjects->getImageAvailableSemaphores();
        std::vector<VkSemaphore>& renderFinishedSemaphores = vulkanSyncObjects->getRenderFinishedSemaphores();

        std::vector<VkFence>& inFlightFences = vulkanSyncObjects->getInFlightFences();
        std::vector<VkFence>& imagesInFlight = vulkanSyncObjects->getImagesInFlight();
        VkDevice& device = vulkanLogicalDevice->getDevice();
        VkQueue& graphicsQueue = vulkanLogicalDevice->getGraphicsQueue();
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, vulkanSwapChain->getSwapChain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }


        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &((vulkanSwapChain->getCommandBuffers())[imageIndex]);

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { vulkanSwapChain->getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        presentInfo.pResults = nullptr; // Optional


        result = vkQueuePresentKHR(vulkanLogicalDevice->getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }


        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    }

    void recreateSwapChain() {
        vkDeviceWaitIdle(vulkanLogicalDevice->getDevice());
        vulkanSwapChain = nullptr;
        vulkanSwapChain = std::make_unique<VulkanSwapChain>(vulkanLogicalDevice->getDevice(), vulkanSurface->getSurface(), vulkanLogicalDevice->getPhysicalDevice(), vulkanCommandPool->getCommandPool());

    }



    void cleanup() {
        vulkanSyncObjects = nullptr;
        vulkanSwapChain = nullptr;
        vulkanCommandPool = nullptr;
        vulkanLogicalDevice = nullptr;
        vulkanSurface = nullptr;
        vulkanDebugMessenger = nullptr;
        vulkanInstance = nullptr;
    }

    
};

int main() {
    HelloTriangleApplication app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

