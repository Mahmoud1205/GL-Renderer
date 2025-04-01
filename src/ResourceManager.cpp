#include "ResourceManager.h"

#include "Memory.h"
#include "Utils.h"
#include <unordered_map>
#include <filesystem>
#include <new>

// i hate this file

using namespace ResMgr;
namespace fs = std::filesystem;

struct Resource
{
	void*	mPtr		= nullptr;
	u32		mRefCount	= 0;
};

static struct
{
	std::unordered_map<std::string, Resource>	mResourceMap;
	std::unordered_map<uptr, std::string>		mResourcePtrMap;
} gState;

bool ResMgr::StartUp()
{
	return true;
}

void ResMgr::ShutDown()
{
	for (const auto& p : gState.mResourceMap)
	{
		if (p.second.mRefCount != 0)
		{
			printf("WARN: ResMgr was shut down while object (0x%p) has a reference count above 0.\n", p.second.mPtr);
			SBREAK();
		}
	}
}

Geom::Model* ResMgr::GetModel(const char* inFilePath)
{
	std::string filePath = fs::absolute(inFilePath).string();

	if (gState.mResourceMap.find(filePath) != gState.mResourceMap.end())
	{
		Geom::Model* model = (Geom::Model*)gState.mResourceMap[filePath].mPtr;
		gState.mResourceMap[filePath].mRefCount++;
		return model;
	} else
	{
		gState.mResourceMap[filePath].mRefCount = 1;
		gState.mResourceMap[filePath].mPtr = Mem::AllocT<Geom::Model>(EMemSource::ModelRAM);
		Geom::Model* model = (Geom::Model*)gState.mResourceMap[filePath].mPtr;
		if (!model)
			return nullptr;

		gState.mResourcePtrMap[(uptr)model] = inFilePath;

		if (!model->Load(filePath.c_str()))
		{
			Mem::FreeT<Geom::Model>(model, EMemSource::ModelRAM);

			gState.mResourceMap.erase(filePath);
			gState.mResourcePtrMap.erase((uptr)model);

			return nullptr;
		}

		return model;
	}
}

void ResMgr::ReleaseModel(const char* inFilePath)
{
	std::string filePath = fs::absolute(inFilePath).string();

	if (gState.mResourceMap.find(filePath) != gState.mResourceMap.end())
	{
		if (gState.mResourceMap[filePath].mRefCount == 0)
		{
			ZR_ASSERT(false, "Attempted to release model which has a ref-count of 0.");
			return;
		}
		
		gState.mResourceMap[filePath].mRefCount--;

		if (gState.mResourceMap[filePath].mRefCount == 0)
		{
			Geom::Model* model = (Geom::Model*)gState.mResourceMap[filePath].mPtr;

			model->Unload();
			Mem::FreeT<Geom::Model>(model, EMemSource::ModelRAM);

			gState.mResourceMap.erase(filePath);
			gState.mResourcePtrMap.erase((uptr)model);
		}
	} else
	{
		ZR_ASSERT(false, "Attempted to release model which doesn't exist.");
	}
}

void ResMgr::ReleaseModel(const Geom::Model* inModel)
{
	if (gState.mResourcePtrMap.find((uptr)inModel) == gState.mResourcePtrMap.end())
	{
		ZR_ASSERT(false, "Attempted to release model by reference which doesn't exist.");
		return;
	}

	const std::string& modelPath = gState.mResourcePtrMap[(uptr)inModel];
	std::string filePath = fs::absolute(modelPath).string();

	if (gState.mResourceMap[filePath].mRefCount == 0)
	{
		ZR_ASSERT(false, "Attempted to release model which has a ref-count of 0.");
		return;
	}

	gState.mResourceMap[filePath].mRefCount--;

	if (gState.mResourceMap[filePath].mRefCount == 0)
	{
		Geom::Model* model = (Geom::Model*)gState.mResourceMap[filePath].mPtr;

		model->Unload();
		Mem::FreeT<Geom::Model>(model, EMemSource::ModelRAM);

		gState.mResourceMap.erase(filePath);
		gState.mResourcePtrMap.erase((uptr)model);
	}
}

Geom::Texture* ResMgr::GetTexture(const char* inFilePath, Geom::ETextureType inType)
{
	std::string filePath = fs::absolute(inFilePath).string();

	if (gState.mResourceMap.find(filePath) != gState.mResourceMap.end())
	{
		Geom::Texture* texture = (Geom::Texture*)gState.mResourceMap[filePath].mPtr;
		gState.mResourceMap[filePath].mRefCount++;
		return texture;
	} else
	{
		gState.mResourceMap[filePath].mRefCount = 1;
		gState.mResourceMap[filePath].mPtr = Mem::AllocT<Geom::Texture>(EMemSource::TextureRAM);
		Geom::Texture* texture = (Geom::Texture*)gState.mResourceMap[filePath].mPtr;
		if (!texture)
			return nullptr;

		gState.mResourcePtrMap[(uptr)texture] = inFilePath;

		if (!texture->Load(filePath.c_str(), inType))
		{
			Mem::FreeT<Geom::Texture>(texture, EMemSource::TextureRAM);

			gState.mResourceMap.erase(filePath);
			gState.mResourcePtrMap.erase((uptr)texture);

			return nullptr;
		}

		return texture;
	}
}

void ResMgr::ReleaseTexture(const char* inFilePath)
{
	std::string filePath = fs::absolute(inFilePath).string();

	if (gState.mResourceMap.find(filePath) != gState.mResourceMap.end())
	{
		if (gState.mResourceMap[filePath].mRefCount == 0)
		{
			ZR_ASSERT(false, "Attempted to release texture which has a ref-count of 0.");
			return;
		}

		gState.mResourceMap[filePath].mRefCount--;

		if (gState.mResourceMap[filePath].mRefCount == 0)
		{
			Geom::Texture* texture = (Geom::Texture*)gState.mResourceMap[filePath].mPtr;

			texture->Unload();
			Mem::FreeT<Geom::Texture>(texture, EMemSource::TextureRAM);

			gState.mResourceMap.erase(filePath);
			gState.mResourcePtrMap.erase((uptr)texture);
		}
	} else
	{
		ZR_ASSERT(false, "Attempted to release texture which doesn't exist.");
	}
}

void ResMgr::ReleaseTexture(const Geom::Texture* inTexture)
{
	if (gState.mResourcePtrMap.find((uptr)inTexture) == gState.mResourcePtrMap.end())
	{
		ZR_ASSERT(false, "Attempted to release texture by reference which doesn't exist.");
		return;
	}

	const std::string& texturePath = gState.mResourcePtrMap[(uptr)inTexture];
	std::string filePath = fs::absolute(texturePath).string();

	if (gState.mResourceMap[filePath].mRefCount == 0)
	{
		ZR_ASSERT(false, "Attempted to release texture which has a ref-count of 0.");
		return;
	}

	gState.mResourceMap[filePath].mRefCount--;

	if (gState.mResourceMap[filePath].mRefCount == 0)
	{
		Geom::Texture* texture = (Geom::Texture*)gState.mResourceMap[filePath].mPtr;

		texture->Unload();
		Mem::FreeT<Geom::Texture>(texture, EMemSource::TextureRAM);

		gState.mResourceMap.erase(filePath);
		gState.mResourcePtrMap.erase((uptr)texture);
	}
}
