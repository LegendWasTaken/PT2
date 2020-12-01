#include "pt2/geometry.h"

#include <cmath>
// RAY

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

void BvhNode::add_triangles(const Ray &ray, std::vector<Triangle> *triangles) const noexcept
{
    const auto ray_inv =
      Vec3 { 1.f / ray.direction.x, 1.f / ray.direction.y, 1.f / ray.direction.z };
    auto t1 = (aabb[0] - ray.origin[0]) * ray_inv[0];
    auto t2 = (aabb[3] - ray.origin[0]) * ray_inv[0];

    auto tmin = fminf(t1, t2);
    auto tmax = fmaxf(t1, t2);

    for (int i = 1; i < 3; ++i)
    {
        t1 = (aabb[i] - ray.origin[i]) * ray_inv[i];
        t2 = (aabb[i + 3] - ray.origin[i]) * ray_inv[i];

        tmin = fmaxf(tmin, fminf(t1, t2));
        tmax = fminf(tmax, fmaxf(t1, t2));
    }

    if (tmax > fmaxf(tmin, 0.0))
    {
        if (!this->triangles.empty())
        {
            triangles->reserve(this->triangles.size() + triangles->size());
            for (const auto &tri : this->triangles) triangles->push_back(tri);
        }
        if (is_leaf()) return;
        left->add_triangles(ray, triangles);
        right->add_triangles(ray, triangles);
    }
}

bool BvhNode::is_leaf() const noexcept
{
    return left == nullptr && right == nullptr;
}
