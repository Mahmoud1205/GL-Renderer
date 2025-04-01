#include "Environment.h"

#include <glad/glad.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

bool Skybox::StartUpSystem()
{
	f32 skyboxVertices[] = {
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	if (!sSkyboxShader.Load("res/shaders/Skybox.vert", "res/shaders/Skybox.frag"))
		return false;

	glCreateVertexArrays(1, &sVAO);
	glCreateBuffers(1, &sVBO);

	glVertexArrayVertexBuffer(sVAO, 0, sVBO, 0, 3 * sizeof(f32));

	glNamedBufferData(sVBO, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexArrayAttrib(sVAO, 0);
	glVertexArrayAttribFormat(sVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(sVAO, 0, 0);

	return true;
}

void Skybox::ShutDownSystem()
{
	glDeleteBuffers(1, &sVBO);
	glDeleteVertexArrays(1, &sVAO);
	sSkyboxShader.Unload();
}

void Skybox::UpdateProjection(const glm::mat4& inProjection)
{
	sSkyboxShader.Use();
	sSkyboxShader.SetMat4("uProjection", inProjection);
}

bool Skybox::Load(const char* inBasePath, const char* inExtension)
{
	glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &mID);

	i32 width, height, channelCount;

	{
		i32 infoRes = stbi_info(
			(std::string(inBasePath).c_str() + std::string("posx") + std::string(inExtension)).c_str(),
			&width, &height, &channelCount
		);

		if (infoRes != 1)
		{
			printf("ERROR(Skybox): Failed to find image information of +X cubemap face.\n");
			return false;
		}
	}

	// because the target mID type is GL_TEXTURE_CUBE_MAP, opengl automatically
	// allocates 6 faces. each size (width x height) and GL_RGB8 format and 1 mip
	glTextureStorage2D(mID, 1, GL_RGB8, width, height);

	for (u32 i = 0; i < 6; i++)
	{
		std::string sideExt = "";
		switch (i)
		{
		case 0: sideExt = "posx" + std::string(inExtension); break;
		case 1: sideExt = "negx" + std::string(inExtension); break;
		case 2: sideExt = "posy" + std::string(inExtension); break;
		case 3: sideExt = "negy" + std::string(inExtension); break;
		case 4: sideExt = "posz" + std::string(inExtension); break;
		case 5: sideExt = "negz" + std::string(inExtension); break;
		default: break; // this will never ever happen
		}

		u8* data = nullptr;
		std::string sidePath = std::string(inBasePath) + sideExt;

		i32 faceWidth, faceHeight, faceChannelCount;
		data = stbi_load(sidePath.c_str(), &faceWidth, &faceHeight, &faceChannelCount, 0);

		if (!data)
		{
			fprintf(stderr, "ERROR(Skybox): Failed to load skybox side %s. \"%s\"\n", sidePath.c_str(), stbi_failure_reason());
			return false;
		}

		if (faceWidth != width || faceHeight != height || faceChannelCount != channelCount)
		{
			printf("ERROR(Skybox): Face %u width, height, or channel count doesn't match that of face +X.\n", i);
			return false;
		}

		glTextureSubImage3D(mID, 0, 0, 0, i, faceWidth, faceHeight, 1, GL_RGB, GL_UNSIGNED_BYTE, data);

		stbi_image_free(data);
	}

	glTextureParameteri(mID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(mID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(mID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(mID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return true;
}

void Skybox::Unload()
{
	glDeleteTextures(1, &mID);
	mID = UINT32_MAX;
}

void Skybox::Draw(const glm::mat4& inView) const
{
	glDepthFunc(GL_LEQUAL);
	
	sSkyboxShader.Use();
	
	glm::mat4 view = glm::mat4(glm::mat3(inView));
	sSkyboxShader.SetMat4("uView", view);

	glBindVertexArray(sVAO);
	glBindTextureUnit(1, mID);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glDepthFunc(GL_LESS);
}
