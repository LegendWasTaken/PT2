#pragma once

#include <cmath>

#include <pt2/vec3.h>
#include <pt2/geometry.h>

class Camera
{
public:
    Camera(Vec3 location, Vec3 look_at, float fov, float aspect);

    [[nodiscard]] Ray get_ray(float x, float y) const noexcept;
private:
    Vec3 center, vertical, horizontal, location;
};


