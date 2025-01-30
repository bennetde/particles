add_executable(renderer
    main.cpp
    renderer.cpp
    swapchain.cpp
    init.cpp
    image.cpp
    buffer.cpp
    pipeline.cpp
    particles/particle.cpp
    deletion_queue.cpp
    )


set_property(TARGET renderer PROPERTY CXX_STANDARD 20)
target_compile_definitions(renderer PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

# Engine Dependencies
target_link_libraries(renderer PRIVATE 
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    vk-bootstrap::vk-bootstrap
    vk-bootstrap::vk-bootstrap-compiler-warnings
    SDL3::SDL3
    glm::glm
    imgui::imgui
    fmt::fmt
    fastgltf::fastgltf
    imgui_sdl3
    )

target_include_directories(renderer PRIVATE ${Stb_INCLUDE_DIR} ../third_party/imgui_sdl3)

target_precompile_headers(renderer PUBLIC <optional> <vector> <string> <vulkan/vulkan.h>)

add_custom_command(TARGET renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:renderer> $<TARGET_FILE_DIR:renderer>
    COMMAND_EXPAND_LISTS
    )