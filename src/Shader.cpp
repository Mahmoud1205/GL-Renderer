#include "Shader.h"

#include "Utils.h"
#define STB_INCLUDE_IMPLEMENTATION
#define STB_INCLUDE_LINE_GLSL
#include <stb_include.h>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

bool Shader::Load(const char* inVertexPath, const char* inFragmentPath, const char* inGeometryPath)
{
	ZR_ASSERT(mID == UINT32_MAX, "");

	i32		success;
	char	infoLog[512];
	u32		fragShader = UINT32_MAX;
	u32		vertShader = UINT32_MAX;
	u32		geomShader = UINT32_MAX;

	// vertex shader
	{
		FILE* vertFd = fopen(inVertexPath, "rb");
		if (!vertFd)
		{
			printf("ERROR: Failed to open shader \"%s\".", inVertexPath);
			FATAL();
			return false;
		}

		fseek(vertFd, 0, SEEK_END);
		u32 vertSize = (u32)ftell(vertFd);
		fseek(vertFd, 0, SEEK_SET);

		char* vertSrc = (char*)malloc(vertSize + 1);
		if (!vertSrc)
		{
			printf("ERROR: Failed to allocate %u bytes for shader source.\n", vertSize);
			FATAL();
			return false;
		}

		vertSrc[vertSize] = '\0';
		usize bytesRead = fread(vertSrc, 1, vertSize, vertFd);
		if (bytesRead != vertSize)
		{
			puts("ERROR: Failed to read full bytes of vertex shader.\n");
			FATAL();
			return false;
		}

		char* included = stb_include_string(vertSrc, nullptr, "res/shaders", "", nullptr);
		if (!included)
		{
			puts("ERROR: Failed to allocate memory for included shader source.");
			FATAL();
			return false;
		}

		free(vertSrc);

		vertShader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertShader, 1, &included, NULL);
		glCompileShader(vertShader);
		glGetShaderiv(vertShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(vertShader, 512, NULL, infoLog);
			glDeleteShader(vertShader);
			fprintf(stderr, "Error: Vertex shader compilation failed,\n%s\n", infoLog);
			SBREAK();
			free(included);
			fclose(vertFd);
			return false;
		}

		free(included);
		fclose(vertFd);
	}

	// fragment shader
	{
		FILE* fragFd = fopen(inFragmentPath, "rb");
		if (!fragFd)
		{
			printf("ERROR: Failed to open shader \"%s\".", inVertexPath);
			FATAL();
			return false;
		}

		fseek(fragFd, 0, SEEK_END);
		u32 fragSize = (u32)ftell(fragFd);
		fseek(fragFd, 0, SEEK_SET);

		char* fragSrc = (char*)malloc(fragSize + 1);
		if (!fragSrc)
		{
			printf("ERROR: Failed to allocate %u bytes for shader source.\n", fragSize);
			FATAL();
			return false;
		}

		fragSrc[fragSize] = '\0';
		usize bytesRead = fread(fragSrc, 1, fragSize, fragFd);
		if (bytesRead != fragSize)
		{
			puts("ERROR: Failed to read full bytes of fragment shader.\n");
			FATAL();
			return false;
		}

		char* included = stb_include_string(fragSrc, nullptr, "res/shaders", "", nullptr);
		if (!included)
		{
			puts("ERROR: Failed to allocate memory for included shader source.");
			FATAL();
			return false;
		}

		free(fragSrc);

		fragShader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragShader, 1, &included, NULL);
		glCompileShader(fragShader);
		glGetShaderiv(fragShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(fragShader, 512, NULL, infoLog);
			glDeleteShader(vertShader);
			glDeleteShader(fragShader);
			fprintf(stderr, "Error: Fragment shader compilation failed,\n%s\n", infoLog);
			SBREAK();
			free(included);
			fclose(fragFd);
			return false;
		}

		free(included);
		fclose(fragFd);
	}

	// geom shader
	if (inGeometryPath)
	{
		FILE* geomFd = fopen(inGeometryPath, "rb");
		if (!geomFd)
		{
			printf("ERROR: Failed to open shader \"%s\".", inVertexPath);
			FATAL();
			return false;
		}

		fseek(geomFd, 0, SEEK_END);
		u32 geomSize = (u32)ftell(geomFd);
		fseek(geomFd, 0, SEEK_SET);

		char* geomSrc = (char*)malloc(geomSize + 1);
		if (!geomSrc)
		{
			printf("ERROR: Failed to allocate %u bytes for shader source.\n", geomSize);
			FATAL();
			return false;
		}

		geomSrc[geomSize] = '\0';
		usize bytesRead = fread(geomSrc, 1, geomSize, geomFd);
		if (bytesRead != geomSize)
		{
			puts("ERROR: Failed to read full bytes of geometry shader.\n");
			FATAL();
			return false;
		}

		char* included = stb_include_string(geomSrc, nullptr, "res/shaders", "", nullptr);
		if (!included)
		{
			puts("ERROR: Failed to allocate memory for included shader source.");
			FATAL();
			return false;
		}

		free(geomSrc);

		geomShader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geomShader, 1, &included, NULL);
		glCompileShader(geomShader);
		glGetShaderiv(geomShader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(geomShader, 512, NULL, infoLog);
			glDeleteShader(vertShader);
			glDeleteShader(fragShader);
			glDeleteShader(geomShader);
			fprintf(stderr, "Error: Geometry shader compilation failed,\n%s\n", infoLog);
			SBREAK();
			free(included);
			fclose(geomFd);
			return false;
		}

		free(included);
		fclose(geomFd);
	}

	mID = glCreateProgram();
	glAttachShader(mID, vertShader);
	glAttachShader(mID, fragShader);
	if (inGeometryPath)
		glAttachShader(mID, geomShader);
	
	glLinkProgram(mID);

	glDeleteShader(vertShader);
	glDeleteShader(fragShader);
	if (inGeometryPath)
		glDeleteShader(geomShader);

	glGetProgramiv(mID, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(mID, 512, NULL, infoLog);
		fprintf(stderr, "Error: Shader program linking failed,\n%s\n", infoLog);
		mID = UINT32_MAX;
		return false;
	}

	return true;
}

