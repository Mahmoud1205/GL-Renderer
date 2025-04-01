#pragma once

#include "defines.h"

struct ComputeShader
{
			ComputeShader()		= default;
			~ComputeShader()	= default;

	bool	Load(const char* inPath);
	void	Unload();

	u32		mID = UINT32_MAX;
};
