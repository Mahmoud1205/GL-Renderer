#include "Renderer.h"

#include "Environment.h"
#include "Shader.h"
#include "DebugDraw.h"
#include "ResourceManager.h"
#include "Utils.h"
#include "Compute.h"
#include "PostFX.h"
#include <vector>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

constexpr f32 gShadowCascadeLevels[Renderer::kFrustumCount] = {
	//kFarPlane / 50.0f,
	kFarPlane / 25.0f,
	kFarPlane / 10.0f,
	kFarPlane / 2.0f
};

//glm::vec3 gSunPos(-18.0f, 100.0f, -89.3f); // city
glm::vec3 gSunPos(2.0f, 85.0f, 0.0f); // sponza

static void openglDebugCb(
	GLenum inSrc, GLenum inType, GLuint inID,
	GLenum inSeverity, GLsizei inLength,
	const GLchar* inMsg, const void* inUserParam);

static void getFrustumCornersWorld(glm::vec4 ioCorners[8], const glm::mat4& inViewProj);

bool Renderer::StartUp(u32 inWidth, u32 inHeight, GLFWwindow* ioWindow)
{
	#pragma region Main Setup (GL State, Shaders, etc.)
	mWidth = inWidth;
	mHeight = inHeight;

	glViewport(0, 0, mWidth, mHeight);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	//glFrontFace(GL_CCW);

	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(openglDebugCb, nullptr);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForOpenGL(ioWindow, true);
	ImGui_ImplOpenGL3_Init();

	{ // full screen quad setup
		const f32 quadVertices[] = {
			-1.0f,  1.0f,  0.0f, 1.0f,
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,

			-1.0f,  1.0f,  0.0f, 1.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f
		};

		glCreateVertexArrays(1, &mFullScreenQuadVAO);
		glCreateBuffers(1, &mFullScreenQuadVBO);
		glVertexArrayVertexBuffer(mFullScreenQuadVAO, 0, mFullScreenQuadVBO, 0, 4 * sizeof(f32));

		glNamedBufferStorage(mFullScreenQuadVBO, sizeof(quadVertices), quadVertices, GL_DYNAMIC_STORAGE_BIT);

		glEnableVertexArrayAttrib(mFullScreenQuadVAO, 0);
		glEnableVertexArrayAttrib(mFullScreenQuadVAO, 1);

		glVertexArrayAttribFormat(mFullScreenQuadVAO, 0, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribFormat(mFullScreenQuadVAO, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32));

		glVertexArrayAttribBinding(mFullScreenQuadVAO, 0, 0);
		glVertexArrayAttribBinding(mFullScreenQuadVAO, 1, 0);
	}

	{
		bloomSetup();

		mBloomDownsampleShader.Load("res/shaders/FullScreen.vert", "res/shaders/BloomDownsample.frag");
		mBloomDownsampleShader.Use();

		mBloomUpsampleShader.Load("res/shaders/FullScreen.vert", "res/shaders/BloomUpsample.frag");
		mBloomUpsampleShader.Use();
	}

	{
		mLensDirtTexture = ResMgr::GetTexture("res/textures/lens_dirt.jpg");

		mPostFxShader.Load("res/shaders/FullScreen.vert", "res/shaders/PostFX.frag");
		mPostFxShader.Use();

		// glBindTexture(GL_TEXTURE_3D, 0);
	}

	mTransparentShader.Load("res/shaders/Transparent.vert", "res/shaders/Transparent.frag");
	mTransparentShader.Use();
	mTransparentShader.SetVec3("uDirLight.mDirection", kSunDirection);
	mTransparentShader.SetVec3("uDirLight.mColor", glm::vec3(0.38f));

	mLightingShader.Load("res/shaders/FullScreen.vert", "res/shaders/Lighting.frag");
	mLightingShader.Use();
	mLightingShader.SetVec3("uDirLight.mDirection", kSunDirection);
	mLightingShader.SetVec3("uDirLight.mColor", glm::vec3(0.38f));

	mFullScreenShader.Load("res/shaders/FullScreen.vert", "res/shaders/FullScreen.frag");

	mSsaoBlurShader.Load("res/shaders/FullScreen.vert", "res/shaders/SsaoBlur.frag");
	mSsaoBlurShader.Use();

	{
		mSsaoShader.Load("res/shaders/FullScreen.vert", "res/shaders/SSAO.frag");
		mSsaoShader.Use();
		mSsaoShader.SetMat4("uProjection", mCamera.mProjection);

		for (u32 i = 0; i < 32; i++)
		{
			glm::vec3 sample(
				Utils::RandomBetween(-1.0f, 1.0f),
				Utils::RandomBetween(-1.0f, 1.0f),
				Utils::RandomBetween(0.0f, 1.0f)
			);

			sample = glm::normalize(sample);
			sample *= Utils::RandomBetween(0.0f, 1.0f);

			f32 scale = (f32)i / 64.0f;
			scale = glm::mix(0.1f, 1.0f, scale * scale);
			sample *= scale;

			mSsaoShader.SetVec3("uSamples[" + std::to_string(i) + "]", sample);
		}

		glm::vec3 ssaoNoise[16] = {};
		for (u32 i = 0; i < 16; i++)
		{
			ssaoNoise[i] = glm::vec3(
				Utils::RandomBetween(-1.0f, 1.0f),
				Utils::RandomBetween(-1.0f, 1.0f),
				0.0f
			);
		}

		glCreateTextures(GL_TEXTURE_2D, 1, &mSsaoNoiseTex);
		glTextureParameteri(mSsaoNoiseTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(mSsaoNoiseTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(mSsaoNoiseTex, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(mSsaoNoiseTex, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTextureStorage2D(mSsaoNoiseTex, 1, GL_RGB16F, 4, 4);
		glTextureSubImage2D(mSsaoNoiseTex, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT, ssaoNoise);
	}

	mFxaaShader.Load("res/shaders/FullScreen.vert", "res/shaders/FXAA.frag");
	mFxaaShader.Use();

	mDeferredShader.Load("res/shaders/Deferred.vert", "res/shaders/Deferred.frag");
	mDeferredShader.Use();
	mDeferredShader.SetMat4("uProjection", mCamera.mProjection);

	mShadowMapShader.Load("res/shaders/ShadowMap.vert", "res/shaders/ShadowMap.frag", "res/shaders/ShadowMap.geom");

	Skybox::StartUpSystem();
	Skybox::UpdateProjection(mCamera.mProjection);

	if (!mSkybox.Load("res/skybox/day/day_", ".jpg"))
	{
		ZR_ASSERT(false, "Failed to load skybox.");
		return false;
	}
	#pragma endregion

	#pragma region Setup framebuffers
	glCreateTextures(GL_TEXTURE_2D, 1, &mAlbedoTex);
	GL_LABEL(GL_TEXTURE, mAlbedoTex, "G Albedo");
	glTextureParameteri(mAlbedoTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mAlbedoTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(mAlbedoTex, 1, GL_RGBA16F, mWidth, mHeight);

	glCreateTextures(GL_TEXTURE_2D, 1, &mSpecularTex);
	GL_LABEL(GL_TEXTURE, mSpecularTex, "G Specular");
	glTextureParameteri(mSpecularTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mSpecularTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(mSpecularTex, 1, GL_RGBA16F, mWidth, mHeight);

	glCreateTextures(GL_TEXTURE_2D, 1, &mNormalTex);
	GL_LABEL(GL_TEXTURE, mNormalTex, "G Normal");
	glTextureParameteri(mNormalTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mNormalTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(mNormalTex, 1, GL_RGBA16F, mWidth, mHeight);

	glCreateTextures(GL_TEXTURE_2D, 1, &mPositionTex);
	GL_LABEL(GL_TEXTURE, mPositionTex, "G Position");
	glTextureParameteri(mPositionTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mPositionTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(mPositionTex, 1, GL_RGBA32F, mWidth, mHeight);

	glCreateRenderbuffers(1, &mDepthRBO);
	GL_LABEL(GL_RENDERBUFFER, mDepthRBO, "Depth RBO");
	glNamedRenderbufferStorage(mDepthRBO, GL_DEPTH_COMPONENT24, mWidth, mHeight);

	glCreateFramebuffers(1, &mDeferredFBO);
	GL_LABEL(GL_TEXTURE, mDeferredFBO, "Deferred FBO");
	glNamedFramebufferTexture(mDeferredFBO, GL_COLOR_ATTACHMENT0, mAlbedoTex, 0);
	glNamedFramebufferTexture(mDeferredFBO, GL_COLOR_ATTACHMENT1, mSpecularTex, 0);
	glNamedFramebufferTexture(mDeferredFBO, GL_COLOR_ATTACHMENT2, mNormalTex, 0);
	glNamedFramebufferTexture(mDeferredFBO, GL_COLOR_ATTACHMENT3, mPositionTex, 0);
	glNamedFramebufferRenderbuffer(mDeferredFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthRBO);
	u32 colorAttachments4[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glNamedFramebufferDrawBuffers(mDeferredFBO, 4, colorAttachments4);
	if (glCheckNamedFramebufferStatus(mDeferredFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create deferred framebuffer (G-Buffer).\n");
		getchar();
		return false;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &mLightingTex);
	GL_LABEL(GL_TEXTURE, mLightingTex, "Deferred Lighting");
	glTextureParameteri(mLightingTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mLightingTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(mLightingTex, 1, GL_RGBA16F, mWidth, mHeight);
	glCreateFramebuffers(1, &mLightingFBO);
	GL_LABEL(GL_FRAMEBUFFER, mLightingFBO, "Lighting FBO");
	glNamedFramebufferTexture(mLightingFBO, GL_COLOR_ATTACHMENT0, mLightingTex, 0);
	// in modern opengl you can bind one render buffer to two frame buffers
	glNamedFramebufferRenderbuffer(mLightingFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthRBO);
	if (glCheckNamedFramebufferStatus(mLightingFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create lighting framebuffer.\n");
		getchar();
		return false;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &mHdrTex);
	GL_LABEL(GL_TEXTURE, mHdrTex, "HDR Texture");
	glTextureParameteri(mHdrTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mHdrTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureStorage2D(mHdrTex, 1, GL_RGBA8, mWidth, mHeight);

	glCreateFramebuffers(1, &mHdrFBO);
	GL_LABEL(GL_FRAMEBUFFER, mHdrFBO, "HDR FBO");
	glNamedFramebufferTexture(mHdrFBO, GL_COLOR_ATTACHMENT0, mHdrTex, 0);
	if (glCheckNamedFramebufferStatus(mHdrFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create HDR tonemapping framebuffer.\n");
		getchar();
		return false;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &mSsaoTex);
	GL_LABEL(GL_TEXTURE, mSsaoTex, "SSAO Texture");
	glTextureParameteri(mSsaoTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(mSsaoTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureStorage2D(mSsaoTex, 1, GL_RGBA16F, mWidth, mHeight);

	glCreateFramebuffers(1, &mSsaoFBO);
	GL_LABEL(GL_FRAMEBUFFER, mSsaoFBO, "SSAO FBO");
	glNamedFramebufferTexture(mSsaoFBO, GL_COLOR_ATTACHMENT0, mSsaoTex, 0);
	if (glCheckNamedFramebufferStatus(mSsaoFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create SSAO framebuffer.\n");
		getchar();
		return false;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &mSsaoBlurTex);
	GL_LABEL(GL_TEXTURE, mSsaoBlurTex, "SSAO Blur Texture");
	glTextureParameteri(mSsaoBlurTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(mSsaoBlurTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureStorage2D(mSsaoBlurTex, 1, GL_RGBA16F, mWidth, mHeight);

	glCreateFramebuffers(1, &mSsaoBlurFBO);
	GL_LABEL(GL_FRAMEBUFFER, mSsaoBlurFBO, "SSAO Blur FBO");
	glNamedFramebufferTexture(mSsaoBlurFBO, GL_COLOR_ATTACHMENT0, mSsaoBlurTex, 0);
	if (glCheckNamedFramebufferStatus(mSsaoBlurFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create SSAO blur framebuffer.\n");
		getchar();
		return false;
	}

	glCreateTextures(GL_TEXTURE_2D, 1, &mFxaaTex);
	GL_LABEL(GL_TEXTURE, mFxaaTex, "FXAA Texture");
	glTextureParameteri(mFxaaTex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(mFxaaTex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureStorage2D(mFxaaTex, 1, GL_RGBA8, mWidth, mHeight);

	glCreateFramebuffers(1, &mFxaaFBO);
	GL_LABEL(GL_FRAMEBUFFER, mFxaaFBO, "FXAA FBO");
	glNamedFramebufferTexture(mFxaaFBO, GL_COLOR_ATTACHMENT0, mFxaaTex, 0);
	if (glCheckNamedFramebufferStatus(mFxaaFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create SSAO blur framebuffer.\n");
		getchar();
		return false;
	}

	glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &mCascadeTexArray);
	GL_LABEL(GL_TEXTURE, mCascadeTexArray, "CSM Tex Array");
	glTextureParameteri(mCascadeTexArray, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(mCascadeTexArray, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(mCascadeTexArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTextureParameteri(mCascadeTexArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	f32 shadowMapBorderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTextureParameterfv(mCascadeTexArray, GL_TEXTURE_BORDER_COLOR, shadowMapBorderColor);
	glTextureStorage3D(mCascadeTexArray, 1, GL_DEPTH_COMPONENT16, kShadowQuality, kShadowQuality, kCascadeCount);

	glCreateFramebuffers(1, &mShadowMapFBO);
	GL_LABEL(GL_FRAMEBUFFER, mShadowMapFBO, "Shadow Map FBO");
	glNamedFramebufferTexture(mShadowMapFBO, GL_DEPTH_ATTACHMENT, mCascadeTexArray, 0);
	glNamedFramebufferDrawBuffer(mShadowMapFBO, GL_NONE);
	glNamedFramebufferReadBuffer(mShadowMapFBO, GL_NONE);
	if (glCheckNamedFramebufferStatus(mShadowMapFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create shadow map framebuffer.\n");
		getchar();
		return false;
	}
	#pragma endregion

	mLumaShader.Load("res/shaders/Luma.comp");
	mExposureShader.Load("res/shaders/Exposure.comp");

	glCreateBuffers(1, &mLumaSSBO);
	glNamedBufferStorage(
		mLumaSSBO, sizeof(LumaExposureComp), nullptr,
		GL_MAP_READ_BIT | GL_DYNAMIC_STORAGE_BIT
	);
	LumaExposureComp initial{};
	glNamedBufferSubData(mLumaSSBO, 0, sizeof(LumaExposureComp), &initial);

	PostFX::CLUT clut{};
	if (!PostFX::LoadCLUT(&clut, "res/postfx/vibrant2.CUBE"))
	{
		fprintf(stderr, "ERROR: Failed to load CLUT.\n");
		return false;
	}

	glCreateTextures(GL_TEXTURE_3D, 1, &mClutTex);
	glTextureParameteri(mClutTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(mClutTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(mClutTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(mClutTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureParameteri(mClutTex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTextureStorage3D(mClutTex, 1, GL_RGB32F, clut.mSize, clut.mSize, clut.mSize);
	glTextureSubImage3D(mClutTex, 0, 0, 0, 0, clut.mSize, clut.mSize, clut.mSize, GL_RGB, GL_FLOAT, clut.mData);

	return true;
}

void Renderer::ShutDown()
{
	mMeshes.clear();
	mPointLights.clear();
	mTransparentMeshes.clear();

	glDeleteVertexArrays(1, &mFullScreenQuadVAO);
	glDeleteBuffers(1, &mFullScreenQuadVBO);

	ResMgr::ReleaseTexture(mLensDirtTexture);

	mSkybox.Unload();
	Skybox::ShutDownSystem();

	mDeferredShader.Unload();
	mPostFxShader.Unload();
	mFxaaShader.Unload();
	mLightingShader.Unload();
	mSsaoShader.Unload();
	mSsaoBlurShader.Unload();
	mBloomUpsampleShader.Unload();
	mBloomDownsampleShader.Unload();
	mShadowMapShader.Unload();
	mTransparentShader.Unload();
	mFullScreenShader.Unload();
	mLumaShader.Unload();
	mExposureShader.Unload();

	glDeleteTextures(1, &mAlbedoTex);
	glDeleteTextures(1, &mSpecularTex);
	glDeleteTextures(1, &mNormalTex);
	glDeleteTextures(1, &mPositionTex);
	glDeleteTextures(1, &mLightingTex);
	glDeleteTextures(1, &mHdrTex);
	glDeleteTextures(1, &mSsaoTex);
	glDeleteTextures(1, &mSsaoBlurTex);
	glDeleteTextures(1, &mSsaoNoiseTex);
	glDeleteTextures(1, &mFxaaTex);
	glDeleteTextures(1, &mCascadeTexArray);
	glDeleteTextures(1, &mClutTex);
	glDeleteFramebuffers(1, &mShadowMapFBO);
	glDeleteFramebuffers(1, &mDeferredFBO);
	glDeleteFramebuffers(1, &mLightingFBO);
	glDeleteFramebuffers(1, &mHdrFBO);
	glDeleteFramebuffers(1, &mSsaoFBO);
	glDeleteFramebuffers(1, &mSsaoBlurFBO);
	glDeleteFramebuffers(1, &mFxaaFBO);
	glDeleteFramebuffers(1, &mBloomFBO);

	glDeleteBuffers(1, &mLumaSSBO);

	for (u32 i = 0; i < mBloomMipChain.size(); i++)
	{
		BloomMip* mip = &mBloomMipChain[i];
		glDeleteTextures(1, &mip->mID);
	}
	mBloomMipChain.clear();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void Renderer::SetCamera(Camera inCamera)
{
	mCamera = inCamera;
	mDeferredShader.Use();
	mDeferredShader.SetMat4("uProjection", mCamera.mProjection);
	mSsaoShader.Use();
	mSsaoShader.SetMat4("uProjection", mCamera.mProjection);
	Skybox::UpdateProjection(mCamera.mProjection);
}

void Renderer::OnResize(u32 inWidth, u32 inHeight)
{
	mWidth = inWidth;
	mHeight = inHeight;

	glViewport(0, 0, inWidth, inHeight);

	{
		Skybox::UpdateProjection(mCamera.mProjection);

		mDeferredShader.Use();
		mDeferredShader.SetMat4("uProjection", mCamera.mProjection);

		mSsaoShader.Use();
		mSsaoShader.SetMat4("uProjection", mCamera.mProjection);
	}

	{
		for (u32 i = 0; i < mBloomMipChain.size(); i++)
			glDeleteTextures(1, &mBloomMipChain[i].mID);

		mBloomMipChain.clear();
		glDeleteFramebuffers(1, &mBloomFBO);
		bloomSetup();
	}

	{
		glDeleteRenderbuffers(1, &mDepthRBO);
		glCreateRenderbuffers(1, &mDepthRBO);
		glNamedRenderbufferStorage(mDepthRBO, GL_DEPTH_COMPONENT24, mWidth, mHeight);
	}

	glNamedFramebufferRenderbuffer(mDeferredFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthRBO);
	if (glCheckNamedFramebufferStatus(mDeferredFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to update depth renderbuffer size (for deferred FBO).\n");
		exit(1);
	}

	glNamedFramebufferRenderbuffer(mLightingFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthRBO);
	if (glCheckNamedFramebufferStatus(mLightingFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to update depth renderbuffer size (for lighting FBO).\n");
		exit(1);
	}

	auto resizeTexAndUpdateFBO = [&](u32* ioTexId, u32 inFBO, u32 inAttachment, u32 inInternalFormat) {
		glDeleteTextures(1, ioTexId);
		glCreateTextures(GL_TEXTURE_2D, 1, ioTexId);
		glTextureParameteri(*ioTexId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(*ioTexId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureStorage2D(*ioTexId, 1, inInternalFormat, mWidth, mHeight);

		glNamedFramebufferTexture(inFBO, inAttachment, *ioTexId, 0);
		if (glCheckNamedFramebufferStatus(inFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			puts("Error: Failed to update framebuffer size.\n");
			exit(1);
		}
	};

	resizeTexAndUpdateFBO(&mAlbedoTex, mDeferredFBO, GL_COLOR_ATTACHMENT0, GL_RGBA16F);
	resizeTexAndUpdateFBO(&mSpecularTex, mDeferredFBO, GL_COLOR_ATTACHMENT1, GL_RGBA16F);
	resizeTexAndUpdateFBO(&mNormalTex, mDeferredFBO, GL_COLOR_ATTACHMENT2, GL_RGBA16F);
	resizeTexAndUpdateFBO(&mPositionTex, mDeferredFBO, GL_COLOR_ATTACHMENT3, GL_RGBA32F);
	
	resizeTexAndUpdateFBO(&mLightingTex, mLightingFBO, GL_COLOR_ATTACHMENT0, GL_RGBA16F);
	resizeTexAndUpdateFBO(&mHdrTex, mHdrFBO, GL_COLOR_ATTACHMENT0, GL_RGBA8);

	resizeTexAndUpdateFBO(&mSsaoTex, mSsaoFBO, GL_COLOR_ATTACHMENT0, GL_RGBA16F);
	resizeTexAndUpdateFBO(&mSsaoBlurTex, mSsaoBlurFBO, GL_COLOR_ATTACHMENT0, GL_RGBA16F);

	resizeTexAndUpdateFBO(&mFxaaTex, mFxaaFBO, GL_COLOR_ATTACHMENT0, GL_RGBA8);
}

void Renderer::BeginUI()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void Renderer::Render(f32 inDeltaTime, f32 inCurrentTime)
{
	{ // render geometry data to g-buffer
		GL_ZONE("Render G-Buffer");
		ZoneScopedN("Render G-Buffer");
		mDeferredShader.Use();
		glBindFramebuffer(GL_FRAMEBUFFER, mDeferredFBO);
		glEnable(GL_DEPTH_TEST);

		glClearColor(0.1f, 0.14f, 0.21f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		mDeferredShader.SetMat4("uView", mCamera.mView);
		renderMeshes(mDeferredShader);

		GL_ZONE_END();
	}

	GL_ZONE("SSAO");
	{ // calculate SSAO
		GL_ZONE("Calculate Occlusion");
		ZoneScopedN("Calculate SSAO");

		mSsaoShader.Use();
		glBindFramebuffer(GL_FRAMEBUFFER, mSsaoFBO);

		glDisable(GL_DEPTH_TEST);

		glBindTextureUnit(0, mPositionTex);
		glBindTextureUnit(1, mNormalTex);
		glBindTextureUnit(2, mSsaoNoiseTex);

		glBindVertexArray(mFullScreenQuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		GL_ZONE_END();
	}

	{ // blur SSAO output
		GL_ZONE("Blur Occlusion");
		ZoneScopedN("Blur SSAO");

		mSsaoBlurShader.Use();
		glBindFramebuffer(GL_FRAMEBUFFER, mSsaoBlurFBO);

		glDisable(GL_DEPTH_TEST);

		glBindTextureUnit(0, mSsaoTex);

		glBindVertexArray(mFullScreenQuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		GL_ZONE_END();
	}
	GL_ZONE_END();

	glm::mat4 cascadeMatrices[kCascadeCount];
	glm::vec4 viewCorners[8];

	//DebugDraw::AddLine({
	//	.mFrom = gSunPos,
	//	.mTo = glm::vec3(-10.0f, 0.0f, 0.0f),
	//	.mStyle = DebugDraw::ELineStyle::Arrow,
	//	.mColor = glm::vec3(1.0f, 0.0f, 0.0f),
	//	.mLifetime = 1
	//});

	{
		GL_ZONE("Sun Shadow Map");
		ZoneScopedN("Sun Shadow Map");

		glBindFramebuffer(GL_FRAMEBUFFER, mShadowMapFBO);
		glViewport(0, 0, kShadowQuality, kShadowQuality);
		glClear(GL_DEPTH_BUFFER_BIT);

		// city cfg
		//constexpr f32 kNearPlane		= 0.0001f;
		//constexpr f32 kFarPlane			= 200.0f;
		//constexpr f32 kShadowMapSize	= 100.0f;

		// sponza cfg
		//constexpr f32 kNearPlane		= 0.1f;
		//constexpr f32 kFarPlane			= 100.0f;
		//constexpr f32 kShadowMapSize	= 50.0f;

		//glm::mat4 lightProjection = glm::ortho(
		//	-kShadowMapSize, kShadowMapSize,
		//	-kShadowMapSize, kShadowMapSize,
		//	kNearPlane, kFarPlane
		//);

		//glm::vec3 camPosXZ(mCamera.mPos.x, 0.0f, mCamera.mPos.z);

		// city cfg
		//glm::mat4 lightView = glm::lookAt(
		//	gSunPos,
		//	glm::vec3(60.0f, 0.0f, 60.0f),
		//	glm::vec3(0.0f, 1.0f, 0.0f)
		//);

		// sponza cfg
		//glm::mat4 lightView = glm::lookAt(
		//	glm::vec3(3.6f, 97.0f, 7.1f),
		//	glm::vec3(0.0f),
		//	glm::vec3(0.0f, 1.0f, 0.0f)
		//);

		getLightSpaceMatrices(cascadeMatrices);

		mLightingShader.Use();
		mLightingShader.SetVec3("uDirLight.mDirection", glm::normalize(gSunPos));
		for (u32 i = 0; i < kCascadeCount; i++)
			mLightingShader.SetMat4("uCascadeMatrices[" + std::to_string(i) + "]", cascadeMatrices[i]);

		mTransparentShader.Use();
		for (u32 i = 0; i < kCascadeCount; i++)
			mTransparentShader.SetMat4("uCascadeMatrices[" + std::to_string(i) + "]", cascadeMatrices[i]);

		mShadowMapShader.Use();
		for (u32 i = 0; i < kCascadeCount; i++)
			mShadowMapShader.SetMat4("uCascadeMatrices[" + std::to_string(i) + "]", cascadeMatrices[i]);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_DEPTH_CLAMP);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderMeshes(mShadowMapShader);
		glDisable(GL_DEPTH_CLAMP);
		glCullFace(GL_BACK);
		glDisable(GL_CULL_FACE);

		// reset the viewport to normal size
		glViewport(0, 0, mWidth, mHeight);
		GL_ZONE_END();
	}

	{ // apply lighting in screen space
		GL_ZONE("Deferred Lighting");
		ZoneScopedN("Deferred Lighting");

		glBlitNamedFramebuffer(
			mDeferredFBO, mLightingFBO,
			0, 0, mWidth, mHeight,
			0, 0, mWidth, mHeight,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST
		);

		glBindFramebuffer(GL_FRAMEBUFFER, mLightingFBO);

		glDisable(GL_DEPTH_TEST);
		mLightingShader.Use();
		mLightingShader.SetVec3("uViewPos", mCamera.mPos);
		mLightingShader.SetMat4("uView", mCamera.mView);
		mLightingShader.SetInt("uEnableSSAO", (i32)mSettings.mEnableSSAO);

		// Optimize: sending these uniforms when they change only
		mLightingShader.SetUint("uNumPointLights", mPointLights.size());
		for (u32 i = 0; i < mPointLights.size(); i++)
		{
			mLightingShader.SetVec3("uPointLights[" + std::to_string(i) + "].mPosition", mPointLights[i]->mPosition);
			mLightingShader.SetVec3("uPointLights[" + std::to_string(i) + "].mColor", mPointLights[i]->mColor);
			mLightingShader.SetFloat("uPointLights[" + std::to_string(i) + "].mLinear", mPointLights[i]->mLinear);
			mLightingShader.SetFloat("uPointLights[" + std::to_string(i) + "].mQuadratic", mPointLights[i]->mQuadratic);
		}

		for (u32 i = 0; i < kFrustumCount; i++)
		{
			mLightingShader.SetFloat("uShadowCascadeLevels[" + std::to_string(i) + "]", gShadowCascadeLevels[i]);
		}

		glBindTextureUnit(0, mAlbedoTex);
		glBindTextureUnit(1, mSpecularTex);
		glBindTextureUnit(2, mNormalTex);
		glBindTextureUnit(3, mPositionTex);
		glBindTextureUnit(4, mSsaoBlurTex);
		glBindTextureUnit(5, mCascadeTexArray);

		glBindVertexArray(mFullScreenQuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		GL_ZONE_END();
	}

	{ // render transparent meshes by forward rendering
		GL_ZONE("Transparent Meshes (Fwd Rendering)");
		ZoneScopedN("Transparent Meshes (Fwd Rendering)");

		glBindFramebuffer(GL_FRAMEBUFFER, mLightingFBO);

		mTransparentShader.Use();

		for (u32 i = 0; i < kFrustumCount; i++)
		{
			mTransparentShader.SetFloat("uShadowCascadeLevels[" + std::to_string(i) + "]", gShadowCascadeLevels[i]);
		}

		mTransparentShader.SetMat4("uView", mCamera.mView);
		mTransparentShader.SetMat4("uProjection", mCamera.mProjection);
		mTransparentShader.SetVec3("uViewPos", mCamera.mPos);

		// OPTIMIZE: sending these uniforms when they change only
		mTransparentShader.SetUint("uNumPointLights", mPointLights.size());
		for (u32 i = 0; i < mPointLights.size(); i++)
		{
			mTransparentShader.SetVec3("uPointLights[" + std::to_string(i) + "].mPosition", mPointLights[i]->mPosition);
			mTransparentShader.SetVec3("uPointLights[" + std::to_string(i) + "].mColor", mPointLights[i]->mColor);
			mTransparentShader.SetFloat("uPointLights[" + std::to_string(i) + "].mLinear", mPointLights[i]->mLinear);
			mTransparentShader.SetFloat("uPointLights[" + std::to_string(i) + "].mQuadratic", mPointLights[i]->mQuadratic);
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);

		glBindTextureUnit(3, mCascadeTexArray);

		for (u32 i = 0; i < mTransparentMeshes.size(); i++)
		{
			const Geom::Mesh* mesh = mTransparentMeshes[i];

			if (mesh->mOpacityTexture)
				mTransparentShader.SetInt("uUseTransparencyTex", true);
			else if (mesh->mDiffuseTexture->mHasTransparency)
				mTransparentShader.SetInt("uUseTransparencyTex", false);
			else
				ZR_ASSERT(false, "");

			mTransparentShader.SetMat4("uModel", mesh->mTransform);
			mesh->Draw();
		}

		mTransparentMeshes.clear();

		glDisable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);

		GL_ZONE_END();
	}

	// calculate lighting texture luma in compute shader
	if (mSettings.mEnableAutoExposure)
	{
		GL_ZONE("Compute Scene Luma");
		ZoneScopedN("Compute Luma");

		static const u32 zero = 0;

		{
			ZoneScopedN("Dispatch Luma Comp");
			glUseProgram(mLumaShader.mID);
			glBindTextureUnit(0, mLightingTex);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mLumaSSBO);
			glDispatchCompute(mWidth, mHeight, 1);
		}
		{
			ZoneScopedN("Barrier");
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		{
			ZoneScopedN("Dispatch Exposure Comp");
			glUseProgram(mExposureShader.mID);
			glUniform1f(glGetUniformLocation(mExposureShader.mID, "uDeltaTime"), inDeltaTime);
			glUniform2f(glGetUniformLocation(mExposureShader.mID, "uScreenSize"), mWidth, mHeight);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mLumaSSBO);
			glDispatchCompute(1, 1, 1);
		}
		{
			ZoneScopedN("Barrier");
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		{
			ZoneScopedN("Reset Total Luma");
			glNamedBufferSubData(mLumaSSBO, OFFSETOF(LumaExposureComp, mTotalLuma), sizeof(u32), &zero);
		}

		GL_ZONE_END();
	}

	{ // draw the skybox using forward rendering and use deferred stage depth
		GL_ZONE("Skybox");
		ZoneScopedN("Render Skybox");

		// the lighting FBO is already bound for read and write

		glEnable(GL_DEPTH_TEST);
		mSkybox.Draw(mCamera.mView);
		GL_ZONE_END();
	}

	{ // render bloom texture
		GL_ZONE("Bloom");
		ZoneScopedN("Render Bloom Texture");

		glBindFramebuffer(GL_FRAMEBUFFER, mBloomFBO);

		{ // downsample
			GL_ZONE("Downsample");
			ZoneScopedN("Downsample Bloom");

			mBloomDownsampleShader.Use();
			glBindTextureUnit(0, mLightingTex);

			// downscale and apply filter for each mip
			for (u32 i = 0; i < mBloomMipChain.size(); i++)
			{
				const BloomMip& mip = mBloomMipChain[i];
				glViewport(0, 0, mip.mSize.x, mip.mSize.y);
				glNamedFramebufferTexture(mBloomFBO, GL_COLOR_ATTACHMENT0, mip.mID, 0);

				glBindVertexArray(mFullScreenQuadVAO);
				glDrawArrays(GL_TRIANGLES, 0, 6);

				glBindTextureUnit(0, mip.mID);
			}
			GL_ZONE_END();
		}

		{ // upsample
			GL_ZONE("Upsample");
			ZoneScopedN("Upsample Bloom");

			mBloomUpsampleShader.Use();
			mBloomUpsampleShader.SetFloat("uFilterRadius", 0.005f);

			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glBlendEquation(GL_FUNC_ADD);

			// reverse the downscale and apply filter for each mip
			for (u32 i = mBloomMipChain.size() - 1; i > 0; i--)
			{
				const BloomMip& currentMip = mBloomMipChain[i];
				const BloomMip& nextMip = mBloomMipChain[i - 1];

				glBindTextureUnit(0, currentMip.mID);

				glViewport(0, 0, nextMip.mSize.x, nextMip.mSize.y);
				glNamedFramebufferTexture(mBloomFBO, GL_COLOR_ATTACHMENT0, nextMip.mID, 0);
				glBindVertexArray(mFullScreenQuadVAO);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}

			glDisable(GL_BLEND);
			GL_ZONE_END();
		}

		glViewport(0, 0, mWidth, mHeight);
		GL_ZONE_END();
	}

	{ // render texture to full screen quad and apply post process
		GL_ZONE("Post FX");
		ZoneScopedN("Post FX");

		glBindFramebuffer(GL_FRAMEBUFFER, mHdrFBO);

		glDisable(GL_DEPTH_TEST);

		mPostFxShader.Use();
		mPostFxShader.SetInt("uEnableCLUT", (i32)mSettings.mEnableCLUT);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mLumaSSBO);

		glBindTextureUnit(0, mLightingTex);
		glBindTextureUnit(1, mBloomMipChain[0].mID);
		glBindTextureUnit(2, mLensDirtTexture->mID);
		glBindTextureUnit(3, mClutTex);

		glBindVertexArray(mFullScreenQuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		GL_ZONE_END();
	}

	// fxaa cant anti alias lines :)

	// Note: always draw UI after FXAA
	{
		GL_ZONE("FXAA");
		ZoneScopedN("Apply FXAA");

		glBindFramebuffer(GL_FRAMEBUFFER, mFxaaFBO);
		glDisable(GL_DEPTH_TEST);
		mFxaaShader.Use();
		mFxaaShader.SetInt("uEnableFxaa", (i32)mSettings.mEnableFXAA);
		glBindTextureUnit(0, mHdrTex);
		glBindVertexArray(mFullScreenQuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// draw FXAA output to back buffer to full screen quad
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		mFullScreenShader.Use();
		glBindTextureUnit(0, mFxaaTex);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		GL_ZONE_END();
	}

	// blit lighting FBO depth buffer to back buffer depth
	glBlitNamedFramebuffer(
		mLightingFBO, 0,
		0, 0, mWidth, mHeight,
		0, 0, mWidth, mHeight,
		GL_DEPTH_BUFFER_BIT, GL_NEAREST
	);
	gDebugDraw.DrawFrame(mCamera);

	{
		GL_ZONE("ImGui");
		ZoneScopedN("Render ImGui");

		u32 csmTexViews[kCascadeCount] = { 0 };
		glGenTextures(kCascadeCount, csmTexViews);
		for (u32 i = 0; i < kCascadeCount; i++)
		{
			glTextureView(csmTexViews[i], GL_TEXTURE_2D, mCascadeTexArray, GL_DEPTH_COMPONENT16, 0, 1, i, 1);
		}

		ImGui::Begin("Renderer");
		{
			#define IMGUI_IMAGE(inID, inPos) ImGui::Image(inID, inPos, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f))

			static f32 sunPos[3] = { gSunPos.x, gSunPos.y, gSunPos.z };

			if (ImGui::Button("save"))
				getFrustumCornersWorld(viewCorners, mCamera.mProjection * mCamera.mView);

			ImGui::SliderFloat3("Sun", sunPos, -100.0f, 100.0f);
			gSunPos.x = sunPos[0];
			gSunPos.y = sunPos[1];
			gSunPos.z = sunPos[2];
			ImGui::Text("CSM");
			for (u32 i = 0; i < kCascadeCount; i++)
			{
				IMGUI_IMAGE(csmTexViews[i], ImVec2(256.0f / f32(i + 1), 256.0f / f32(i + 1)));
			}
			ImGui::Text("Normal Map");
			IMGUI_IMAGE(mNormalTex, ImVec2((f32)mWidth / 6.0f, (f32)mHeight / 6.0f));
		} ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		GL_ZONE_END();

		glDeleteTextures(kCascadeCount, csmTexViews);
	}
}

void openglDebugCb(
	GLenum inSrc, GLenum inType, GLuint inID,
	GLenum inSeverity, GLsizei inLength,
	const GLchar* inMsg, const void* inUserParam)
{
	const char* source = [inSrc]() {
		switch (inSrc)
		{
		case GL_DEBUG_SOURCE_API:				return "API";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:		return "Window System";
		case GL_DEBUG_SOURCE_SHADER_COMPILER:	return "Shader Compiler";
		case GL_DEBUG_SOURCE_THIRD_PARTY:		return "Third Party";
		case GL_DEBUG_SOURCE_APPLICATION:		return "Application";
		case GL_DEBUG_SOURCE_OTHER:				return "Other";
		default:								return "Unknown Source";
		};
	}();

	const char* type = [inType]() {
		switch (inType)
		{
		case GL_DEBUG_TYPE_ERROR:				return "Error";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated Behavior";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	return "UB";
		case GL_DEBUG_TYPE_PORTABILITY:			return "Portability";
		case GL_DEBUG_TYPE_PERFORMANCE:			return "Performance";
		case GL_DEBUG_TYPE_MARKER:				return "Marker";
		case GL_DEBUG_TYPE_OTHER:				return "Other";
		default:								return "Unknown Type";
		}
	}();

	const char* severity = [inSeverity]() {
		switch (inSeverity)
		{
		case GL_DEBUG_SEVERITY_NOTIFICATION:	return "Info";
		case GL_DEBUG_SEVERITY_LOW:				return "Low";
		case GL_DEBUG_SEVERITY_MEDIUM:			return "Medium";
		case GL_DEBUG_SEVERITY_HIGH:			return "High";
		default:								return "Unknown Severity";
		}
	}();

	if (inSeverity != GL_DEBUG_SEVERITY_NOTIFICATION)
	{
		if (strncmp(inMsg, "Program/shader state performance warning: Vertex shader in program", 66) == 0)
			return;

		if (strncmp(inMsg, "API_ID_RECOMPILE_FRAGMENT_SHADER performance warning has been generated", 70) == 0)
			return;

		printf("%s level GL debug [%s, %s]: %s\n", severity, source, type, inMsg);

		SBREAK();
	}
}

void Renderer::bloomSetup()
{
	glCreateFramebuffers(1, &mBloomFBO);

	glm::vec2 mipSize((f32)mWidth, (f32)mHeight);
	for (u32 i = 0; i < 5; i++)
	{
		BloomMip mip{};
		mipSize /= 2.0f;
		mip.mSize = mipSize;

		glCreateTextures(GL_TEXTURE_2D, 1, &mip.mID);
		glTextureStorage2D(mip.mID, 1, GL_R11F_G11F_B10F, (i32)mipSize.x, (i32)mipSize.y);
		glTextureParameteri(mip.mID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(mip.mID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(mip.mID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(mip.mID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		mBloomMipChain.push_back(mip);
	}

	glNamedFramebufferTexture(mBloomFBO, GL_COLOR_ATTACHMENT0, mBloomMipChain[0].mID, 0);
	if (glCheckNamedFramebufferStatus(mBloomFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create bloom framebuffer.\n");
		getchar();
		exit(1);
	}
}

void Renderer::renderMeshes(Shader inShader)
{
	// Optimize: remove mMeshes. split into mOpaqueMeshes and mTransparentMeshes

	GL_ZONE("Render Opaque Meshes");
	for (u32 i = 0; i < mMeshes.size(); i++)
	{
		const Geom::Mesh* mesh = mMeshes[i];
		if (mesh->mOpacityTexture || (mesh->mDiffuseTexture && mesh->mDiffuseTexture->mHasTransparency))
		{
			// Note: this is mean tto be cleared each frame, or we have
			// a memory leak
			mTransparentMeshes.push_back(mesh);
			continue;
		}

		inShader.SetMat4("uModel", mesh->mTransform);
		mesh->Draw();
	}
	GL_ZONE_END();
}

void getFrustumCornersWorld(glm::vec4 ioCorners[8], const glm::mat4& inProjView)
{
	// apply inverse of P*V on the corners of the NDC cube ([-1,1])
	// to get the corners of the view frustum
	// vNDC = P*V*v
	// vWorld = ((P*V)^-1)vNDC

	const glm::mat4 invProjView = glm::inverse(inProjView);

	u8 i = 0;
	for (u32 x = 0; x < 2; x++)
	{
		for (u32 y = 0; y < 2; y++)
		{
			for (u32 z = 0; z < 2; z++)
			{
				// ((P*V)^-1)vNDC
				const glm::vec4 vWorld =
					invProjView *
					glm::vec4(
						2.0f * x - 1.0f,
						2.0f * y - 1.0f,
						2.0f * z - 1.0f,
						1.0f
					);

				// perspective devide
				ioCorners[i] = glm::vec4(vWorld / vWorld.w);
				i++;
			}
		}
	}
}

glm::mat4 Renderer::getLightSpaceMatrix(f32 inNear, f32 inFar) const
{
	glm::mat4 proj = glm::perspective(
		glm::radians(mCamera.mFOV),
		f32(mWidth) / f32(mHeight),
		inNear, inFar
	);

	glm::vec4 frustCorners[8];
	getFrustumCornersWorld(frustCorners, proj * mCamera.mView);

	// center of frust = avg(corners)
	glm::vec3 center(0.0f);
	for (u32 i = 0; i < 8; i++)
	{
		center += glm::vec3(frustCorners[i]);
	}
	center /= 8.0f;

	const glm::mat4 lightView = glm::lookAt(
		center + glm::normalize(gSunPos),
		center,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	f32 minX = FLT_MAX;
	f32 minY = FLT_MAX;
	f32 minZ = FLT_MAX;
	f32 maxX = -FLT_MAX;
	f32 maxY = -FLT_MAX;
	f32 maxZ = -FLT_MAX;
	// get smallest OBB aligned by lightView space that fits the
	// camera view frustmu
	for (u32 i = 0; i < 8; i++)
	{
		// frustum corner in the light view space (light view space
		// is the OBB axis)
		const glm::vec4 frustCorner = lightView * frustCorners[i];
		minX = std::min(minX, frustCorner.x);
		minY = std::min(minY, frustCorner.y);
		minZ = std::min(minZ, frustCorner.z);
		maxX = std::max(maxX, frustCorner.x);
		maxY = std::max(maxY, frustCorner.y);
		maxZ = std::max(maxZ, frustCorner.z);
	}

	// so objects behind or in front of the camera view
	// frustum can cast shadows.
	constexpr f32 kMultiplierZ = 2.0f;

	if (minZ < 0.0f)
		minZ *= kMultiplierZ;
	else
		minZ /= kMultiplierZ;

	if (maxZ < 0.0f)
		maxZ /= kMultiplierZ;
	else
		maxZ *= kMultiplierZ;

	const glm::mat4 lightProj = glm::ortho(
		minX * 2.0f,
		maxX * 2.0f,
		minY * 2.0f,
		maxY * 2.0f,
		minZ,
		maxZ
	);

	const glm::mat4 res = lightProj * lightView;

	return res;
}

void Renderer::getLightSpaceMatrices(glm::mat4 ioMats[kCascadeCount]) const
{
	for (u32 i = 0; i < kCascadeCount; i++)
	{
		if (i == 0)
			ioMats[i] = getLightSpaceMatrix(kNearPlane, gShadowCascadeLevels[i]);
		else if (i < kFrustumCount)
			ioMats[i] = getLightSpaceMatrix(gShadowCascadeLevels[i - 1], gShadowCascadeLevels[i]);
		else
			ioMats[i] = getLightSpaceMatrix(gShadowCascadeLevels[i - 1], kFarPlane);
	}
}
