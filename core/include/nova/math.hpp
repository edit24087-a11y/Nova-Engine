// Nova Engine — core math library (C++17, header-only).
// Vec2/Vec3/Vec4, Mat3/Mat4, Quat, plus scalar helpers.
#pragma once
#include <cmath>
#include <cstdint>

namespace nova {

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 2.0f * PI;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

inline float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }
inline float saturate(float v) { return clamp(v, 0.0f, 1.0f); }
inline float deg2rad(float d) { return d * DEG2RAD; }
inline float rad2deg(float r) { return r * RAD2DEG; }

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    static Vec2 zero() { return Vec2(0.0f, 0.0f); }
    static Vec2 one() { return Vec2(1.0f, 1.0f); }
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
    Vec2 operator-() const { return Vec2(-x, -y); }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }
    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec2 normalized() const { float l = length(); return l > 1e-9f ? Vec2(x / l, y / l) : Vec2::zero(); }
    Vec2 rotated(float deg) const {
        float c = std::cos(deg2rad(deg)), s = std::sin(deg2rad(deg));
        return Vec2(x * c - y * s, x * s + y * c);
    }
    Vec2 perpendicular() const { return Vec2(-y, x); }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    static Vec3 zero() { return Vec3(0.0f, 0.0f, 0.0f); }
    static Vec3 one() { return Vec3(1.0f, 1.0f, 1.0f); }
    static Vec3 up() { return Vec3(0.0f, 1.0f, 0.0f); }
    Vec2 xy() const { return Vec2(x, y); }
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator*(const Vec3& o) const { return Vec3(x * o.x, y * o.y, z * o.z); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3& o) const { return !(*this == o); }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const { float l = length(); return l > 1e-9f ? Vec3(x / l, y / l, z / l) : Vec3::zero(); }
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec3 xyz() const { return Vec3(x, y, z); }
    Vec4 operator*(float s) const { return Vec4(x * s, y * s, z * s, w); }
};

// Column-major 4x4 matrix (matches OpenGL).
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    Mat4() = default;
    static Mat4 identity() { return Mat4(); }
    static Mat4 ortho(float l, float r, float b, float t, float n, float f) {
        Mat4 out;
        out.m[0] = 2.0f / (r - l);
        out.m[5] = 2.0f / (t - b);
        out.m[10] = -2.0f / (f - n);
        out.m[12] = -(r + l) / (r - l);
        out.m[13] = -(t + b) / (t - b);
        out.m[14] = -(f + n) / (f - n);
        out.m[15] = 1.0f;
        return out;
    }
    static Mat4 perspective(float fovyDeg, float aspect, float n, float f) {
        float s = 1.0f / std::tan(deg2rad(fovyDeg) * 0.5f);
        Mat4 out;
        out.m[0] = s / aspect;
        out.m[5] = s;
        out.m[10] = (f + n) / (n - f);
        out.m[11] = -1.0f;
        out.m[14] = (2.0f * f * n) / (n - f);
        return out;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);
        Mat4 out;
        out.m[0] = s.x; out.m[1] = s.y; out.m[2] = s.z; out.m[3] = 0.0f;
        out.m[4] = u.x; out.m[5] = u.y; out.m[6] = u.z; out.m[7] = 0.0f;
        out.m[8] = -f.x; out.m[9] = -f.y; out.m[10] = -f.z; out.m[11] = 0.0f;
        out.m[12] = -s.dot(eye);
        out.m[13] = -u.dot(eye);
        out.m[14] = f.dot(eye);
        out.m[15] = 1.0f;
        return out;
    }
    static Mat4 translation(const Vec3& t) {
        Mat4 out;
        out.m[12] = t.x; out.m[13] = t.y; out.m[14] = t.z;
        return out;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 out;
        out.m[0] = s.x; out.m[5] = s.y; out.m[10] = s.z;
        return out;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 out;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                out.m[c * 4 + r] =
                    m[c * 4 + 0] * o.m[0 * 4 + r] + m[c * 4 + 1] * o.m[1 * 4 + r] +
                    m[c * 4 + 2] * o.m[2 * 4 + r] + m[c * 4 + 3] * o.m[3 * 4 + r];
            }
        }
        return out;
    }
    Vec3 transformPoint(const Vec3& p) const {
        float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
        float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
        float z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
        return Vec3(x, y, z);
    }
};

struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Quat() = default;
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    static Quat fromAxisAngle(const Vec3& axis, float deg) {
        float a = deg2rad(deg) * 0.5f;
        float s = std::sin(a);
        Vec3 n = axis.normalized();
        return Quat(n.x * s, n.y * s, n.z * s, std::cos(a));
    }
    static Quat fromEuler(const Vec3& eulerDeg) {
        Quat qx = fromAxisAngle(Vec3(1, 0, 0), eulerDeg.x);
        Quat qy = fromAxisAngle(Vec3(0, 1, 0), eulerDeg.y);
        Quat qz = fromAxisAngle(Vec3(0, 0, 1), eulerDeg.z);
        return (qx * qy) * qz;
    }
    Quat operator*(const Quat& o) const {
        return Quat(
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z);
    }
    Vec3 rotate(const Vec3& v) const {
        Vec3 qv(x, y, z);
        Vec3 uv = qv.cross(v);
        Vec3 uuv = qv.cross(uv);
        return v + (uv * w + uuv) * 2.0f;
    }
    Mat4 toMat4() const {
        Mat4 out;
        float xx = x * x, yy = y * y, zz = z * z;
        float xy = x * y, xz = x * z, yz = y * z;
        float wx = w * x, wy = w * y, wz = w * z;
        out.m[0] = 1 - 2 * (yy + zz); out.m[1] = 2 * (xy + wz);     out.m[2] = 2 * (xz - wy);     out.m[3] = 0;
        out.m[4] = 2 * (xy - wz);     out.m[5] = 1 - 2 * (xx + zz); out.m[6] = 2 * (yz + wx);     out.m[7] = 0;
        out.m[8] = 2 * (xz + wy);     out.m[9] = 2 * (yz - wx);     out.m[10] = 1 - 2 * (xx + yy); out.m[11] = 0;
        out.m[15] = 1;
        return out;
    }
};

} // namespace nova
