#pragma once

#include "defines.h"
#include "Shader.h"
#include "Renderer.h"
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

class DebugDraw final
{
	struct Drawable
	{
		/// @brief e.g. GL_TRIANGLES, GL_LINES..
		u32						mMode;
		u32						mLifeTime = 1; ///< In frames
		glm::vec4				mColor;
		std::vector<glm::vec3>	mVerts;
	};

public:
							DebugDraw() = default;
							~DebugDraw() = default;

	bool					StartUp();
	void					ShutDown();

	void					AddLine(const glm::vec3& inFrom, const glm::vec3& inTo, const glm::vec4& inColor);
	void					AddArrow(const glm::vec3& inFrom, const glm::vec3& inTo, f32 inSize, const glm::vec4& inColor);
	void					AddTriangle(const glm::vec3& inV0, const glm::vec3& inV1, const glm::vec3& inV2, const glm::vec4& inColor);
	void					AddWireTriangle(const glm::vec3& inV0, const glm::vec3& inV1, const glm::vec3& inV2, const glm::vec4& inColor);
	void					AddCoordinateSystem(const glm::mat4& inTransform);

	void					DrawFrame(const Camera& inCamera);

	// TODO: if std::vector performance is a problem make a pool allocator
	std::vector<Drawable>	mDrawables{};
	Shader					mShader;

	u32						mVAO = 0;
	u32						mVBO = 0;
};

extern DebugDraw gDebugDraw;
