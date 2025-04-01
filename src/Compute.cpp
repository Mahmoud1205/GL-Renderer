#include "Compute.h"

#include <stb_include.h>
#include <cstdio>
#include <cstdlib>
#include <glad/glad.h>

bool ComputeShader::Load(const char* inPath)
{
	FILE* shaderFD = fopen(inPath, "rb");
	fseek(shaderFD, 0, SEEK_END);
	u32 shaderSize = ftell(shaderFD);
	fseek(shaderFD, 0, SEEK_SET);

	char* shaderSource = (char*)malloc(shaderSize + 1);
	shaderSource[shaderSize] = '\0';
	fread(shaderSource, 1, shaderSize, shaderFD);
	fclose(shaderFD);

	char* included = stb_include_string(shaderSource, nullptr, "res/shaders", "", nullptr);
	free(shaderSource);

	u32 shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &included, nullptr);
	glCompileShader(shader);

	i32 success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		i32 logLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		char* log = (char*)malloc(logLength);
		glGetShaderInfoLog(shader, logLength, &logLength, log);
		printf("Shader compilation failed: %s\n", log);
		free(log);
		free(included);
		return false;
	}

	mID = glCreateProgram();
	glAttachShader(mID, shader);
	glLinkProgram(mID);

	glGetProgramiv(mID, GL_LINK_STATUS, &success);
	if (!success)
	{
		i32 logLength;
		glGetProgramiv(mID, GL_INFO_LOG_LENGTH, &logLength);
		char* log = (char*)malloc(logLength);
		glGetProgramInfoLog(mID, logLength, &logLength, log);
		printf("Program linking failed: %s\n", log);
		free(log);
		free(included);
		return false;
	}

	glDeleteShader(shader);

	return true;
}

void ComputeShader::Unload()
{
	glDeleteProgram(mID);
	mID = UINT32_MAX;
}
