#pragma once

#include <memory>
#include <limits>
#include <vector>

#include <pt2/vec3.h>

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

struct Vertex
{
    Vec3  position;
    Vec3  normal;
    float u = -1.f;
    float v = -1.f;
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

struct Triangle
{
    Vertex verts[3];
    AABB   aabb;
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

class BvhNode
{
public:
    [[nodiscard]] void
      add_triangles(const Ray &ray, std::vector<Triangle> *triangles) const noexcept;

    [[nodiscard]] bool is_leaf() const noexcept;

    AABB aabb;

    std::vector<Triangle>    triangles;
    std::unique_ptr<BvhNode> left;
    std::unique_ptr<BvhNode> right;
};
