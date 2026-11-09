// Math3.h: 3D vectors and affine transforms for the EisenScript builder. No Rack dependency.
// A Mat maps p -> m * p + t. State composes in local space: state = state * T (T acts first).
// Rotations, scales, mirrors and matrices act about the unit cube's centre (0.5, 0.5, 0.5), as in Structure Synth.
// Author: Ari Klopfer / A_Development. MIT
#pragma once
#include <cmath>

namespace ss {

struct Vec3
{
    float x = 0.f, y = 0.f, z = 0.f;
    Vec3() {}
    Vec3 (float x_, float y_, float z_) : x (x_), y (y_), z (z_) {}
    Vec3 operator+ (const Vec3& o) const { return Vec3 (x + o.x, y + o.y, z + o.z); }
    Vec3 operator- (const Vec3& o) const { return Vec3 (x - o.x, y - o.y, z - o.z); }
    Vec3 operator* (float k) const { return Vec3 (x * k, y * k, z * k); }
    float dot (const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross (const Vec3& o) const { return Vec3 (y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x); }
    float length() const { return std::sqrt (dot (*this)); }
};

struct Mat
{
    float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    float t[3] = {0, 0, 0};

    static Mat ident() { return Mat(); }

    static Mat translate (float x, float y, float z)
    {
        Mat r;
        r.t[0] = x; r.t[1] = y; r.t[2] = z;
        return r;
    }

    // linear part l (row-major 9) applied about the cube centre: T(c) * L * T(-c)
    static Mat aboutCentre (const float* l)
    {
        Mat r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) r.m[i][j] = l[i * 3 + j];
        for (int i = 0; i < 3; ++i) r.t[i] = 0.5f - 0.5f * (l[i * 3] + l[i * 3 + 1] + l[i * 3 + 2]);
        return r;
    }

    static Mat rot (int axis, float deg)
    {
        const float a = deg * 3.14159265358979f / 180.f, c = std::cos (a), s = std::sin (a);
        float l[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        if (axis == 0) { l[4] = c; l[5] = -s; l[7] = s; l[8] = c; }
        if (axis == 1) { l[0] = c; l[2] = s; l[6] = -s; l[8] = c; }
        if (axis == 2) { l[0] = c; l[1] = -s; l[3] = s; l[4] = c; }
        return aboutCentre (l);
    }

    static Mat scale (float a, float b, float c)
    {
        const float l[9] = {a, 0, 0, 0, b, 0, 0, 0, c};
        return aboutCentre (l);
    }

    static Mat mirror (int axis)
    {
        float l[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        l[axis * 4] = -1;
        return aboutCentre (l);
    }

    Vec3 apply (const Vec3& p) const
    {
        return Vec3 (m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + t[0],
                     m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + t[1],
                     m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + t[2]);
    }

    Vec3 applyDir (const Vec3& p) const
    {
        return Vec3 (m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z,
                     m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z,
                     m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z);
    }

    float det() const
    {
        return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
             + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    // the object's linear size: cube root of the volume scale
    float size() const { return std::cbrt (std::fabs (det())); }
};

// (a * b)(p) = a(b(p))
inline Mat operator* (const Mat& a, const Mat& b)
{
    Mat r;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j) r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
        r.t[i] = a.m[i][0] * b.t[0] + a.m[i][1] * b.t[1] + a.m[i][2] * b.t[2] + a.t[i];
    }
    return r;
}

}   // namespace ss
