#pragma once

#include <cmath>

class Vec3
{
public:
    float x, y, z;
    Vec3();
    Vec3(float, float, float);

    float operator[](int i) const noexcept;
    void  operator+=(const Vec3 &a) noexcept;
    void  operator-=(const Vec3 &a) noexcept;
    void  operator/=(const Vec3 &a) noexcept;
    void  operator*=(const Vec3 &a) noexcept;
    Vec3  operator+(float a) const noexcept;
    Vec3  operator-(float a) const noexcept;
    Vec3  operator/(float a) const noexcept;
    Vec3  operator*(float t) const noexcept;
    Vec3  operator+(const Vec3 &a) const noexcept;
    Vec3  operator-(const Vec3 &a) const noexcept;
    Vec3  operator/(const Vec3 &a) const noexcept;
    Vec3  operator*(const Vec3 &a) const noexcept;

    void                mix(const Vec3 &, float) noexcept;
    void                refract(const Vec3 &, float) noexcept;
    [[nodiscard]] float dot(const Vec3 &rhs) const noexcept;
    [[nodiscard]] float dist(const Vec3 &rhs) const noexcept;
    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] float length_sq() const noexcept;
    [[nodiscard]] Vec3  cross(const Vec3 &rhs) const noexcept;
    [[nodiscard]] Vec3  to_unit_vector() const noexcept;
    [[nodiscard]] Vec3  reflect(const Vec3 &rhs) const noexcept;
    [[nodiscard]] Vec3  half_unit_from(const Vec3 &rhs, bool normal = true) const noexcept;
    [[nodiscard]] Vec3  inv() const noexcept;
};
