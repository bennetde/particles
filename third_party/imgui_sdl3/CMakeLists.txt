add_library(imgui_sdl3
    imgui_impl_sdl3.cpp
    )

set_property(TARGET imgui_sdl3 PROPERTY CXX_STANDARD 20)

target_link_libraries(imgui_sdl3 PRIVATE 
    SDL3::SDL3
    imgui::imgui
    )