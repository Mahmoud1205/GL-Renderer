#include "DebugDraw.h"

#include "Memory.h"
#include "Utils.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

DebugDraw gDebugDraw;

bool DebugDraw::StartUp()
{
	glCreateVertexArrays(1, &mVAO);
	glCreateBuffers(1, &mVBO);
	glVertexArrayVertexBuffer(mVAO, 0, mVBO, 0, 3 * sizeof(f32));

	glEnableVertexArrayAttrib(mVAO, 0);
	glVertexArrayAttribFormat(mVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(mVAO, 0, 0);

	if (!mShader.Load("res/shaders/DebugDraw.vert", "res/shaders/DebugDraw.frag"))
		return false;

	return true;
}

void DebugDraw::ShutDown()
{
	glDeleteBuffers(1, &mVBO);
	glDeleteVertexArrays(1, &mVAO);
	mVBO = 0;
	mVAO = 0;
	mDrawables.clear();

	mShader.Unload();
}

void DebugDraw::AddLine(const glm::vec3& inFrom, const glm::vec3& inTo, const glm::vec4& inColor)
{
	Drawable d;
	d.mColor = inColor;
	d.mMode = GL_LINES;
	d.mVerts.push_back(inFrom);
	d.mVerts.push_back(inTo);

	mDrawables.push_back(d);
}

void DebugDraw::AddArrow(const glm::vec3& inFrom, const glm::vec3& inTo, f32 inSize, const glm::vec4& inColor)
{
	// https://github.com/jrouwe/JoltPhysics/blob/master/Jolt/Renderer/DebugRenderer.cpp#L184

	f32 arrowSize = inSize * 0.1f;

	// arrow base
	AddLine(inFrom, inTo, inColor);

	glm::vec3 dir = inTo - inFrom;
	f32 len = glm::length(dir);
	if (len != 0.0f)
		dir = dir * (arrowSize / len);
	else
		dir = glm::vec3(arrowSize, 0.0f, 0.0f);

	glm::vec3 perp;

	glm::vec3 cross = glm::cross(glm::normalize(dir), glm::vec3(0.0f, 1.0f, 0.0f));
	f32 crossLen = glm::length(cross);

	if (crossLen > 0.0f)
	{
		// if cross product is not parallel with the direction
		perp = arrowSize * glm::normalize(cross);
	} else
	{
		// if cross product is parallel with the direction. use (1,1,1) as a default axis
		perp = arrowSize * glm::normalize(glm::cross(glm::normalize(dir), glm::vec3(1.0f)));
	}

	glm::vec3 tip1From = inTo - dir + perp;
	glm::vec3 tip1To = inTo;

	glm::vec3 tip2From = inTo - dir - perp;
	glm::vec3 tip2To = inTo;

	AddLine(tip1From, tip1To, inColor);
	AddLine(tip2From, tip2To, inColor);
}

void DebugDraw::AddTriangle(const glm::vec3& inV0, const glm::vec3& inV1, const glm::vec3& inV2, const glm::vec4& inColor)
{
	Drawable d;
	d.mColor = inColor;
	d.mMode = GL_TRIANGLES;
	d.mVerts.push_back(inV0);
	d.mVerts.push_back(inV1);
	d.mVerts.push_back(inV2);

	mDrawables.push_back(d);
}

void DebugDraw::AddWireTriangle(const glm::vec3& inV0, const glm::vec3& inV1, const glm::vec3& inV2, const glm::vec4& inColor)
{
	Drawable d;
	d.mColor = inColor;
	d.mMode = GL_LINES;

	d.mVerts.push_back(inV0);
	d.mVerts.push_back(inV1);

	d.mVerts.push_back(inV1);
	d.mVerts.push_back(inV2);

	d.mVerts.push_back(inV2);
	d.mVerts.push_back(inV0);

	mDrawables.push_back(d);
}

void DebugDraw::AddCoordinateSystem(const glm::mat4& inTransform)
{
	glm::vec3 center(0.0f);
	glm::vec3 x(1.0f, 0.0f, 0.0f);
	glm::vec3 y(0.0f, 1.0f, 0.0f);
	glm::vec3 z(0.0f, 0.0f, 1.0f);

	center = inTransform * glm::vec4(center, 1.0f);
	x = inTransform * glm::vec4(x, 1.0f);
	y = inTransform * glm::vec4(y, 1.0f);
	z = inTransform * glm::vec4(z, 1.0f);

	AddArrow(center, x, 1.0f, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	AddArrow(center, y, 1.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
	AddArrow(center, z, 1.0f, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

void DebugDraw::DrawFrame(const Camera& inCamera)
{
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	mShader.Use();
	mShader.SetMat4("uView", inCamera.mView);
	mShader.SetMat4("uProjection", inCamera.mProjection);
	mShader.SetMat4("uModel", glm::mat4(1.0f));

	glBindVertexArray(mVAO);
	for (i32 i = mDrawables.size() - 1; i >= 0; i--)
	{
		Drawable* d = &mDrawables[i];

		mShader.SetVec4("uColor", d->mColor);
		glNamedBufferData(mVBO, d->mVerts.size() * 3 * sizeof(f32), d->mVerts.data(), GL_DYNAMIC_DRAW);
		glDrawArrays(d->mMode, 0, 6);

		if (--d->mLifeTime == 0)
			mDrawables.pop_back();
	}
}
