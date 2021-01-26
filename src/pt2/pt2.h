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
#include <filesystem>

#include <pt2/structs.h>
#include <pt2/thread_pool.h>

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

        void load_model(const std::string &model, ModelType model_type);

    private:
        static void _read_file(const std::string &path, std::string &contents);

        void _handle_window();

        void _update_uniforms() const;

        void _render_screen(uint64_t spp = 0);

        void _render_task(RenderTaskDetail detail);

        void _initialize();

        [[nodiscard]] Ray _process_hit(const HitRecord &record, const Ray &ray) const;

        [[nodiscard]] HitRecord _intersect_scene(const Ray &ray);

        GLFWwindow *_window;

        RenderTargetSettings _render_target_setting;
        RayTracingContext    _ray_tracing_context;
        RenderingContext     _rendering_context;

        RTCScene    _scene;
        RTCDevice   _device;
        RTCGeometry _geometry;

        ThreadPool _render_pool;

        std::optional<Image> _envmap;
        std::vector<Material> _loaded_materials;

        std::vector<uint8_t> _material_indices;
        std::vector<glm::vec3> _vertices;
        std::vector<uint32_t>  _indices;
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