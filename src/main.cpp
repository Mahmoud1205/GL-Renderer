#include "Geom.h"
#include "ResourceManager.h"
#include "Environment.h"
#include "Shader.h"
#include "Utils.h"
#include "Physics.h"
#include "PostFX.h"
#include "Renderer.h"
#include "UI.h"
#include "DebugDraw.h"
#include "Memory.h"
#include "defines.h"
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <csignal>
#include <cfenv>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <tracy/Tracy.hpp>

REQUEST_DEDICATED_GPU();

/**
 * TODO
 * - fix every TODO in the task list
 * - use compute shader for post process https://juandiegomontoya.github.io/modern_opengl.html
 * - custom model file format
 */

#define KEY_PRESSED(inKey) (glfwGetKey(gWindow, inKey) == GLFW_PRESS)

u32 gWindowWidth = 800;
u32 gWindowHeight = 600;

Geom::Model* gModel = nullptr;

Camera gCamera;

Physics* gPhysics = nullptr;
Renderer* gRenderer = nullptr;

GLFWwindow* gWindow		= nullptr;
bool gAppIsRunning		= true;
bool gCaptureMouse		= false;
bool gJustCapturedMouse	= true;
bool gEnablePhysics		= true;
bool gFreeCam			= false;

f32 gDeltaTime = 0.0f;
f32 gLastTime = 0.0f;

static void framebufferSizeCb(GLFWwindow* ioWindow, i32 inWidth, i32 inHeight);
static void mouseMoveCb(GLFWwindow* inWindow, f64 inX, f64 inY);
static void keyCb(GLFWwindow* inWindow, i32 inKey, i32 inScancode, i32 inAction, i32 inMods);
static void scrollCb(GLFWwindow* inWindow, f64 inX, f64 inY);
static void processInput();

static void update();

