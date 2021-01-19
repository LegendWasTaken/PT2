//
// Created by legend on 12/2/20.
//

#pragma once

#include <cstdint>
#include <cmath>

#include <glm/glm.hpp>

namespace PT2
{
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
        enum
        {
            DIFFUSE,
            REFRACTIVE,
            MIRROR,
            METAL,
        } type;

        float reflectiveness = 0.0f;
        float roughness      = 0.0f;
        float emission       = 0.0f;
        float ior            = 0.0f;

        glm::vec3 color = glm::vec3(0.0f, 0.0f, 0.0f);
    };

    struct RenderDetail
    {
    public:
        uint16_t thread_count      = 1;
        uint16_t samples_per_pixel = 4;
        uint16_t min_bounces       = 5;
        uint16_t width             = 512;
        uint16_t height            = 512;
        struct
        {
            glm::vec3 origin  = glm::vec3();
            glm::vec3 look_at = glm::vec3();
            float     fov     = 90.f;
        } camera;
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

        [[nodiscard]] Ray get_ray(float x, float) const noexcept { }

    private:
        glm::vec3 horizontal{};
        glm::vec3 vertical{};
        glm::vec3 location{};
        glm::vec3 center{};
    };

}    // namespace PT2
