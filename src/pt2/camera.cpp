#include "camera.h"

const auto PI = atanf(1) * 4;

Camera::Camera(Vec3 location, Vec3 look_at, float fov, float aspect)
{
    const auto theta = fov * PI / 180.f;
    const auto half_height = tanf(theta / 2.f);
    const auto half_width = aspect * half_height;

    const auto up = Vec3(0.f, 1.f, 0.f);
    const auto w = (look_at - location).to_unit_vector();
    const auto u = up.cross(w).to_unit_vector();
    const auto v = w.cross(u);

    this->location = location;
    this->center = { location + w };
    this->vertical = { v * half_height };
    this->horizontal = { u * half_width };
}

[[nodiscard]] Ray Camera::get_ray(float x, float y) const noexcept
{
    const auto x_offset = horizontal * (x * 2.f - 1);
    const auto y_offset = vertical * (y * 2.f - 1);
    const auto direction = ((center + x_offset + y_offset) - location).to_unit_vector();
    return Ray(location, direction);
}
