#pragma once

#if __has_include(<GLFW/glfw3.h>)
    #define PT2_GLFW
    #include <GLFW/glfw3.h>
#endif

#include <memory>
#include <vector>
#include <string>

#include <pt2/structs.h>
#include <pt2/vec3.h>
#include <pt2/geometry.h>

namespace pt2
{
    constexpr uint32_t INVALID_HANDLE = std::numeric_limits<uint32_t>::max();
    constexpr uint8_t MAX_THREAD_COUNT = 0;

    [[nodiscard]] uint32_t load_image(std::string_view image_name);

    void load_model(std::string path, std::string model_name);

    [[maybe_unused]] void set_skybox(uint32_t image_handle);

    void render_scene(SceneRenderDetail detail, uint32_t *buffer);

#ifdef PT2_GLFW

    void render_and_display_scene(SceneRenderDetail detail);

#endif

}
