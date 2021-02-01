//
// Created by legend on 12/2/20.
//

#pragma once

#include <cstdint>
#include <cmath>

#include <glm/glm.hpp>

namespace PT2
{
    struct RenderTaskDetail
    {
        uint16_t x;
        uint16_t y;
    };

    struct Image
    {
        uint64_t             width;
        uint64_t             height;
        std::vector<uint8_t> data;
    };

    struct Ray
    {
    public:
        Ray(const glm::vec3 &origin, const glm::vec3 &direction)
            : origin(origin), direction(direction)
        {
        }

        [[nodiscard]] glm::vec3 point_at(float t) const noexcept { return origin + direction * t; }

        glm::vec3 origin;
        glm::vec3 direction;
    };

    struct Material
    {
    public:
        enum Type
        {
            DIFFUSE,
            REFRACTIVE,
            MIRROR,
            METAL,
        } type;

        float reflectiveness = 0.2f;
        float roughness      = 0.0f;
        float emission       = 0.0f;
        float ior            = 0.0f;

        glm::vec3   color = glm::vec3(0.0f, 0.0f, 0.0f);
        std::string name;
    };

    struct RenderTargetSettings
    {
        float x_offset = 0.f;
        float y_offset = 0.f;
        float x_scale  = .5f;
        float y_scale  = .50f;
    };

    struct RenderingContext
    {
        unsigned int gl_program;
        unsigned int gl_fragment_shader;
        unsigned int gl_vertex_shader;
        unsigned int resolution_x;
        unsigned int resolution_y;
    };

    struct HitRecord
    {
    public:
        bool      hit          = false;
        float     distance     = std::numeric_limits<float>::min();
        Material *hit_material = nullptr;
        glm::vec3 normal;
        glm::vec3 intersection_point;
    };

    struct Camera
    {
    public:
        Camera(const glm::vec3 &location, const glm::vec3 &look_at, float fov, float aspect)
        {
            const auto theta       = fov * 3.141592f / 180.f;
            const auto half_height = tanf(theta / 2.f);
            const auto half_width  = aspect * half_height;

            constexpr auto up = glm::vec3(0, 1, 0);
            const auto     w  = glm::normalize(look_at - location);
            const auto     u  = glm::normalize(glm::cross(up, w));
            const auto     v  = glm::cross(w, u);

            this->horizontal = u * half_width;
            this->vertical   = v * half_height;
            this->location   = location;
            this->center     = location + w;
        }

        [[nodiscard]] Ray get_ray(float x, float y) const noexcept
        {
            const auto x_offset  = horizontal * (x * 2.f - 1);
            const auto y_offset  = vertical * (y * 2.f - 1);
            const auto direction = glm::normalize((center + x_offset + y_offset) - location);
            return Ray(location, direction);
        }

    private:
        glm::vec3 horizontal {};
        glm::vec3 vertical {};
        glm::vec3 location {};
        glm::vec3 center {};
    };

    struct RayTracingContext
    {
        int bounces = 4;
        int spp     = 1;
        struct
        {
            int x = 512;
            int y = 512;
        } resolution;
        struct
        {
            int count  = 16;
            int x_size = 512 / 8;
            int y_size = 512 / 8;
        } tiles;
        std::vector<uint32_t> buffer;

        Camera camera = Camera(glm::vec3(-15, 12, 8), glm::vec3(0, 0, 0), 90, 1);
    };

}    // namespace PT2
