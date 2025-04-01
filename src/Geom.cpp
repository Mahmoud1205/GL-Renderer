#include "Geom.h"

#include "ResourceManager.h"
#include "Utils.h"
#include <glad/glad.h>
#include <unordered_map>
#include <functional>
#include <string>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/integer.hpp>
#include <tracy/Tracy.hpp>

using namespace Geom;
namespace fs = std::filesystem;

struct AttenuationData
{
	// this field is not necessary but good for debugging
	u32 mDistance	= 0;
	f32 mLinear		= 0.0f;
	f32 mQuadratic	= 0.0f;
};

static struct
{
	u32 mDiffuseMapFallback		= 0;
	u32 mOpacityMapFallback		= 0;
	u32 mSpecularMapFallback	= 0;
	u32 mNormalMapFallback		= 0;

	// https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
	AttenuationData	mAttenuationMap[12] = {
		// dst	linear		quadratic
		{ 7,	0.7f,		1.8f		}, //  0
		{ 13,	0.35f,		0.44f		}, //  1
		{ 20,	0.22f,		0.20f		}, //  2
		{ 32,	0.14f,		0.07f		}, //  3
		{ 50,	0.09f,		0.032f		}, //  4
		{ 65,	0.07f,		0.017f		}, //  5
		{ 100,	0.045f,		0.0075f		}, //  6
		{ 160,	0.027f,		0.0028f		}, //  7
		{ 200,	0.022f,		0.0019f		}, //  8
		{ 325,	0.014f,		0.0007f		}, //  9
		{ 600,	0.007f,		0.0002f		}, // 10
		{ 3250,	0.0014f,	0.000007f	}, // 11
	};

	u32 mVAO = UINT32_MAX;
} gState;

static glm::mat4 AssimpToGlm(const aiMatrix4x4& inMat4)
{
	glm::mat4 out{};
	out[0][0] = (f32)inMat4.a1; out[0][1] = (f32)inMat4.b1; out[0][2] = (f32)inMat4.c1; out[0][3] = (f32)inMat4.d1;
	out[1][0] = (f32)inMat4.a2; out[1][1] = (f32)inMat4.b2; out[1][2] = (f32)inMat4.c2; out[1][3] = (f32)inMat4.d2;
	out[2][0] = (f32)inMat4.a3; out[2][1] = (f32)inMat4.b3; out[2][2] = (f32)inMat4.c3; out[2][3] = (f32)inMat4.d3;
	out[3][0] = (f32)inMat4.a4; out[3][1] = (f32)inMat4.b4; out[3][2] = (f32)inMat4.c4; out[3][3] = (f32)inMat4.d4;
	return out;
}

