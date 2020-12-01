//
// Created by legend on 12/2/20.
//

#pragma once

#include <cstdint>
#include <vector>

#include <pt2/vec3.h>
#include <pt2/structs.h>

namespace pt2
{
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
        uint32_t              _width;
        uint32_t              _height;
        std::vector<uint32_t> _data;
    };

    struct SceneRenderDetail
    {
        uint8_t  max_bounces  = 5;
        uint8_t  thread_count = 1;
        uint16_t spp          = 4;
        uint16_t width        = 512;
        uint16_t height       = 512;
        struct
        {
            Vec3 origin;
            Vec3 lookat;
        } camera;
    };

    struct HitRecord
    {
        Vec3  normal;
        Vec3  emission;
        Vec3  albedo;
        Vec3  intersection_point;
        float distance       = std::numeric_limits<float>::min();
        float reflectiveness = 0;
        bool  hit            = false;
    };

    class AABB
    {
    public:
        AABB();

        AABB(Vec3 min, Vec3 max, bool unsafe = false);

        AABB(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z);

        [[nodiscard]] float operator[](int i) const noexcept;

        [[nodiscard]] Vec3 get_center() const noexcept;

        [[nodiscard]] bool contains(const Vec3 &point) const noexcept;

        [[nodiscard]] float size_x() const noexcept;

        [[nodiscard]] float size_y() const noexcept;

        [[nodiscard]] float size_z() const noexcept;

        void expand(Vec3 to) noexcept;

        void expand(AABB to) noexcept;

        float min_x;
        float min_y;
        float min_z;
        float max_x;
        float max_y;
        float max_z;
    };

    struct Indice
    {
        size_t position;
        size_t normal;
        size_t texture;
    };

    class Ray
    {
    public:
        Ray(Vec3 origin, Vec3 direction);

        [[nodiscard]] Vec3 point_at(float T) const noexcept;

        Vec3 origin;
        Vec3 direction;
    };

    class Camera
    {
    public:
        Camera(Vec3 location, Vec3 look_at, float fov, float aspect);

        [[nodiscard]] Ray get_ray(float x, float y) const noexcept;

    private:
        Vec3 center, vertical, horizontal, location;
    };

}    // namespace pt2