i32 main(void)
{
	Utils::EnableFpeExcept();

	if (Utils::IsDebuggerAttached())
		puts("[WARN]: A debugger is attached, this may interfere with the Tracy profiler results.");

	srand(time(NULL));

	// the physics class must be instanced after jolt default allocators
	// are registered
	Physics::Ready();
	gPhysics = Mem::AllocT<Physics>(EMemSource::Physics);
	gRenderer = Mem::AllocT<Renderer>(EMemSource::RendererRAM);

	tracy::SetThreadName("Main Thread");

	if (!glfwInit())
	{
		fprintf(stderr, "ERROR: Failed to initialize GLFW.\n");
		getchar();
		exit(1);
	}
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	gWindow = glfwCreateWindow(gWindowWidth, gWindowHeight, "Renderer", NULL, NULL);
	if (!gWindow)
	{
		glfwTerminate();
		fprintf(stderr, "ERROR: Failed to create GLFW window.\n");
		getchar();
		exit(1);
	}
	glfwMakeContextCurrent(gWindow);

	glfwSwapInterval(1);

	glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	gCaptureMouse = true;

	glfwSetFramebufferSizeCallback(gWindow, framebufferSizeCb);
	glfwSetCursorPosCallback(gWindow, mouseMoveCb);
	glfwSetKeyCallback(gWindow, keyCb);
	glfwSetScrollCallback(gWindow, scrollCb);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		glfwDestroyWindow(gWindow);
		glfwTerminate();
		getchar();
		fprintf(stderr, "ERROR: Failed to initialize GLAD.\n");
		exit(1);
	}

	gCamera.UpdateProjection(gWindowWidth, gWindowHeight);

	gRenderer->StartUp(gWindowWidth, gWindowHeight, gWindow);
	gRenderer->SetCamera(gCamera);
	gDebugDraw.StartUp();
	Geom::StartUp();
	gPhysics->StartUp();

	if (!gUiMgr.StartUp())
		return 1;

	gModel = ResMgr::GetModel("res/models/sponza2/sponza2.gltf");
	//gModel = ResMgr::GetModel("res/models/city/city.gltf");
	ZR_ASSERT(gModel->mPointLights.size() <= 42,
		"The lighting shader currently only supports 42 point lights,"
		"the loaded model has %zu.", gModel->mPointLights.size()
	);

	gPhysics->AddModel(*gModel);

	puts("Finished loading.");
	TracyMessage("Finished loading.", 17);

	// remember to update the UI projection before caching text
	gUiMgr.UpdateProjection(gWindowWidth, gWindowHeight);
	ArabicCache cache;
	cache.CreateAndRender(u8"السلام عليكم");

	for (u32 i = 0; i < gModel->mMeshes.size(); i++)
		gRenderer->mMeshes.push_back(&gModel->mMeshes[i]);
	for (u32 i = 0; i < gModel->mPointLights.size(); i++)
		gRenderer->mPointLights.push_back(&gModel->mPointLights[i]);

	while (gAppIsRunning)
	{
		glfwPollEvents();
		if (glfwWindowShouldClose(gWindow))
		{
			gAppIsRunning = false;
			break;
		}

		f32 currentFrame = static_cast<f32>(glfwGetTime());
		gDeltaTime = currentFrame - gLastTime;
		gDeltaTime = (gDeltaTime > 0.2f) ? 0.2f : gDeltaTime;
		gLastTime = currentFrame;

		gRenderer->BeginUI();

		processInput();
		update();

		gRenderer->SetCamera(gCamera);

		gRenderer->Render(gDeltaTime, currentFrame);

		{
			ZoneScopedN("Text Rendering");
			GL_ZONE("Draw Text");
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			//glClear(GL_COLOR_BUFFER_BIT);
			gUiMgr.UpdateProjection(gWindowWidth, gWindowHeight);
			//gUiMgr.RenderEnglish("Hello, world!", glm::vec2(100.0f, 300.0f), 1.0f, glm::vec3(1.0f));
			//gUiMgr.RenderArabic(u8"السلام عليكم", glm::vec2(400.0f, 300.0f), 1.0f, glm::vec3(1.0f));
			//gUiMgr.RenderArabicCached(cache, glm::vec2(400.0f, 300.0f), 1.0f, glm::vec3(1.0f));
			GL_ZONE_END();
		}

		{
			ZoneScopedN("Swap Buffers");
			glfwSwapBuffers(gWindow);
		}

		FrameMark;
	}

	cache.Destroy();

	gUiMgr.ShutDown();

	gPhysics->ShutDown();
	gRenderer->ShutDown();
	Mem::FreeT<Physics>(gPhysics, EMemSource::Physics);
	Mem::FreeT<Renderer>(gRenderer, EMemSource::RendererRAM);

	ResMgr::ReleaseModel(gModel);
	ResMgr::ShutDown();
	Geom::ShutDown();

	gDebugDraw.ShutDown();

	glfwDestroyWindow(gWindow);
	glfwTerminate();

	return 0;
}

void framebufferSizeCb(GLFWwindow* ioWindow, i32 inWidth, i32 inHeight)
{
	gWindowWidth = inWidth;
	gWindowHeight = inHeight;

	gCamera.UpdateProjection(inWidth, inHeight);

	gRenderer->SetCamera(gCamera);
	gRenderer->OnResize(inWidth, inHeight);
}

