#pragma once

#include <memory>

#include <vector>
#include <string>
#include <optional>

#include <pt2/structs.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>
#include <atomic>

namespace PT2
{
    enum class ModelType
    {
        OBJ
    };

    class Renderer
    {
    public:
        Renderer() = default;

        void start_gui();

        void submit_detail(const RenderDetail &detail);

        void load_model(const std::string &model, ModelType model_type);

    private:

        static void _read_file(const std::string &path, std::string &contents);

        void _handle_window();

        void _update_uniforms() const;

        struct RenderTargetSettings
        {
            float x_offset = 0.f;
            float y_offset = 0.f;
            float x_scale = 1.0f;
            float y_scale = 1.0f;
        } _render_target_setting;

        RenderDetail current_render_detail;

        GLFWwindow *_window;

        struct
        {
            unsigned int gl_program;
            unsigned int gl_fragment_shader;
            unsigned int gl_vertex_shader;
            unsigned int resolution_x;
            unsigned int resolution_y;
        } _rendering_context;
    };
}    // namespace PT2

/*
namespace pt2
{
    constexpr uint32_t INVALID_HANDLE   = std::numeric_limits<uint32_t>::max();
    constexpr uint8_t  MAX_THREAD_COUNT = 0;

    void load_model(std::string path, std::string model_name);

    void render_scene(SceneRenderDetail detail, uint32_t *buffer);

    [[nodiscard]] uint32_t load_image(std::string_view image_name);

    [[maybe_unused]] void set_skybox(uint32_t image_handle);

}    // namespace pt2
*/