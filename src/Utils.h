#pragma once

#include "defines.h"
#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

inline JPH::Vec3 GlmToJph(const glm::vec3& inFrom)
{
	return JPH::Vec3(inFrom.x, inFrom.y, inFrom.z);
}

inline glm::vec3 JphToGlm(const JPH::Vec3& inFrom)
{
	return glm::vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
}

namespace Utils
{
	bool	IsDebuggerAttached();
	void	EnableFpeExcept();

	f32		InvertRange(f32 inVal, f32 inRangeStart, f32 inRangeEnd);
	f32		RandomBetween(f32 inMin, f32 inMax);
}

/// @brief Safe Debug Break. Break into the debugger if a debugger is attached.
#define SBREAK() \
	do { \
		if (Utils::IsDebuggerAttached()) __debugbreak(); \
	} while (0)

/// @brief Conditional Debug Break. Call SBREAK() if the expression inExpr is true.
#define CBREAK(inExpr) \
	do { \
		if (inExpr) SBREAK(); \
	} while (0)

/// @brief Call SBREAK() and abort the application.
#define FATAL() \
	do { \
		SBREAK(); \
		abort(); \
	} while (0)