void mouseMoveCb(GLFWwindow* inWindow, f64 inX, f64 inY)
{
	if (!gCaptureMouse)
		return;

	f32 posX = static_cast<f32>(inX);
	f32 posY = static_cast<f32>(inY);

	static f32 lastX = posX;
	static f32 lastY = posY;

	if (gJustCapturedMouse)
	{
		lastX = posX;
		lastY = posY;
		gJustCapturedMouse = false;
	}

	f32 offsetX = posX - lastX;
	f32 offsetY = lastY - posY;
	lastX = posX;
	lastY = posY;

	f32 sensitivity = 0.2f;
	// to make sensitivity scales correctly by FOV
	sensitivity *= gCamera.mFOV / kMaxCameraFOV;

	offsetX *= sensitivity;
	offsetY *= sensitivity;

	gCamera.mYaw += offsetX;
	gCamera.mPitch += offsetY;

	if (gCamera.mPitch > 89.0f)
		gCamera.mPitch = 89.0f;
	if (gCamera.mPitch < -89.0f)
		gCamera.mPitch = -89.0f;

	glm::vec3 front{};
	front.x = cos(glm::radians(gCamera.mYaw)) * cos(glm::radians(gCamera.mPitch));
	front.y = sin(glm::radians(gCamera.mPitch));
	front.z = sin(glm::radians(gCamera.mYaw)) * cos(glm::radians(gCamera.mPitch));
	gCamera.mFront = glm::normalize(front);
}

void keyCb(GLFWwindow* inWindow, i32 inKey, i32 inScancode, i32 inAction, i32 inMods)
{
	if (inAction == GLFW_PRESS)
	{
		switch (inKey)
		{
		case GLFW_KEY_M:
			// if the camera was freecam then closed it.teleport hischaracter
			// to the camera position
			if (gFreeCam)
			{
				gPhysics->mCharacter->SetLinearVelocity(JPH::Vec3::sReplicate(0.0f));
				gPhysics->mCharacter->SetPosition(GlmToJph(gCamera.mPos));
				// update new world contacts after teleporting
				gPhysics->mCharacter->RefreshContacts(
					gPhysics->mPhysicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::kMoving),
					gPhysics->mPhysicsSystem.GetDefaultLayerFilter(Layers::kMoving),
					{}, {}, gPhysics->mTempAllocator
				);
			}
			gFreeCam = !gFreeCam;
			break;

		case GLFW_KEY_SPACE:
		{
			glm::vec3 dashDirection(0.0f);

			// increase the dash direction by the movement direction
			if (KEY_PRESSED(GLFW_KEY_W))
				dashDirection += gCamera.mFront;
			if (KEY_PRESSED(GLFW_KEY_S))
				dashDirection -= gCamera.mFront;
			if (KEY_PRESSED(GLFW_KEY_A))
				dashDirection -= glm::normalize(glm::cross(gCamera.mFront, gCamera.mUp));
			if (KEY_PRESSED(GLFW_KEY_D))
				dashDirection += glm::normalize(glm::cross(gCamera.mFront, gCamera.mUp));

			// if the camera doesnt move dash forward
			if (dashDirection == glm::vec3(0.0f))
				dashDirection += gCamera.mFront;

			dashDirection = glm::normalize(dashDirection) * 10.0f;
			gCamera.mPos += dashDirection;
			break;
		}

		case GLFW_KEY_1:
			gRenderer->mSettings.mEnableFXAA = !gRenderer->mSettings.mEnableFXAA;
			break;

		case GLFW_KEY_2:
			gRenderer->mSettings.mEnableAutoExposure = !gRenderer->mSettings.mEnableAutoExposure;
			break;

		case GLFW_KEY_3:
			gRenderer->mSettings.mEnableSSAO = !gRenderer->mSettings.mEnableSSAO;
			break;

		case GLFW_KEY_4:
			gRenderer->mSettings.mEnableCLUT = !gRenderer->mSettings.mEnableCLUT;
			break;

		default:
			break;
		}
	}
}

void scrollCb(GLFWwindow* inWindow, f64 inX, f64 inY)
{
	gCamera.mFOV -= inY;
	gCamera.mFOV = glm::clamp(gCamera.mFOV, kMinCameraFOV, kMaxCameraFOV);
	gCamera.UpdateProjection(gWindowWidth, gWindowHeight);
	gRenderer->SetCamera(gCamera);
}

