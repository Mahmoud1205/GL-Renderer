#pragma once

#include "defines.h"
#include "Geom.h"
#include "Environment.h"
#include "Compute.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifndef ZR_DISTRIBUTION
	#define GL_ZONE(inName) glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, inName)
	#define GL_ZONE_END() glPopDebugGroup()
	#define GL_LABEL(inTarget, inID, inName) glObjectLabel(inTarget, inID, -1, inName)
#else
	#define GL_ZONE(inName)
	#define GL_ZONE_END()
	#define GL_LABEL(inTarget, inID, inName)
#endif

/**
 * @brief The data structure of the SSBO used inside `Luma.comp` and `Exposure.comp`.
 * Note: This must be tightly packed!!!!!
 */
struct LumaExposureComp
{
	u32 mTotalLuma = 0.0f;
	f32 mDesiredExposure = 0.0f;
	f32 mExposure = 0.0f;
};

struct BloomMip
{
	glm::vec2	mSize;
	u32			mID;
};

constexpr f32 kNearPlane = 0.1f;
constexpr f32 kFarPlane = 1000.0f;

constexpr f32 kMinCameraFOV = 5.0f;
constexpr f32 kMaxCameraFOV = 75.0f;

struct Camera
{
	void		UpdateProjection(u32 inWidth, u32 inHeight)
	{
		mProjection = glm::perspective(
			glm::radians(mFOV),
			static_cast<f32>(inWidth) / static_cast<f32>(inHeight),
			kNearPlane, kFarPlane
		);
	}

	glm::vec3	mPos		= glm::vec3(0.0f, 0.0f, 4.0f);
	glm::vec3	mFront		= glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3	mUp			= glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4	mView		= glm::mat4(1.0f);
	glm::mat4	mProjection	= glm::mat4(1.0f);
	f32			mYaw		= -90.0f;
	f32			mPitch		= 0.0f;
	f32			mFOV		= kMaxCameraFOV; /// @brief In degrees.
};

class Renderer final
{
public:
											Renderer() = default;
											~Renderer() = default;

	bool									StartUp(u32 inWidth, u32 inHeight, GLFWwindow* ioWindow);
	void									ShutDown();

	void									SetCamera(Camera inCamera);

	/**
	 * @brief Before calling. The camera projection must be updated and
	 * Renderer::SetCamera() must be called on the new updated camera copy.
	 */
	void									OnResize(u32 inWidth, u32 inHeight);

	/// @brief ImGui commands must go between BeginUI() and Render().
	void									BeginUI();
	void									Render(f32 inDeltaTime, f32 inCurrentTime);
	struct
	{
		bool								mEnableFXAA = true;
		bool								mEnableSSAO = true;
		bool								mEnableCLUT = false;
		bool								mEnableAutoExposure = true;
	} mSettings;

	u32										mWidth = 0;
	u32										mHeight = 0;

	std::vector<const Geom::Mesh*>			mMeshes;
	std::vector<const Geom::PointLight*>	mPointLights;
	Camera									mCamera;
	Geom::Texture*							mLensDirtTexture = nullptr;
	Shader									mDeferredShader;
	Shader									mPostFxShader;
	Shader									mFxaaShader;
	Shader									mLightingShader;
	Shader									mSsaoShader;
	Shader									mSsaoBlurShader;
	Shader									mBloomUpsampleShader;
	Shader									mBloomDownsampleShader;
	Shader									mShadowMapShader;
	Shader									mTransparentShader;
	Shader									mFullScreenShader;
	ComputeShader							mLumaShader;
	ComputeShader							mExposureShader;
	Skybox									mSkybox;
	u32										mFullScreenQuadVAO = 0;
	u32										mFullScreenQuadVBO = 0;

	u32										mAlbedoTex = 0;
	u32										mSpecularTex = 0;
	u32										mNormalTex = 0;
	u32										mPositionTex = 0;
	u32										mDepthRBO = 0;
	u32										mDeferredFBO = 0;
	u32										mLightingFBO = 0;
	u32										mLightingTex = 0;
	u32										mHdrFBO = 0;
	u32										mHdrTex = 0;
	u32										mSsaoFBO = 0;
	u32										mSsaoTex = 0;
	u32										mSsaoBlurFBO = 0;
	u32										mSsaoBlurTex = 0;
	u32										mSsaoNoiseTex = 0;
	u32										mFxaaFBO = 0;
	u32										mFxaaTex = 0;
	u32										mBloomFBO = 0;
	u32										mShadowMapFBO = 0;
	u32										mCascadeTexArray = 0;
	u32										mClutTex = 0;
	std::vector<BloomMip>					mBloomMipChain;

	std::vector<const Geom::Mesh*>			mTransparentMeshes;

	u32										mLumaSSBO = 0;


	glm::vec4								mClearColor = glm::vec4(0.1f, 0.14f, 0.21f, 1.0f);

	/// ****************** CONSTANTS ****************** ///
	sconst u32								kShadowQuality = 1024 * 4;
	sconst u32								kFrustumCount = 3;
	sconst u32								kCascadeCount = kFrustumCount + 1;
	sconst glm::vec3						kSunDirection = glm::vec3(-0.2f, -1.0f, -0.2f);

private:
	glm::mat4								getLightSpaceMatrix(f32 inNear, f32 inFar) const;
	void									getLightSpaceMatrices(glm::mat4 ioMats[kCascadeCount]) const;
	void									renderMeshes(Shader inShader);
	void									bloomSetup();
};
