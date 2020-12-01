//
// Created by legend on 12/2/20.
//

#include <pt2/structs.h>

namespace pt2
{
    Camera::Camera(Vec3 location, Vec3 look_at, float fov, float aspect)
    {
        const auto theta       = fov * PI / 180.f;
        const auto half_height = tanf(theta / 2.f);
        const auto half_width  = aspect * half_height;

        const auto up = Vec3(0.f, 1.f, 0.f);
        const auto w  = (look_at - location).to_unit_vector();
        const auto u  = up.cross(w).to_unit_vector();
        const auto v  = w.cross(u);

        this->location   = location;
        this->center     = { location + w };
        this->vertical   = { v * half_height };
        this->horizontal = { u * half_width };
    }

    [[nodiscard]] Ray Camera::get_ray(float x, float y) const noexcept
    {
        const auto x_offset  = horizontal * (x * 2.f - 1);
        const auto y_offset  = vertical * (y * 2.f - 1);
        const auto direction = ((center + x_offset + y_offset) - location).to_unit_vector();
        return Ray(location, direction);
    }

    Ray::Ray(Vec3 origin, Vec3 direction) : origin(origin), direction(direction)
    {
    }

    Vec3 Ray::point_at(float T) const noexcept
    {
        return origin + direction * T;
    }

// AABB

    AABB::AABB() : min_x(0), min_y(0), min_z(0), max_x(0), max_y(0), max_z(0)
    {
    }

    AABB::AABB(Vec3 min, Vec3 max, bool unsafe)
      : min_x(min.x), min_y(min.y), min_z(min.z), max_x(max.x), max_y(max.y), max_z(max.z)
    {
        if (unsafe)
        {    // Unsafe means that it might be mixed up with min/max so we need to do extra work
            min_x = fminf(min_x, max_x);
            min_y = fminf(min_y, max_y);
            min_z = fminf(min_z, max_z);

            max_x = fmaxf(min_x, max_x);
            max_y = fmaxf(min_y, max_y);
            max_z = fmaxf(min_z, max_z);
        }
    }

    AABB::AABB(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z)
      : min_x(min_x), min_y(min_y), min_z(min_z), max_x(max_x), max_y(max_y), max_z(max_z)
    {
    }

    float AABB::operator[](int i) const noexcept
    {
        switch (i)
        {
        case 0: return min_x;
        case 1: return min_y;
        case 2: return min_z;
        case 3: return max_x;
        case 4: return max_y;
        case 5: return max_z;
        default: return std::numeric_limits<float>::lowest();
        }
    }

    Vec3 AABB::get_center() const noexcept
    {
        return Vec3(min_x + size_x() / 2, min_y + size_y() / 2, min_z + size_z() / 2);
    }

    float AABB::size_x() const noexcept
    {
        return max_x - min_x;
    }

    float AABB::size_y() const noexcept
    {
        return max_y - min_y;
    }

    float AABB::size_z() const noexcept
    {
        return max_z - min_z;
    }

    void AABB::expand(Vec3 to) noexcept
    {
        min_x = fminf(to.x, min_x);
        min_y = fminf(to.y, min_y);
        min_z = fminf(to.z, min_x);
        max_x = fmaxf(to.x, max_x);
        max_y = fmaxf(to.y, max_y);
        max_z = fmaxf(to.z, max_z);
    }

    bool AABB::contains(const Vec3 &point) const noexcept
    {
        const auto x = min_x < point.x && point.x < max_x;
        const auto y = min_y < point.y && point.y < max_y;
        const auto z = min_z < point.z && point.z < max_z;
        return x & y & z;
    }

    void AABB::expand(AABB to) noexcept
    {
        min_x = fminf(to.min_x, min_x);
        min_y = fminf(to.min_y, min_y);
        min_z = fminf(to.min_z, min_x);
        max_x = fmaxf(to.max_x, max_x);
        max_y = fmaxf(to.max_y, max_y);
        max_z = fmaxf(to.max_z, max_z);
    }
}
