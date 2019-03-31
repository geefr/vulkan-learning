#ifndef WINDOWINTEGRATION_H
#define WINDOWINTEGRATION_H

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#else
# error "WSI type not supported by WindowIntegration"
#endif
#include <vulkan/vulkan.hpp>

/**
 * Functionality for window system integration, swapchains and such
 */
class WindowIntegration
{
public:
  WindowIntegration() = delete;
  WindowIntegration(const WindowIntegration&) = delete;
  WindowIntegration(WindowIntegration&&) = default;
  ~WindowIntegration() = default;

#ifdef USE_GLFW
  WindowIntegration(vk::Instance& instance, vk::PhysicalDevice& pDevice, uint32_t queueFamilyIndex, vk::Device& device, GLFWwindow* window);
#endif

  // TODO: Giving direct access like this is dangerous, clean this up
  const vk::Extent2D& extent() const { return mSwapChainExtent; }
  const vk::Format& format() const { return mSwapChainFormat; }
  const vk::SwapchainKHR& swapChain() const { return mSwapChain.get(); }
  const std::vector<vk::Image>& swapChainImages() const { return mSwapChainImages; }
  const std::vector<vk::UniqueImageView>& swapChainImageViews() const { return mSwapChainImageViews; }

private:
#ifdef USE_GLFW
  void createSurfaceGLFW(vk::Instance& instance, GLFWwindow* window);
#endif

  void createSwapChain(vk::PhysicalDevice& pDevice, uint32_t queueFamilyIndex, vk::Device& device);
  void createSwapChainImageViews(vk::Device& device);

  // These members 'owned' by the unique pointers below
  vk::Format mSwapChainFormat = vk::Format::eUndefined;
  vk::Extent2D mSwapChainExtent;
  std::vector<vk::Image> mSwapChainImages;

  // Order of members matters for Unique pointers
  // will be destructed in reverse order
  vk::UniqueSurfaceKHR mSurface;
  vk::UniqueSwapchainKHR mSwapChain;
  std::vector<vk::UniqueImageView> mSwapChainImageViews;
};

#endif // WINDOWINTEGRATION_H
