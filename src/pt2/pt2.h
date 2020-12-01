#pragma once

#define PT2_GLFW

#ifdef PT2_GLFW

#include <GLFW/glfw3.h>

#endif

#include <memory>
#include <cstdint>
#include <vector>
#include <string>

#include <pt2/vec3.h>
#include <pt2/geometry.h>

namespace pt2
{
    constexpr uint32_t INVALID_HANDLE = std::numeric_limits<uint32_t>::max();
    constexpr uint8_t MAX_THREAD_COUNT = 0;

    uint64_t add_vertex(float x, float y, float z);


    void add_triangle(size_t vertex_0, size_t vertex_1, size_t vertex_2);

    class Image
    {
    public:
        Image() = delete;

        Image(int width, int height, const uint8_t *data);

        [[nodiscard]] auto width() const noexcept -> uint32_t;

        [[nodiscard]] auto height() const noexcept -> uint32_t;

        [[nodiscard]] auto get(float u, float v) const noexcept -> uint32_t;

        [[nodiscard]] auto get(uint32_t x, uint32_t y) const noexcept -> uint32_t;

    private:
        uint32_t _width;
        uint32_t _height;
        std::vector<uint32_t> _data;
    };

    class Model
    {
    public:
        Model() = delete;

        Model(std::vector<Vec3> &vertices, std::vector<Vec3> &normals, std::vector<Indice> &indices,
              std::vector<std::pair<float, float>> &textures);

        std::vector<Vec3> _vertices;
        std::vector<Vec3> _normals;
        std::vector<Indice> _indices;
        std::vector<std::pair<float, float>> _textures;
    };

    struct SceneRenderDetail
    {
        uint8_t max_bounces = 5;
        uint8_t thread_count = 1;
        uint16_t spp = 4;
        uint16_t width = 512;
        uint16_t height = 512;
        struct
        {
            Vec3 origin;
            Vec3 lookat;
        } camera;
    };


    [[nodiscard]] uint32_t load_image(std::string_view image_name);

    void load_model(std::string path, std::string model_name);

    [[maybe_unused]] void set_skybox(uint32_t image_handle);

    void init();

    void build_bvh();

    void render_scene(SceneRenderDetail detail, uint32_t *buffer);

#ifdef PT2_GLFW

    void render_and_display_scene(SceneRenderDetail detail);

#endif

}
