#pragma once

#include <memory>

#include <vector>
#include <string>
#include <optional>
#include <queue>
#include <mutex>
#include <thread>
#include <shared_mutex>
#include <embree3/rtcore.h>

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
        Renderer();

        ~Renderer();

        void start_gui();

        void submit_detail(const RenderDetail &detail);

        void load_model(const std::string &model, ModelType model_type);

    private:
        static void _read_file(const std::string &path, std::string &contents);

        void _handle_window();

        void _update_uniforms() const;

        void _render_screen();

        void _render_task();

        [[noreturn]] void _render_thread_task();

        void _initialize();

        [[nodiscard]] Ray _process_hit(const HitRecord &record, const Ray &ray) const;

        [[nodiscard]] HitRecord _intersect_scene(const Ray &ray) const;

        [[nodiscard]] std::optional<std::pair<uint16_t, uint16_t>> _next_tile_to_render();

        GLFWwindow *_window;

        std::mutex _tile_mutex;
        std::atomic<bool> _is_rendering = false;
        std::atomic<bool> _updating_scene = false;

        RenderTargetSettings _render_target_setting;
        RayTracingContext    _ray_tracing_context;
        RenderingContext     _rendering_context;
        RenderDetail         _render_detail;

        RTCScene    _scene;
        RTCDevice   _device;
        RTCGeometry _geometry;

        std::vector<glm::vec3> _vertices;
        std::vector<uint32_t> _indices;
        std::vector<std::thread> _render_threads;

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