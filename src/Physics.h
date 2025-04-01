#pragma once

#include "defines.h"
#include "Geom.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Core/JobSystemThreadPool.h>

namespace Layers
{
	static constexpr JPH::ObjectLayer kNonMoving	= 0;
	static constexpr JPH::ObjectLayer kMoving		= 1;
	static constexpr JPH::ObjectLayer kNumLayers	= 2;
}

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case Layers::kNonMoving:
			return inObject2 == Layers::kMoving;

		case Layers::kMoving:
			return true;

		default:
			ZR_ASSERT(false, "");
			return false;
		}
	}
};

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer	kNonMoving(0);
	static constexpr JPH::BroadPhaseLayer	kMoving(1);
	static constexpr u32					kNumLayers(2);
}

class BpLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
									BpLayerInterfaceImpl()
	{
		mObjectToBroadPhase[Layers::kNonMoving] = BroadPhaseLayers::kNonMoving;
		mObjectToBroadPhase[Layers::kMoving] = BroadPhaseLayers::kMoving;
	}

	virtual u32						GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::kNumLayers;
	}

	virtual JPH::BroadPhaseLayer	GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
	{
		ZR_ASSERT(inLayer < Layers::kNumLayers, "");
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char*				GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)Layers::kNonMoving:	return "kNonMoving";
		case (JPH::BroadPhaseLayer::Type)Layers::kMoving:		return "kMoving";
		default:												ZR_ASSERT(false, ""); return "INVALID";
		}
	}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
	JPH::BroadPhaseLayer			mObjectToBroadPhase[Layers::kNumLayers];
};

class ObjectVsBpLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::kNonMoving:
			return inLayer2 == BroadPhaseLayers::kMoving;
		case Layers::kMoving:
			return true;
		default:
			ZR_ASSERT(false, "");
			return false;
		}
	}
};

// Note: thise callbacks run in jobs. the code must be thread safe
class ContactListenerImpl : public JPH::ContactListener
{
public:
	virtual JPH::ValidateResult		OnContactValidate(
		const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset,
		const JPH::CollideShapeResult& inCollisionResult
	) override
	{
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	virtual void					OnContactAdded(
		const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
		JPH::ContactSettings& ioSettings
	) override
	{
	}

	virtual void					OnContactPersisted(
		const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold,
		JPH::ContactSettings& ioSettings
	) override
	{
	}

	virtual void					OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
	{
	}
};

class Physics final
{
public:
								Physics()
									: mTempAllocator(2_mb)
									, mJobSystem(nullptr) {};
								~Physics() = default;

	/// @brief Note: Call this before consturcting any instance of Physics.
	static void					Ready();

	bool 						StartUp();
	void 						ShutDown();

	/**
	 * @brief Remember to ctrl + a apply all transform in blender before exporting a gltf or
	 * similar formats. so scale can be imported correctly in the physics scene
	 */
	void						AddModel(const Geom::Model& inModel);
	void						ClearModels(const Geom::Model& inModel);

	void						Update(f32 inDeltaTime);

	/**
	 * @param inMovementDirection It must be normalized and the movement speed must be baked into it.
	 * (e.g. inMovementDirection = glm::normalize(movementDirection) * speed)
	 */
	void						UpdateCharacter(f32 inDeltaTime, const glm::vec3& inMovementDirection);

	std::vector<JPH::BodyID>	mStaticModels;

	glm::vec3					mCharacterPosition = glm::vec3(0.0f);

	JPH::CharacterVirtual*		mCharacter = nullptr;
	JPH::Shape*					mInnerShape = nullptr;

	JPH::PhysicsSystem			mPhysicsSystem;
	JPH::BodyInterface&			mBodyInterface = mPhysicsSystem.GetBodyInterface();
	ContactListenerImpl			mContactListener;

	JPH::TempAllocatorImpl		mTempAllocator;
	JPH::JobSystemThreadPool*	mJobSystem;

	BpLayerInterfaceImpl		mBpLayerInterface;
	ObjectVsBpLayerFilterImpl	mObjectVsBpLayerFilter;
	ObjectLayerPairFilterImpl	mObjectVsObjectLayerFilter;
};
