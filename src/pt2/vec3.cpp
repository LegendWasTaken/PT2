#include "vec3.h"

Vec3::Vec3()
{
    this->x = 0;
    this->y = 0;
    this->z = 0;
}

Vec3::Vec3(float x, float y, float z)
{
    this->x = x;
    this->y = y;
    this->z = z;
}

float Vec3::operator[](int i) const noexcept
{
    if (i == 0)
        return x;
    if (i == 1)
        return y;
    if (i == 2)
        return z;
    return x;
}

Vec3 Vec3::operator+(const Vec3 &a) const noexcept
{
    return Vec3(x + a.x, y + a.y, z + a.z);
}

void Vec3::operator+=(const Vec3 &a) noexcept
{
    x += a.x;
    y += a.y;
    z += a.z;
}

Vec3 Vec3::operator-(const Vec3 &a) const noexcept
{
    return Vec3(x - a.x, y - a.y, z - a.z);
}

void Vec3::operator-=(const Vec3 &a) noexcept
{
    x -= a.x;
    y -= a.y;
    z -= a.z;
}

Vec3 Vec3::operator/(const Vec3 &a) const noexcept
{
    return Vec3(x / a.x, y / a.y, z / a.z);
}

void Vec3::operator/=(const Vec3 &a) noexcept
{
    x /= a.x;
    y /= a.y;
    z /= a.z;
}

Vec3 Vec3::operator*(const Vec3 &a) const noexcept
{
    return Vec3(x * a.x, y * a.y, z * a.z);
}

void Vec3::operator*=(const Vec3 &a) noexcept
{
    x *= a.x;
    y *= a.y;
    z *= a.z;
}

Vec3 Vec3::operator+(float a) const noexcept
{
    return Vec3(x + a, y + a, z + a);
}

Vec3 Vec3::operator-(float a) const noexcept
{
    return Vec3(x - a, y - a, z - a);
}

Vec3 Vec3::operator/(float a) const noexcept
{
    return Vec3(x / a, y / a, z / a);
}

Vec3 Vec3::operator*(float t) const noexcept
{
    return Vec3(x * t, y * t, z * t);
}

void Vec3::mix(const Vec3 &a, float t) noexcept
{
    x = x + (a.x - x) * t;
    y = y + (a.y - y) * t;
    z = z + (a.z - z) * t;
}

[[nodiscard]] Vec3 Vec3::reflect(const Vec3 &a) const noexcept
{
    Vec3 b = a * 2.0 * dot(a);
    return *this - b;
}

void Vec3::refract(const Vec3 &a, float t) noexcept
{
    Vec3 unit = to_unit_vector();
    float dot = unit.dot(a);
    float disc = 1.f - t * t * (1.0 - dot * dot);
    Vec3 other = a * sqrtf(disc);
    Vec3 ret = (a * dot);
    ret -= a;
    ret = ret * t;
    *this = disc > 0.0f ? ret - other : Vec3();
}

[[nodiscard]] float Vec3::dot(const Vec3 &a) const noexcept
{
    return x * a.x + y * a.y + z * a.z;
}


[[nodiscard]] float Vec3::dist(const Vec3 &a) const noexcept
{
    return sqrtf(powf(x - a.x, 2) + powf(y * a.y, 2) + powf(z * a.z, 2));
}

[[nodiscard]] float Vec3::length() const noexcept
{
    return sqrtf(length_sq());
}

[[nodiscard]] float Vec3::length_sq() const noexcept
{
    return x * x + y * y + z * z;
}

[[nodiscard]] Vec3 Vec3::cross(const Vec3 &a) const noexcept
{
    return Vec3(
            y * a.z - z * a.y,
            z * a.x - x * a.z,
            x * a.y - y * a.x);
}

[[nodiscard]]  Vec3 Vec3::to_unit_vector() const noexcept
{
    float t = length();
    return Vec3(x / t, y / t, z / t);
}