void processInput()
{
	ZoneScopedN("Process Input");

	if (KEY_PRESSED(GLFW_KEY_ESCAPE))
	{
		gCaptureMouse = false;
		glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	if (KEY_PRESSED(GLFW_KEY_0))
	{
		glfwSetWindowShouldClose(gWindow, true);
		glfwDestroyWindow(gWindow);
		glfwTerminate();
		abort();
	}

	if (KEY_PRESSED(GLFW_KEY_BACKSPACE))
	{
		gAppIsRunning = false;
	}

	glm::vec3 movementDir(0.0f);
	if (gFreeCam)
	{
		f32 camSpeed = 18.5f * gDeltaTime;
		if (KEY_PRESSED(GLFW_KEY_LEFT_SHIFT))
			camSpeed *= 2.5f;
		if (KEY_PRESSED(GLFW_KEY_LEFT_CONTROL))
			camSpeed /= 2.5f;

		if (KEY_PRESSED(GLFW_KEY_W))
			movementDir += gCamera.mFront;
		if (KEY_PRESSED(GLFW_KEY_S))
			movementDir -= gCamera.mFront;
		if (KEY_PRESSED(GLFW_KEY_A))
			movementDir -= glm::normalize(glm::cross(gCamera.mFront, gCamera.mUp));
		if (KEY_PRESSED(GLFW_KEY_D))
			movementDir += glm::normalize(glm::cross(gCamera.mFront, gCamera.mUp));

		if (movementDir != glm::vec3(0.0f))
		{
			movementDir = glm::normalize(movementDir);
			movementDir *= camSpeed;
			gCamera.mPos += movementDir;
		}
	} else
	{
		if (KEY_PRESSED(GLFW_KEY_W))
			movementDir += gCamera.mFront;
		if (KEY_PRESSED(GLFW_KEY_S))
			movementDir -= gCamera.mFront;
		if (KEY_PRESSED(GLFW_KEY_A))
			movementDir -= glm::normalize(glm::cross(gCamera.mFront, gCamera.mUp));
		if (KEY_PRESSED(GLFW_KEY_D))
			movementDir += glm::normalize(glm::cross(gCamera.mFront, gCamera.mUp));

		if (movementDir != glm::vec3(0.0f))
		{
			// set y = 0 so it can avoid flying then normalize to
			// preserve direction then multiply to set speed
			movementDir.y = 0.0f;
			movementDir = glm::normalize(movementDir);
			movementDir *= 5.8f;
			if (KEY_PRESSED(GLFW_KEY_LEFT_SHIFT))
				movementDir *= 1.75f;
		}
	}

	gPhysics->UpdateCharacter(gDeltaTime, movementDir);

	if (glfwGetMouseButton(gWindow, GLFW_MOUSE_BUTTON_LEFT))
	{
		if (!ImGui::GetIO().WantCaptureMouse)
		{
			gCaptureMouse = true;
			gJustCapturedMouse = true;
			glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
	}
}

void update()
{
	ZoneScopedN("Update");

	if (gEnablePhysics)
		gPhysics->Update(gDeltaTime);

	if (!gFreeCam)
		gCamera.mPos = JphToGlm(gPhysics->mCharacter->GetPosition());

	gCamera.mView = glm::lookAt(
		gCamera.mPos,
		gCamera.mPos + gCamera.mFront,
		gCamera.mUp
	);

	ImGui::Begin("Debug Menu");
	{
		// ImGui::SliderFloat("Exposure", &gExposure, 0.0f, 10.0f);
		ImGui::Checkbox("FXAA", &gRenderer->mSettings.mEnableFXAA);
		ImGui::Checkbox("SSAO", &gRenderer->mSettings.mEnableSSAO);
		ImGui::Checkbox("Physics", &gEnablePhysics);
		ImGui::Checkbox("CLUT", &gRenderer->mSettings.mEnableCLUT);
		ImGui::Text("Delta Time: %.3fms\nFPS: %.2f", gDeltaTime, 1.0f / gDeltaTime);
	}
	ImGui::End();

	ImGui::Begin("Memory Usage");
	{
		std::string memUsageTable = Mem::GetUsageTable();
		ImGui::Text("%s", memUsageTable.c_str());
	}
	ImGui::End();
}
