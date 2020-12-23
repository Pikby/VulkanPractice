#pragma once
#include <GLFW\glfw3.h>
#include <string>



class HellowTriangleApplication;
class VulkanWindow {
private:
	GLFWwindow* window;
    static bool framebufferResized;
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        framebufferResized = true;
    }
public:

    static std::unique_ptr<VulkanWindow> makeWindow(int x, int y, const std::string& str) {
        return std::make_unique<VulkanWindow>(x, y, str);
    }


    VulkanWindow(int x, int y, const std::string& str)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(800, 600, "Vulkan Testing", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    ~VulkanWindow(){
        glfwDestroyWindow(window);
    }

    bool getFramebufferResized() {
        return framebufferResized;
    }

   void setFramebufferResized(bool b) {
        framebufferResized = b;
    }

    GLFWwindow* getWindow() {
        return window;
    }
};

bool VulkanWindow::framebufferResized;