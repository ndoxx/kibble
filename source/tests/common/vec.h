#pragma once

#include <array>
#include <ostream>
#include <cmath>

// Placeholder for a more serious vector class, like the one provided by glm for example.
struct vec2
{
    std::array<float, 2> val;

    vec2() : val{0.f, 0.f}
    {
    }
    vec2(float a) : val{a, a}
    {
    }
    vec2(float a, float b) : val{a, b}
    {
    }

    inline float operator[](size_t idx) const
    {
        return val[idx];
    }

    inline float &operator[](size_t idx)
    {
        return val[idx];
    }

    inline float &x()
    {
        return val[0];
    }

    inline float &y()
    {
        return val[1];
    }

    inline float x() const
    {
        return val[0];
    }

    inline float y() const
    {
        return val[1];
    }

    inline vec2 operator*(float a) const
    {
        return vec2(a * val[0], a * val[1]);
    }

    inline vec2 operator+(const vec2 &rhs) const
    {
        return vec2(val[0] + rhs.val[0], val[1] + rhs.val[1]);
    }

    inline vec2 operator-(const vec2 &rhs) const
    {
        return vec2(val[0] - rhs.val[0], val[1] - rhs.val[1]);
    }

    inline const vec2 &operator+=(const vec2 &rhs)
    {
        val[0] += rhs.val[0];
        val[1] += rhs.val[1];
        return *this;
    }

    inline const vec2 &operator-=(const vec2 &rhs)
    {
        val[0] -= rhs.val[0];
        val[1] -= rhs.val[1];
        return *this;
    }

    inline float norm() const
    {
        return std::sqrt(val[0] * val[0] + val[1] * val[1]);
    }

    inline void normalize()
    {
        float n = norm();
        val[0] /= n;
        val[1] /= n;
    }

    friend vec2 operator*(float, const vec2 &);

    friend std::ostream &operator<<(std::ostream &, const vec2 &);
};

vec2 operator*(float a, const vec2 &vec)
{
    return vec2(a * vec[0], a * vec[1]);
}

std::ostream &operator<<(std::ostream &stream, const vec2 &vec)
{
    stream << '(' << vec.x() << ',' << vec.y() << ')';
    return stream;
}