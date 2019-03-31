#ifndef REGISTRAR_H
#define REGISTRAR_H

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#endif
// https://github.com/KhronosGroup/Vulkan-Hpp
# include <vulkan/vulkan.hpp>



#ifdef VK_USE_PLATFORM_XLIB_KHR
# include <X11/Xlib.h>
#endif

#include "deviceinstance.h"
#include "windowintegration.h"
#include "framebuffer.h"
#include "graphicspipeline.h"

/**
 * Probably the wrong name for this
 * Basically a big singleton for storing
 * vulkan objects in, and a bunch
 * of util functions
 *
 * Each instance of this class maps to a vulkan instance
 * So for now it's a singleton
 */

class Registrar
{
public:
  static Registrar& singleton();

  void tearDown();

private:
  Registrar();
  Registrar(const Registrar&) = delete;
  Registrar(const Registrar&&) = delete;
  ~Registrar();


  static Registrar mReg;
};

inline Registrar& Registrar::singleton() { return mReg; }
#endif
