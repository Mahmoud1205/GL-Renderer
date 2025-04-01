#pragma once

#include "Geom.h"
#include "defines.h"

namespace ResMgr
{
	bool			StartUp();
	void			ShutDown();

	Geom::Model*	GetModel(const char* inFilePath);
	void			ReleaseModel(const char* inFilePath);
	void			ReleaseModel(const Geom::Model* inModel);

	Geom::Texture*	GetTexture(const char* inFilePath, Geom::ETextureType inType = Geom::ETextureType::Unknown);
	void			ReleaseTexture(const char* inFilePath);
	void			ReleaseTexture(const Geom::Texture* inTexture);
}
