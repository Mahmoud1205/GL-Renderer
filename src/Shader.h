#pragma once

#include "defines.h"
#include <glm/glm.hpp>
#include <string>

class Shader final
{
public:
			Shader() = default;
			~Shader() = default;

	/// @brief The Geometry shader is not enabled if `inGeometryPath` is nullptr.
	bool	Load(const char* inVertexPath, const char* inFragmentPath, const char* inGeometryPath = nullptr);
	void	Unload();

	void	Use() const;
	void	SetMat4(const std::string& inUniformName, const glm::mat4& inMat4) const;
	void	SetVec3(const std::string& inUniformName, const glm::vec3& inVec3) const;
	void	SetVec4(const std::string& inUniformName, const glm::vec4& inVec4) const;
	void	SetInt(const std::string& inUniformName, i32 inInt) const;
	void	SetUint(const std::string& inUniformName, u32 inUint) const;
	void	SetFloat(const std::string& inUniformName, f32 inFloat) const;

	u32		mID = UINT32_MAX;
};
