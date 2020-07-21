# Vulkan Learning
This is just an area for me to play around with Vulkan, current plan is to work on putting together a basic engine supporting PBR from glTF, and enough engine concepts to put together some game demos.

So far models can be loaded, arranged in a scene graph, and rendered. There's also the ability to hook update methods onto nodes and a primitive camera setup.

Right now the rendering itself is very basic - The shader interfaces are getting there and there's some basic lighting. Haven't implemented a proper PBR shader yet, or added texturing to the shader interface.

# Platform Support
Development is being done between Linux and Windows, testing on Nvidia and Intel GPUs.

For Android the core sections should mostly work but it's hardcoded to use GLFW for now, would require rework here to use the Android UI system.

# Licence
Everything is BSD 3-Clause unless specified in the file, there's some MIT and similar stuff under extern.

In general though feel free to use this if you want to, I make no guarantees about bugs/functionality.

