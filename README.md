# Particles

Particles is a small particle life/clusters simulation written in C++ and Vulkan for learning purposes.
Clusters is a particle system introduced by [Jeffrey Ventrella](https://www.ventrella.com/Clusters/intro.html).
Right now it can only handle a low amount of particles (<1000) depending on the hardware the renderer is running on.

## Screenshots
![A screenshot was supposed to be here](assets/screenshots/screenshot_1.PNG)

## Dependencies & Setup
All dependencies are handled using vcpkg. CMake is used as a meta buildsystem.

Dependencies:
* fmt
* glm
* imgui
* sdl3
* stb
* vk-bootstrap
* vulkan
* vulkan-memory-allocator

The project adds a custom target for building shaders separately.

## TODO
- [ ] Optimizations
- [ ] Window Resize
- [ ] Code Cleanup & Documentation

## Contributions
This is a private project for learning purporses, so I will not accept any pull requests.
But feel free to fork the project and adapt it to your needs.
