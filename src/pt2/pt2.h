#pragma once

#include <memory>

#include <vector>
#include <string>
#include <optional>

#include <pt2/structs.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec3.hpp>

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

        void _read_file(const std::string &path, std::string &contents);

        void _handle_window();

        RenderDetail current_render_detail;

        GLFWwindow *_window;

        std::vector<PT2::Material> materials;
        std::vector<glm::vec3>     vertices;
        std::vector<uint32_t>      indices;
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