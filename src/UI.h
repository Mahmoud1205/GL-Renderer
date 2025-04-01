#pragma once

#include "Shader.h"
#include "defines.h"
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

constexpr u32 kNumAsciiChars = 128;

// TODO: fix حركات

struct ArabicCache
{
	bool	CreateAndRender(const std::string& inText);
	void	Destroy();

	u32		mFBO		= UINT32_MAX;
	u32		mTextureID	= UINT32_MAX;

	u32		mWidth	= 0;
	u32		mHeight	= 0;
};

class UI
{
public:
				UI() = default;
				~UI() = default;

	bool		StartUp();
	void		ShutDown();

	void		UpdateProjection(u32 inWidth, u32 inHeight) const;
	void		RenderEnglish(const char* inText, glm::vec2 inPos, f32 inScale, const glm::vec3& inColor) const;
	void		RenderArabic(const std::string& inText, glm::vec2 inPos, f32 inScale, const glm::vec3& inColor) const;
	void		RenderArabicCached(const ArabicCache& inCache, glm::vec2 inPos, f32 inScale, const glm::vec3& inColor) const;

	struct Character
	{
		u32			mTextureID = UINT32_MAX;
		glm::ivec2	mSize;
		glm::ivec2	mBearing; ///< Offset from baseline to top-left of the glyph.
		u32			mAdvance; ///< Offset to the next glyph.
	};

	Character*	mEnglishCharMap = nullptr;

private:
};

extern UI gUiMgr;
