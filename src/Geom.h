#pragma once

#include "Shader.h"
#include "defines.h"
#include <vector>
#include <glm/glm.hpp>
#include <assimp/scene.h>

// TODO: remove transparency textures; if you find a transparency texture
// bake it to the alpha channel of the diffuse texture.

namespace Geom
{
	struct Vertex
	{
		glm::vec3 mPosition;
		glm::vec3 mTangent;
		glm::vec3 mBitangent;
		glm::vec3 mNormal;
		glm::vec2 mUV;
	};

	enum class ETextureType
	{
		Diffuse,
		Specular,
		Normal,
		Transparency,
		Unknown,
	};

	struct Texture
	{
						Texture() = default;
						~Texture() = default;

		bool			Load(const char* inFilePath, ETextureType inType);
		void			Unload();

		/**
		 * @brief It is not sufficient to check Mesh::mOpacityTexture is available,
		 * because opacity may be baked in the alpha channel of mDiffuseTexture.
		 */
		bool			mHasTransparency = false;
		u32				mID = UINT32_MAX;
	};

	struct Mesh
	{
		void						Init();
		void						Destroy();
		void						UploadDataGPU();
		void						Draw() const;

		glm::mat4					mTransform;

		const Texture*				mDiffuseTexture	= nullptr;
		const Texture*				mSpecularTexture = nullptr;
		const Texture*				mOpacityTexture = nullptr;
		const Texture*				mNormalTexture = nullptr;

		std::vector<Vertex>			mVertices;
		std::vector<u32>			mIndices;
		u32							mVBO = UINT32_MAX;
		u32							mEBO = UINT32_MAX;
	};

	struct PointLight
	{
		glm::vec3	mPosition	= glm::vec3(0.0f);
		glm::vec3	mColor		= glm::vec3(0.0f);
		f32			mLinear		= 0.0f;
		f32			mQuadratic	= 0.0f;
	};

	class Model final
	{
	public:
									Model() = default;
									~Model() = default;

		bool						Load(const char* inFilePath);
		void						Unload();

		std::vector<Mesh>			mMeshes;
		std::vector<PointLight>		mPointLights;

	private:
		void						parseNodeRecursive(const char* inModelDir, const aiScene* inScene, const aiNode* inNode);
	};

	bool StartUp();
	void ShutDown();
}
