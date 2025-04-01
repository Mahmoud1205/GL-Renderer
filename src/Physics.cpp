#include "Physics.h"

#include "Memory.h"
#include "Utils.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#include <cstdarg>
#include <thread>

using namespace JPH;

JPH_SUPPRESS_WARNINGS

static void* AllocateImpl(usize inSize)
{
	return Mem::Alloc(inSize, EMemSource::Physics);
}

static void* ReallocateImpl(void* ioBlock, usize inOldSize, usize inNewSize)
{
	return Mem::Realloc(ioBlock, inOldSize, inNewSize, EMemSource::Physics);
}

static void FreeImpl(void* inBlock)
{
	Mem::Free(inBlock, EMemSource::Physics);
}

static void* AlignedAllocateImpl(usize inSize, usize inAlignment)
{
	return Mem::AlignedAlloc(inSize, inAlignment, EMemSource::Physics);
}

static void AlignedFreeImpl(void* inBlock)
{
	Mem::AlignedFree(inBlock, EMemSource::Physics);
}

void Physics::Ready()
{
	Allocate		= AllocateImpl;
	Reallocate		= ReallocateImpl;
	Free			= FreeImpl;
	AlignedAllocate	= AlignedAllocateImpl;
	AlignedFree		= AlignedFreeImpl;
}

static void TraceImpl(const char* inFMT, ...)
{
	va_list va{};
	va_start(va, inFMT);
	char buf[1024];
	vsnprintf(buf, sizeof(buf), inFMT, va);
	va_end(va);
	printf("%s\n", buf);
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpr, const char* inMsg, const char* inFile, u32 inLine)
{
	printf("%s:%u: (%s) %s\n", inFile, inLine, inExpr, inMsg ? inMsg : "");
	return true;
}
#endif

bool Physics::StartUp()
{
	Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;);

	Factory::sInstance = new Factory();

	RegisterTypes();

	mJobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	const u32 kMaxBodies = 1024;
	const u32 kNumBodyMutexes = 0;
	const u32 kMaxBodyPairs = 1024;
	const u32 kMaxContactConstraints = 1024;

	mPhysicsSystem.Init(
		kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
		mBpLayerInterface, mObjectVsBpLayerFilter, mObjectVsObjectLayerFilter
	);

	constexpr f32 kCapsuleRadius = 1.0f;
	mInnerShape = new CapsuleShape(1.8f, kCapsuleRadius);
	CharacterVirtualSettings characterSettings;
	characterSettings.mMaxSlopeAngle = DegreesToRadians(45.0f);
	characterSettings.mShape = mInnerShape;
	characterSettings.mInnerBodyShape = mInnerShape;
	characterSettings.mBackFaceMode = EBackFaceMode::CollideWithBackFaces;
	characterSettings.mCharacterPadding = 0.02f;
	characterSettings.mPenetrationRecoverySpeed = 1.0f;
	characterSettings.mPredictiveContactDistance = 0.1f;
	characterSettings.mSupportingVolume = Plane(Vec3::sAxisY(), kCapsuleRadius);
	characterSettings.mEnhancedInternalEdgeRemoval = false;
	characterSettings.mInnerBodyLayer = Layers::kMoving;
	mCharacter = new CharacterVirtual(&characterSettings, RVec3(0.0f, 10.0f, 0.0f), Quat::sIdentity(), 0, &mPhysicsSystem);

	return true;
}

void Physics::ShutDown()
{
	delete mJobSystem;
	mJobSystem = nullptr;

	// when mCharacter is deleted the shape will delete.
	// i dont like that. but its how jolt works
	delete mCharacter;
	mCharacter = nullptr;
	mInnerShape = nullptr;

	delete Factory::sInstance;
	Factory::sInstance = nullptr;

	UnregisterTypes();
}

void Physics::AddModel(const Geom::Model& inModel)
{
	StaticCompoundShapeSettings modelShapeSettings;

	for (u32 i = 0; i < inModel.mMeshes.size(); i++)
	{
		const Geom::Mesh& mesh = inModel.mMeshes[i];

		TriangleList triangles;
		for (u32 j = 0; j < mesh.mIndices.size(); j += 3)
		{
			u32 idx0 = mesh.mIndices[j + 0];
			u32 idx1 = mesh.mIndices[j + 1];
			u32 idx2 = mesh.mIndices[j + 2];

			glm::vec3 v0 = mesh.mVertices[idx0].mPosition;
			glm::vec3 v1 = mesh.mVertices[idx1].mPosition;
			glm::vec3 v2 = mesh.mVertices[idx2].mPosition;

			triangles.emplace_back(GlmToJph(v0), GlmToJph(v1), GlmToJph(v2));
		}

		JPH::Ref<MeshShapeSettings> meshShapeSettings = new MeshShapeSettings(triangles);
		modelShapeSettings.AddShape(Vec3::sZero(), Quat::sIdentity(), meshShapeSettings, 0);
	}

	ShapeSettings::ShapeResult result = modelShapeSettings.Create();
	if (!result.IsValid())
	{
		printf("Failed to add Model to physics. JPH Error: %s\n", result.GetError().c_str());
		return;
	}

	BodyCreationSettings modelBodyCreationSettings(result.Get(), Vec3::sZero(), Quat::sIdentity(), EMotionType::Static, Layers::kNonMoving);
	BodyID id = mBodyInterface.CreateAndAddBody(modelBodyCreationSettings, EActivation::Activate);
	mStaticModels.push_back(id);

	mPhysicsSystem.OptimizeBroadPhase();
}

void Physics::ClearModels(const Geom::Model& inModel)
{
	for (u32 i = 0; i < mStaticModels.size(); i++)
	{
		BodyID id = mStaticModels[i];
		mBodyInterface.RemoveBody(id);
	}
	mPhysicsSystem.OptimizeBroadPhase();
}

void Physics::Update(f32 inDeltaTime)
{
	mPhysicsSystem.Update(inDeltaTime, 1, &mTempAllocator, mJobSystem);

	CharacterVirtual::ExtendedUpdateSettings update_settings;
	update_settings.mStickToFloorStepDown = -mCharacter->GetUp() * update_settings.mStickToFloorStepDown.Length();
	update_settings.mWalkStairsStepUp = mCharacter->GetUp() * update_settings.mWalkStairsStepUp.Length();
	mCharacter->ExtendedUpdate(
		inDeltaTime, -mCharacter->GetUp() * mPhysicsSystem.GetGravity().Length(), {},
		mPhysicsSystem.GetDefaultBroadPhaseLayerFilter(Layers::kMoving),
		mPhysicsSystem.GetDefaultLayerFilter(Layers::kMoving),
		{}, {}, mTempAllocator
	);
}

void Physics::UpdateCharacter(f32 inDeltaTime, const glm::vec3& inMovementDirection)
{
	static Vec3 desiredVelocity = Vec3::sReplicate(0.0f);

	Vec3 movementDir = GlmToJph(inMovementDirection);

	desiredVelocity = 0.25f * movementDir + 0.75f * desiredVelocity;

	Vec3 curVerticalVelocity = mCharacter->GetLinearVelocity().Dot(mCharacter->GetUp()) * mCharacter->GetUp();
	Vec3 newVelocity = curVerticalVelocity + desiredVelocity;
	newVelocity += (mCharacter->GetUp() * mPhysicsSystem.GetGravity()) * inDeltaTime;
	
	mCharacter->SetLinearVelocity(newVelocity);
}
