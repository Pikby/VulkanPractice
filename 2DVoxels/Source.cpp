#include "VulkanHelpers.h"
#include "VulkanWindow.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
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

#include <list>








class HelloTriangleApplication {
public:
    void run() {
        initGLFW();


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


    std::unique_ptr<VulkanWindow> vulkanWindow;



    size_t currentFrame = 0;
    void initVulkan() {
        vulkanInstance = VulkanInstance::make();
        VkInstance& instance = vulkanInstance->getInstance();

        vulkanDebugMessenger = VulkanDebugMessenger::make(instance);
        
        vulkanSurface = VulkanSurface::make(instance);
        VkSurfaceKHR& surface = vulkanSurface->getSurface();

        vulkanLogicalDevice = VulkanLogicalDevice::make(instance, surface);
        VkDevice& device = vulkanLogicalDevice->getDevice();
        VkPhysicalDevice& pDevice = vulkanLogicalDevice->getPhysicalDevice();
        vulkanCommandPool = VulkanCommandPool::make(device, surface, pDevice);

        vulkanSwapChain = VulkanSwapChain::make(device, surface, pDevice, vulkanCommandPool->getCommandPool(),vulkanLogicalDevice->getGraphicsQueue());
        vulkanSyncObjects = VulkanSyncObjects::make(device, vulkanSwapChain->getSwapChainImages());
   
    } 

    void initGLFW() {
        glfwInit();
        vulkanWindow = VulkanWindow::makeWindow(800, 600, "Vulkan Testing");
        VulkanHelper::setWindow(vulkanWindow->getWindow());
    }

    void mainLoop() {

        while (!glfwWindowShouldClose(vulkanWindow->getWindow())) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(vulkanLogicalDevice->getDevice());
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};

        auto& swapChainExtent = (vulkanSwapChain->getSwapChainExtent());
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;


        auto& device = vulkanLogicalDevice->getDevice();
        void* data;

        auto& uniformBuffersMemory = vulkanSwapChain->getUniformBuffersMemory();
        vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
            memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
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

        updateUniformBuffer(imageIndex);

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

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vulkanWindow->getFramebufferResized()) {
            vulkanWindow->setFramebufferResized(false);
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    }

    void recreateSwapChain() {
        vkDeviceWaitIdle(vulkanLogicalDevice->getDevice());
        vulkanSwapChain.reset();
        vulkanSwapChain = VulkanSwapChain::make(vulkanLogicalDevice->getDevice(), vulkanSurface->getSurface(), vulkanLogicalDevice->getPhysicalDevice(), vulkanCommandPool->getCommandPool(),vulkanLogicalDevice->getGraphicsQueue());
    }



    void cleanup() {
        vulkanSyncObjects.reset();
        vulkanSwapChain.reset();
        vulkanCommandPool.reset();
        vulkanLogicalDevice.reset();
        vulkanSurface.reset();
        vulkanDebugMessenger.reset();
        vulkanInstance.reset();


        glfwTerminate();
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

