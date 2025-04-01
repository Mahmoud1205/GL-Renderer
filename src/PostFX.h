#include "defines.h"

namespace PostFX
{
	struct CLUT
	{
		u32		mSize = UINT32_MAX;
		f32*	mData = nullptr;
	};

	/// @param inPath The file must be in .cube format.
	bool LoadCLUT(CLUT* ioCLUT, const char* inPath);
	void FreeCLUT(CLUT* ioCLUT);
}