bool Geom::StartUp()
{
	glCreateVertexArrays(1, &gState.mVAO);

	{
		u8 redPixel = 255;
		glCreateTextures(GL_TEXTURE_2D, 1, &gState.mOpacityMapFallback);

		glTextureParameteri(gState.mOpacityMapFallback, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mOpacityMapFallback, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mOpacityMapFallback, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(gState.mOpacityMapFallback, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTextureStorage2D(gState.mOpacityMapFallback, 1, GL_R8, 1, 1);
		glTextureSubImage2D(gState.mOpacityMapFallback, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &redPixel);
	}

	{
		//u8* crazy = (u8*)malloc(1024 * 1024 * 3);
		//memset(crazy, 0, 1024 * 1024 * 3);
		//for (u32 i = 0; i < 1024 * 1024; i++)
		//{
		//	if (i % 4 == 0 || i % 5 == 0 || i % 6 == 0)
		//	{
		//		crazy[i * 3 + 0] = 0;
		//		crazy[i * 3 + 1] = 255;
		//		crazy[i * 3 + 2] = i ? i : 1 / 256;
		//	} else
		//	{
		//		crazy[i * 3 + 0] = 255;
		//		crazy[i * 3 + 1] = 0;
		//		crazy[i * 3 + 2] = 0;
		//	}
		//}

		//glCreateTextures(GL_TEXTURE_2D, 1, &gState.mDiffuseMapFallback);

		//glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		//glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		//glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		//glTextureStorage2D(gState.mDiffuseMapFallback, 1, GL_RGB8, 1024, 1024);
		//glTextureSubImage2D(gState.mDiffuseMapFallback, 0, 0, 0, 1024, 1024, GL_RGB, GL_UNSIGNED_BYTE, crazy);
		//free(crazy);

		u8 whitePixel[3] = {255, 255, 255};
		glCreateTextures(GL_TEXTURE_2D, 1, &gState.mDiffuseMapFallback);

		glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(gState.mDiffuseMapFallback, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTextureStorage2D(gState.mDiffuseMapFallback, 1, GL_RGB8, 1, 1);
		glTextureSubImage2D(gState.mDiffuseMapFallback, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &whitePixel);
	}

	{
		u8 blackPixel = 0;
		glCreateTextures(GL_TEXTURE_2D, 1, &gState.mSpecularMapFallback);

		glTextureParameteri(gState.mSpecularMapFallback, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mSpecularMapFallback, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mSpecularMapFallback, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(gState.mSpecularMapFallback, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTextureStorage2D(gState.mSpecularMapFallback, 1, GL_R8, 1, 1);
		glTextureSubImage2D(gState.mSpecularMapFallback, 0, 0, 0, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &blackPixel);
	}

	{
		u8 unitNormal[3] = { 128, 128, 255 };
		glCreateTextures(GL_TEXTURE_2D, 1, &gState.mNormalMapFallback);

		glTextureParameteri(gState.mNormalMapFallback, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mNormalMapFallback, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(gState.mNormalMapFallback, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(gState.mNormalMapFallback, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTextureStorage2D(gState.mNormalMapFallback, 1, GL_RGB8, 1, 1);
		glTextureSubImage2D(gState.mNormalMapFallback, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &unitNormal);
	}

	return true;
}

void Geom::ShutDown()
{
	glDeleteVertexArrays(1, &gState.mVAO);

	glDeleteTextures(1, &gState.mDiffuseMapFallback);
	glDeleteTextures(1, &gState.mNormalMapFallback);
	glDeleteTextures(1, &gState.mOpacityMapFallback);
	glDeleteTextures(1, &gState.mSpecularMapFallback);
}

void Model::parseNodeRecursive(const char* inModelDir, const aiScene* inScene, const aiNode* inNode)
{
	for (u32 i = 0; i < inNode->mNumMeshes; i++)
	{
		aiMesh* assimpMesh = inScene->mMeshes[inNode->mMeshes[i]];

		mMeshes.emplace_back();
		Mesh& mesh = mMeshes.back();
		mesh.Init();
		mesh.mTransform = AssimpToGlm(inNode->mTransformation);

		// here assume each face has 3 indices cuz assimp
		// triangulates the meshes in Model::Load();
		mesh.mIndices.reserve(assimpMesh->mNumFaces * 3);
		for (u32 j = 0; j < assimpMesh->mNumFaces; j++)
		{
			const aiFace& face = assimpMesh->mFaces[j];
			if (face.mNumIndices != 3)
			{
				// this should never happen
				printf("ERROR: Mesh \"%s\" is not triangulated.\n", assimpMesh->mName.C_Str());
				FATAL();
			}
			for (u32 k = 0; k < face.mNumIndices; k++)
				mesh.mIndices.push_back(face.mIndices[k]);
		}

		mesh.mVertices.resize(assimpMesh->mNumVertices);
		for (u32 j = 0; j < assimpMesh->mNumVertices; j++)
		{
			const aiVector3D& v = assimpMesh->mVertices[j];
			mesh.mVertices[j].mPosition = glm::vec3(v.x, v.y, v.z);

			if (assimpMesh->HasNormals())
			{
				const aiVector3D& n = assimpMesh->mNormals[j];
				mesh.mVertices[j].mNormal = glm::vec3(n.x, n.y, n.z);
			}

			if (assimpMesh->HasTextureCoords(0))
			{
				const aiVector3D& uv = assimpMesh->mTextureCoords[0][j];
				mesh.mVertices[j].mUV = glm::vec2(uv.x, uv.y);
			}

			if (assimpMesh->HasTangentsAndBitangents())
			{
				const aiVector3D& tan = assimpMesh->mTangents[j];
				mesh.mVertices[j].mTangent = glm::vec3(tan.x, tan.y, tan.z);
				const aiVector3D& bitan = assimpMesh->mBitangents[j];
				mesh.mVertices[j].mBitangent = glm::vec3(bitan.x, bitan.z, bitan.y);
			}
		}

		const aiMaterial* mat = inScene->mMaterials[assimpMesh->mMaterialIndex];

		// Note: we dont accept many textures together from
		// the same type in the same material

		u32 diffuseTexCount = mat->GetTextureCount(aiTextureType_DIFFUSE);
		if (diffuseTexCount == 1)
		{
			aiString texPathAssimp;
			mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPathAssimp);

			std::string texPath = inModelDir + std::string(texPathAssimp.C_Str());
			Texture* tex = ResMgr::GetTexture(texPath.c_str(), ETextureType::Diffuse);
			mesh.mDiffuseTexture = tex;
		} else if (diffuseTexCount != 0 && diffuseTexCount > 1)
		{
			SBREAK();
		}

		u32 specularTexCount = mat->GetTextureCount(aiTextureType_SPECULAR);
		if (specularTexCount == 1)
		{
			aiString texPathAssimp;
			mat->GetTexture(aiTextureType_SPECULAR, 0, &texPathAssimp);

			std::string texPath = inModelDir + std::string(texPathAssimp.C_Str());
			Texture* tex = ResMgr::GetTexture(texPath.c_str(), ETextureType::Specular);
			mesh.mSpecularTexture = tex;
		} else if (specularTexCount != 0 && specularTexCount > 1)
		{
			SBREAK();
		}

		u32 normalTexCount = mat->GetTextureCount(aiTextureType_NORMALS);
		if (normalTexCount == 1)
		{
			aiString texPathAssimp;
			mat->GetTexture(aiTextureType_NORMALS, 0, &texPathAssimp);

			std::string texPath = inModelDir + std::string(texPathAssimp.C_Str());
			Texture* tex = ResMgr::GetTexture(texPath.c_str(), ETextureType::Normal);
			mesh.mNormalTexture = tex;
		} else if (normalTexCount != 0 && normalTexCount > 1)
		{
			SBREAK();
		}

		u32 opacityTexCount = mat->GetTextureCount(aiTextureType_OPACITY);
		if (opacityTexCount == 1)
		{
			aiString texPathAssimp;
			mat->GetTexture(aiTextureType_OPACITY, 0, &texPathAssimp);

			std::string texPath = inModelDir + std::string(texPathAssimp.C_Str());
			Texture* tex = ResMgr::GetTexture(texPath.c_str(), ETextureType::Transparency);
			mesh.mOpacityTexture = tex;
		} else if (opacityTexCount != 0 && opacityTexCount > 1)
		{
			SBREAK();
		}

		mesh.UploadDataGPU();
	}

	for (u32 i = 0; i < inNode->mNumChildren; i++)
		parseNodeRecursive(inModelDir, inScene, inNode->mChildren[i]);
}

bool Model::Load(const char* inFilePath)
{
	if (!inFilePath)
	{
		printf("ERROR: Model file path is null.\n");
		return false;
	}

	std::string filePath = fs::absolute(inFilePath).string();
	std::replace(filePath.begin(), filePath.end(), '\\', '/');

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_FixInfacingNormals |
		aiProcess_GenNormals |
		aiProcess_GenBoundingBoxes |
		aiProcess_CalcTangentSpace
	);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		fprintf(stderr, "ERROR: Failed to load file. \"%s\"\n", importer.GetErrorString());
		return false;
	}

	std::string modelDir = filePath.substr(0, filePath.find_last_of('/'));
	modelDir += '/';
	parseNodeRecursive(modelDir.c_str(), scene, scene->mRootNode);

	printf("aiLight count %u\n", scene->mNumLights);
	for (u32 i = 0; i < scene->mNumLights; i++)
	{
		aiLight* light = scene->mLights[i];

		// lights from any kind mustnt have a parent node;
		// they must be in the root node of the scene
		glm::mat4 lightTransform(1.0f);
		for (u32 i = 0; i < scene->mRootNode->mNumChildren; i++)
		{
			const aiNode* node = scene->mRootNode->mChildren[i];
			if (node->mName == light->mName)
			{
				lightTransform = AssimpToGlm(node->mTransformation);
				break;
			}
		}

		if (light->mType == aiLightSource_POINT)
		{
			glm::vec3 pos = glm::vec3(
				light->mPosition.x,
				light->mPosition.y,
				light->mPosition.z
			);

			pos = lightTransform * glm::vec4(pos, 1.0f);

			glm::vec3 col = glm::vec3(
				light->mColorDiffuse.r,
				light->mColorDiffuse.g,
				light->mColorDiffuse.b
			);

			f32 attLinear = 0.0f;
			f32 attQuad = 0.0f;

			if (
				strncmp("RedLight",		light->mName.C_Str(), 8)	== 0 ||
				strncmp("GreenLight",	light->mName.C_Str(), 10)	== 0
			)
			{
				col			= glm::normalize(col) * 7.2f;
				attLinear	= gState.mAttenuationMap[0].mLinear * 10.0f;
				attQuad		= gState.mAttenuationMap[0].mQuadratic * 10.0f;
			} else if (strncmp("StreetLamp", light->mName.C_Str(), 10) == 0)
			{
				// i removed the street lamps cuz they look bad in the day
				continue;
				col			= glm::normalize(col) * 37.0f;
				attLinear	= gState.mAttenuationMap[0].mLinear;
				attQuad		= gState.mAttenuationMap[0].mQuadratic;
			} else
			{
				col			= glm::normalize(col) * 125.0f;
				attLinear	= gState.mAttenuationMap[0].mLinear;
				attQuad		= gState.mAttenuationMap[0].mQuadratic;
			}

			mPointLights.push_back({
				.mPosition = pos,
				.mColor = col,
				.mLinear = attLinear,
				.mQuadratic = attQuad,
			});

			// printf(
			// 	"Light: [\n"
			// 	"\tName:      %s\n"
			// 	"\tPosition:  (%.2f, %.2f, %.2f)\n"
			// 	"\tConstant:  %.2f\n"
			// 	"\tLinear:    %.2f\n"
			// 	"\tQuadratic: %.2f\n"
			// 	"]\n",
			// 	light->mName.C_Str(),
			// 	pos.x, pos.y, pos.z,
			// 	light->mAttenuationConstant,
			// 	light->mAttenuationLinear,
			// 	light->mAttenuationQuadratic
			// );
		} else
		{
			puts("WARN: Found a non point light source, skipping it.");
		}
	}

	printf("%zu lights\n", mPointLights.size());

	return true;
}

void Model::Unload()
{
	for (u32 i = 0; i < mMeshes.size(); i++)
		mMeshes[i].Destroy();
}

void Mesh::Init()
{
	if (mVBO != UINT32_MAX || mEBO != UINT32_MAX)
	{
		puts("ERROR(Mesh): Mesh is already initialized.");
		FATAL();
		return;
	}

	glCreateBuffers(1, &mVBO);
	glCreateBuffers(1, &mEBO);

	glVertexArrayVertexBuffer(gState.mVAO, 0, mVBO, 0, sizeof(Vertex));
	glVertexArrayElementBuffer(gState.mVAO, mEBO);
}

void Mesh::Destroy()
{
	if (mVBO == UINT32_MAX || mEBO == UINT32_MAX)
	{
		puts("ERROR(Mesh): Mesh is not initialized.");
		FATAL();
		return;
	}

	mIndices.clear();
	mVertices.clear();

	if (mDiffuseTexture)
		ResMgr::ReleaseTexture(mDiffuseTexture);
	if (mSpecularTexture)
		ResMgr::ReleaseTexture(mSpecularTexture);
	if (mOpacityTexture)
		ResMgr::ReleaseTexture(mOpacityTexture);
	if (mNormalTexture)
		ResMgr::ReleaseTexture(mNormalTexture);

	mDiffuseTexture = nullptr;
	mSpecularTexture = nullptr;
	mOpacityTexture = nullptr;
	mNormalTexture = nullptr;

	glDeleteBuffers(1, &mVBO);
	glDeleteBuffers(1, &mEBO);
	mVBO = UINT32_MAX;
	mEBO = UINT32_MAX;
}

void Mesh::UploadDataGPU()
{
	if (mVBO == UINT32_MAX || mEBO == UINT32_MAX || mVertices.size() == 0)
	{
		puts("ERROR(Mesh): Mesh is not initialized.");
		FATAL();
		return;
	}

	glVertexArrayVertexBuffer(gState.mVAO, 0, mVBO, 0, sizeof(Vertex));
	glVertexArrayElementBuffer(gState.mVAO, mEBO);

	glNamedBufferData(mVBO, mVertices.size() * sizeof(Vertex), mVertices.data(), GL_STATIC_DRAW);
	glNamedBufferData(mEBO, mIndices.size() * sizeof(u32), mIndices.data(), GL_STATIC_DRAW);

	glEnableVertexArrayAttrib(gState.mVAO, 0);
	glEnableVertexArrayAttrib(gState.mVAO, 1);
	glEnableVertexArrayAttrib(gState.mVAO, 2);
	glEnableVertexArrayAttrib(gState.mVAO, 3);
	glEnableVertexArrayAttrib(gState.mVAO, 4);

	glVertexArrayAttribFormat(gState.mVAO, 0, 3, GL_FLOAT, GL_FALSE, OFFSETOF(Vertex, mPosition));
	glVertexArrayAttribFormat(gState.mVAO, 1, 3, GL_FLOAT, GL_FALSE, OFFSETOF(Vertex, mNormal));
	glVertexArrayAttribFormat(gState.mVAO, 2, 2, GL_FLOAT, GL_FALSE, OFFSETOF(Vertex, mUV));
	glVertexArrayAttribFormat(gState.mVAO, 3, 3, GL_FLOAT, GL_FALSE, OFFSETOF(Vertex, mTangent));
	glVertexArrayAttribFormat(gState.mVAO, 4, 3, GL_FLOAT, GL_FALSE, OFFSETOF(Vertex, mBitangent));

	glVertexArrayAttribBinding(gState.mVAO, 0, 0);
	glVertexArrayAttribBinding(gState.mVAO, 1, 0);
	glVertexArrayAttribBinding(gState.mVAO, 2, 0);
	glVertexArrayAttribBinding(gState.mVAO, 3, 0);
	glVertexArrayAttribBinding(gState.mVAO, 4, 0);
}

void Mesh::Draw() const
{
	ZoneScopedN("Draw Mesh");

	{
		ZoneScopedN("Bind Texture Units");
		if (mDiffuseTexture)
			glBindTextureUnit(0, mDiffuseTexture->mID);
		else
			glBindTextureUnit(0, gState.mDiffuseMapFallback);

		if (mSpecularTexture)
			glBindTextureUnit(1, mSpecularTexture->mID);
		else
			glBindTextureUnit(1, gState.mSpecularMapFallback);

		if (mOpacityTexture)
			glBindTextureUnit(2, mOpacityTexture->mID);
		else
			glBindTextureUnit(2, gState.mOpacityMapFallback);

		if (mNormalTexture)
			glBindTextureUnit(3, mNormalTexture->mID);
		else
			glBindTextureUnit(3, gState.mNormalMapFallback);
	}

	{
		ZoneScopedN("Bind VAO");
		glBindVertexArray(gState.mVAO);
	}

	glVertexArrayVertexBuffer(gState.mVAO, 0, mVBO, 0, sizeof(Vertex));
	glVertexArrayElementBuffer(gState.mVAO, mEBO);

	{
		ZoneScopedN("DrawElements");
		glDrawElements(GL_TRIANGLES, static_cast<u32>(mIndices.size()), GL_UNSIGNED_INT, 0);
	}
}

bool Texture::Load(const char* inFilePath, ETextureType inType)
{
	auto GetInternalTextureFormat = [](i32 inChannelCount) -> u32 {
		if (inChannelCount == 1)
			return GL_R8;
		if (inChannelCount == 2)
			return GL_RG8;
		if (inChannelCount == 3)
			return GL_RGB8;
		if (inChannelCount == 4)
			return GL_RGBA8;

		printf("ERROR: No texture format for %d channels.\n", inChannelCount);
		FATAL();
		return UINT32_MAX;
	};

	auto GetTextureFormat = [](i32 inChannelCount) -> u32 {
		if (inChannelCount == 1)
			return GL_RED;
		if (inChannelCount == 2)
			return GL_RG;
		if (inChannelCount == 3)
			return GL_RGB;
		if (inChannelCount == 4)
			return GL_RGBA;

		printf("ERROR: No texture format for %d channels.\n", inChannelCount);
		FATAL();
		return UINT32_MAX;
	};

	auto GetInternalTextureFormatSRGB = [](i32 inChannelCount) -> u32 {
		if (inChannelCount < 3)
		{
			printf("ERROR: i wont fix this branch until i see it happen.\n");
			FATAL();
			return UINT32_MAX;
		}

		if (inChannelCount == 3)
			return GL_SRGB8;
		if (inChannelCount == 4)
			return GL_SRGB8_ALPHA8;

		printf("ERROR: No texture format for %d channels.\n", inChannelCount);
		FATAL();
		return UINT32_MAX;
	};

	i32 width, height, channelCount;
	u8* data = stbi_load(inFilePath, &width, &height, &channelCount, 0);
	if (!data)
	{
		fprintf(stderr, "ERROR: Failed to load texture '%s'.\n", inFilePath);
		return false;
	}

	u32 mipLevels = 1 + glm::floor(glm::log2(glm::max(width, height)));

	mID = UINT32_MAX;

	glCreateTextures(GL_TEXTURE_2D, 1, &mID);
	glTextureParameteri(mID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(mID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTextureParameteri(mID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTextureParameteri(mID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTextureParameteri(mID, GL_TEXTURE_MAX_ANISOTROPY, 16);

	if (inType == ETextureType::Diffuse)
	{
		if (channelCount == 4)
		{
			mHasTransparency = true;
		}
		glTextureStorage2D(mID, mipLevels, GetInternalTextureFormatSRGB(channelCount), width, height);
		glTextureSubImage2D(mID, 0, 0, 0, width, height, GetTextureFormat(channelCount), GL_UNSIGNED_BYTE, data);
		goto mip_and_free;
	}

	if (inType == ETextureType::Specular)
	{
		if (channelCount != 1)
		{
			glTextureStorage2D(mID, mipLevels, GetInternalTextureFormatSRGB(channelCount), width, height);
			glTextureSubImage2D(mID, 0, 0, 0, width, height, GetTextureFormat(channelCount), GL_UNSIGNED_BYTE, data);
			goto mip_and_free;
		} else if (channelCount == 1)
		{
			// some specular textures use 1 channel only so it can only have gray specular
			// lighting. this code fixes replicates the r channel to the g and b channels
			// so we can not have red speculars instead of gray

			u8* rgbData = (u8*)malloc((usize)width * (usize)height * 3);
			if (!rgbData)
			{
				printf("ERROR(Texture): Failed to allocate memory for specular texture replication.\n");
				stbi_image_free(data);
				return false;
			}

			for (u32 i = 0; i < width * height; i++)
			{
				rgbData[i * 3 + 0] = data[i];
				rgbData[i * 3 + 1] = data[i];
				rgbData[i * 3 + 2] = data[i];
			}

			glTextureStorage2D(mID, mipLevels, GL_RGB8, width, height);
			glTextureSubImage2D(mID, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);

			free(rgbData);

			goto mip_and_free;
		}
	} else if (inType == ETextureType::Transparency)
	{
		glTextureParameteri(mID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(mID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(mID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(mID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureStorage2D(mID, mipLevels, GetInternalTextureFormat(channelCount), width, height);
		glTextureSubImage2D(mID, 0, 0, 0, width, height, GetTextureFormat(channelCount), GL_UNSIGNED_BYTE, data);
		goto mip_and_free;
	}

	// default texture case
	glTextureStorage2D(mID, mipLevels, GetInternalTextureFormat(channelCount), width, height);
	glTextureSubImage2D(mID, 0, 0, 0, width, height, GetTextureFormat(channelCount), GL_UNSIGNED_BYTE, data);

mip_and_free:
	glGenerateTextureMipmap(mID);

	stbi_image_free(data);
	return true;
}

void Texture::Unload()
{
	glDeleteTextures(1, &mID);
	mID = UINT32_MAX;
}
