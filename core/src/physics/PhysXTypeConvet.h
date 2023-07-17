#pragma once

#include "PxPhysicsAPI.h"
#include "math/Types.h"

namespace pbe {
   using namespace physx;

	vec2 PxVec2ToPBE(const PxVec2& v);
	vec3 PxVec3ToPBE(const PxVec3& v);
	vec4 PxVec4ToPBE(const PxVec4& v);
	quat PxQuatToPBE(const PxQuat& q);

	PxVec2 Vec2ToPx(const vec2& v);
	PxVec3 Vec3ToPx(const vec3& v);
	PxVec4 Vec4ToPx(const vec4& v);
	PxQuat QuatToPx(const quat& q);

}

