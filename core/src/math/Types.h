#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#define M_PI 3.14159265358979323846
#define M_2PI 2 * M_PI

using int2 = glm::ivec2;
using int3 = glm::ivec3;

using glm::vec2;
using glm::vec3;
using glm::vec4;

using glm::mat2;
using glm::mat3;
using glm::mat4;

using glm::quat;

constexpr vec2 Vec2_One  = vec2(1, 1);
constexpr vec2 Vec2_Zero = vec2(0, 0);
constexpr vec2 Vec2_X    = vec2(1, 0);
constexpr vec2 Vec2_XNeg = vec2(-1, 0);
constexpr vec2 Vec2_Y    = vec2(0, 1);
constexpr vec2 Vec2_YNeg = vec2(0, -1);

constexpr vec3 Vec3_One  = vec3(1, 1, 1);
constexpr vec3 Vec3_Zero = vec3(0, 0, 0);
constexpr vec3 Vec3_X    = vec3(1, 0, 0);
constexpr vec3 Vec3_XNeg = vec3(-1, 0, 0);
constexpr vec3 Vec3_Y    = vec3(0, 1, 0);
constexpr vec3 Vec3_YNeg = vec3(0, -1, 0);
constexpr vec3 Vec3_Z    = vec3(0, 0, 1);
constexpr vec3 Vec3_ZNeg = vec3(0, 0, -1);

constexpr vec4 Vec4_One  = vec4(1, 1, 1, 1);
constexpr vec4 Vec4_Zero = vec4(0, 0, 0, 0);
constexpr vec3 Vec4_X    = vec4(1, 0, 0, 0);
constexpr vec3 Vec4_XNeg = vec4(-1, 0, 0, 0);
constexpr vec3 Vec4_Y    = vec4(0, 1, 0, 0);
constexpr vec3 Vec4_YNeg = vec4(0, -1, 0, 0);
constexpr vec3 Vec4_Z    = vec4(0, 0, 1, 0);
constexpr vec3 Vec4_ZNeg = vec4(0, 0, -1, 0);
constexpr vec3 Vec4_W    = vec4(0, 0, 0, 1);
constexpr vec3 Vec4_WNeg = vec4(0, 0, 0, -1);

constexpr quat Quat_Identity = quat(1, 0, 0, 0);

template<typename OStream>
OStream& operator<<(OStream& os, const int2& v)
{
   return os << "(" << v.x << ", " << v.y << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const vec2& v)
{
   return os << "(" << v.x << ", " << v.y << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const vec3& v)
{
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

template<typename OStream>
OStream& operator<<(OStream& os, const vec4& v)
{
   return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
}