void Shader::Unload()
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glDeleteProgram(mID);
	mID = UINT32_MAX;
}

void Shader::Use() const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glUseProgram(mID);
}

void Shader::SetMat4(const std::string& inUniformName, const glm::mat4& inMat4) const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glProgramUniformMatrix4fv(mID, glGetUniformLocation(mID, inUniformName.c_str()), 1, GL_FALSE, glm::value_ptr(inMat4));
}
void Shader::SetVec3(const std::string& inUniformName, const glm::vec3& inVec3) const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glProgramUniform3fv(mID, glGetUniformLocation(mID, inUniformName.c_str()), 1, glm::value_ptr(inVec3));
}
void Shader::SetVec4(const std::string& inUniformName, const glm::vec4& inVec4) const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glProgramUniform4fv(mID, glGetUniformLocation(mID, inUniformName.c_str()), 1, glm::value_ptr(inVec4));
}
void Shader::SetInt(const std::string& inUniformName, i32 inInt) const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glProgramUniform1i(mID, glGetUniformLocation(mID, inUniformName.c_str()), inInt);
}
void Shader::SetUint(const std::string& inUniformName, u32 inUint) const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glProgramUniform1ui(mID, glGetUniformLocation(mID, inUniformName.c_str()), inUint);
}
void Shader::SetFloat(const std::string& inUniformName, f32 inFloat) const
{
	ZR_ASSERT(mID != UINT32_MAX, "");
	glProgramUniform1f(mID, glGetUniformLocation(mID, inUniformName.c_str()), inFloat);
}
