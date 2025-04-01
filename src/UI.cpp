#include "UI.h"

#include "Memory.h"
#include "Renderer.h"
#include "Unicode.h"
#include "Utils.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <hb.h>
#include <hb-ft.h>
#include <tracy/Tracy.hpp>

static struct
{
	FT_Library	mFT;
	FT_Face		mArabicFace;
	Shader		mShader;
	u32			mVAO = UINT32_MAX;
	u32			mVBO = UINT32_MAX;
} g;

UI gUiMgr;

bool ArabicCache::CreateAndRender(const std::string& inText)
{
	mWidth	= 0;
	mHeight	= 0;

	hb_font_t*		hbFont		= hb_ft_font_create(g.mArabicFace, NULL);
	hb_buffer_t*	hbBuffer	= hb_buffer_create();

	u32 numCodepoints = 0;
	u32* codepoints = Unicode::GetCodePoints((u8*)inText.c_str(), inText.size(), &numCodepoints);

	hb_buffer_add_codepoints(hbBuffer, codepoints, numCodepoints, 0, -1);
	hb_buffer_set_direction(hbBuffer, HB_DIRECTION_RTL);
	hb_buffer_set_script(hbBuffer, HB_SCRIPT_ARABIC);
	hb_buffer_set_language(hbBuffer, hb_language_from_string("ar", -1));

	hb_shape(hbFont, hbBuffer, NULL, 0);

	u32 numGlyphs = 0;
	hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &numGlyphs);

	for (u32 i = 0; i < numGlyphs; i++)
	{
		if (FT_Load_Glyph(g.mArabicFace, glyphInfo[i].codepoint, FT_LOAD_BITMAP_METRICS_ONLY))
		{
			fprintf(stderr, "ERROR: UI: Failed to calculate metrics of glyph U+%04X.\n", glyphInfo[i].codepoint);
			return false;
		}
		const FT_GlyphSlot& glyph = g.mArabicFace->glyph;

		glm::ivec2 bearing(
			g.mArabicFace->glyph->bitmap_left,
			g.mArabicFace->glyph->bitmap_top
		);

		mWidth += glyph->advance.x >> 6;
		// TODO: fix the extra height
		mHeight = std::max((i32)mHeight, (i32)glyph->bitmap.rows + bearing.y);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glCreateFramebuffers(1, &mFBO);
	glCreateTextures(GL_TEXTURE_2D, 1, &mTextureID);
	GL_LABEL(GL_TEXTURE, mTextureID, "Arabic Cache Tex");
	
	glTextureParameteri(mTextureID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(mTextureID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureParameteri(mTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTextureParameteri(mTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTextureStorage2D(mTextureID, 1, GL_R8, mWidth, mHeight);

	glNamedFramebufferTexture(mFBO, GL_COLOR_ATTACHMENT0, mTextureID, 0);
	if (glCheckNamedFramebufferStatus(mFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		puts("Error: Failed to create lighting framebuffer.\n");
		return false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	g.mShader.Use();
	g.mShader.SetVec3("uTint", glm::vec3(1.0f));

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glm::vec2 penPos(0.0f);
	for (u32 i = 0; i < numGlyphs; i++)
	{
		if (FT_Load_Glyph(g.mArabicFace, glyphInfo[i].codepoint, FT_LOAD_RENDER))
		{
			fprintf(stderr, "ERROR: UI: Failed to render glyph U+%04X.\n", glyphInfo[i].codepoint);
			return false;
		}
		const FT_GlyphSlot& glyph = g.mArabicFace->glyph;

		UI::Character c;

		u32 width = glyph->bitmap.width;
		u32 height = glyph->bitmap.rows;

		// this branch is for white space; glTextureStorage does not accept empty textures
		if (width == 0 || height == 0)
		{
			width = 1;
			height = 1;
		}

		glCreateTextures(GL_TEXTURE_2D, 1, &c.mTextureID);
		glTextureStorage2D(c.mTextureID, 1, GL_R8, width, height);

		if (glyph->bitmap.buffer)
		{
			glTextureSubImage2D(c.mTextureID, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
		} else
		{
			// a glyph with no buffer data but a valid size will never happen
			if (width != 1 && height != 1)
			{
				FATAL();
			}
			// empty buffer
			u8* b = (u8*)malloc(width * height);
			if (!b)
				FATAL();

			memset(b, 0, width * height);
			glTextureSubImage2D(c.mTextureID, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, b);
			free(b);
		}

		glTextureParameteri(c.mTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(c.mTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(c.mTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(c.mTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		c.mSize = glm::ivec2(
			width,
			height
		);

		c.mBearing = glm::ivec2(
			glyph->bitmap_left,
			glyph->bitmap_top
		);

		c.mAdvance = glyph->advance.x;

		glm::vec2 offset(
			c.mBearing.x,
			-((c.mSize.y - c.mBearing.y))
		);

		// make letters that pixels go below the base line (e.g. م) stay above it;
		offset.y += (f32)mHeight * 0.5f;

		glm::vec2 pos = penPos + offset;

		glm::vec2 size = (glm::vec2)c.mSize;

		f32 verts[6][4] = {
			// position			uv
			{ pos.x,			pos.y + size.y,	0.0f, 0.0f },
			{ pos.x,			pos.y,			0.0f, 1.0f },
			{ pos.x + size.x,	pos.y,			1.0f, 1.0f },

			{ pos.x,			pos.y + size.y,	0.0f, 0.0f },
			{ pos.x + size.x,	pos.y,			1.0f, 1.0f },
			{ pos.x + size.x,	pos.y + size.y,	1.0f, 0.0f },
		};

		glBindTextureUnit(0, c.mTextureID);
		glBindVertexArray(g.mVAO);
		glNamedBufferSubData(g.mVBO, 0, sizeof(verts), verts);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		penPos.x += (c.mAdvance >> 6);
	}

	hb_font_destroy(hbFont);
	hb_buffer_destroy(hbBuffer);

	glDisable(GL_BLEND);

	return true;
}

void ArabicCache::Destroy()
{
	glDeleteTextures(1, &mTextureID);
	glDeleteFramebuffers(1, &mFBO);
}

void UI::RenderArabic(const std::string& inText, glm::vec2 inPos, f32 inScale, const glm::vec3& inColor) const
{
	ZoneScopedN("Render Arabic");

	g.mShader.Use();
	g.mShader.SetVec3("uTint", inColor);

	glBindVertexArray(g.mVAO);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	hb_font_t* hbFont;
	hb_buffer_t* hbBuffer;
	{
		ZoneScopedN("Create Font & Buffer");
		hbFont = hb_ft_font_create(g.mArabicFace, NULL);
		hbBuffer = hb_buffer_create();
	}

	{
		ZoneScopedN("Decode UTF-8");
		hb_buffer_add_utf8(hbBuffer, inText.data(), -1, 0, -1);
	}

	{
		ZoneScopedN("Set Buffer Data");
		hb_buffer_set_direction(hbBuffer, HB_DIRECTION_RTL);
		hb_buffer_set_script(hbBuffer, HB_SCRIPT_ARABIC);
		hb_buffer_set_language(hbBuffer, hb_language_from_string("ar", -1));
	}

	{
		ZoneScopedN("Shape Text");
		hb_shape(hbFont, hbBuffer, NULL, 0);
	}

	u32 numGlyphs = 0;
	hb_glyph_info_t* glyphInfo;
	{
		ZoneScopedN("Get Glyph Data");
		glyphInfo = hb_buffer_get_glyph_infos(hbBuffer, &numGlyphs);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	{
		ZoneScopedN("Draw Glyphs");
		for (usize i = 0; i < numGlyphs; i++)
		{
			Character c;

			{
				ZoneScopedN("Render Glyph");
				if (FT_Load_Glyph(g.mArabicFace, glyphInfo[i].codepoint, FT_LOAD_RENDER))
				{
					fprintf(stderr, "ERROR: UI: Failed to render glyph for U+%04X.\n", glyphInfo[i].codepoint);
					return;
				}
			}

			const FT_GlyphSlot& glyph = g.mArabicFace->glyph;

			u32 width = glyph->bitmap.width;
			u32 height = glyph->bitmap.rows;
			// this branch is for white space; glTextureStorage does not accept empty textures
			if (width == 0 || height == 0)
			{
				width = 1;
				height = 1;
			}

			{
				ZoneScopedN("Upload Texture to GPU");
				glCreateTextures(GL_TEXTURE_2D, 1, &c.mTextureID);
				glTextureStorage2D(c.mTextureID, 1, GL_R8, width, height);

				if (glyph->bitmap.buffer)
				{
					glTextureSubImage2D(c.mTextureID, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
				} else
				{
					// a glyph with no buffer data but a valid size will never happen
					if (width != 1 && height != 1)
					{
						FATAL();
					}
					// empty buffer
					u8* b = (u8*)malloc(width * height);
					if (!b)
						FATAL();

					memset(b, 0, width * height);
					glTextureSubImage2D(c.mTextureID, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, b);
					free(b);
				}

				glTextureParameteri(c.mTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTextureParameteri(c.mTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glTextureParameteri(c.mTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTextureParameteri(c.mTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			c.mSize = glm::ivec2(
				g.mArabicFace->glyph->bitmap.width,
				g.mArabicFace->glyph->bitmap.rows
			);

			c.mBearing = glm::ivec2(
				g.mArabicFace->glyph->bitmap_left,
				g.mArabicFace->glyph->bitmap_top
			);

			c.mAdvance = g.mArabicFace->glyph->advance.x;

			glm::vec2 offset(
				c.mBearing.x * inScale,
				-((c.mSize.y - c.mBearing.y) * inScale)
			);

			glm::vec2 pos = inPos + offset;
			glm::vec2 size = (glm::vec2)c.mSize * inScale;

			{
				ZoneScopedN("Upload Quad to GPU");
				f32 verts[6][4] = {
					// position			uv
					{ pos.x,			pos.y + size.y,	0.0f, 0.0f },
					{ pos.x,			pos.y,			0.0f, 1.0f },
					{ pos.x + size.x,	pos.y,			1.0f, 1.0f },

					{ pos.x,			pos.y + size.y,	0.0f, 0.0f },
					{ pos.x + size.x,	pos.y,			1.0f, 1.0f },
					{ pos.x + size.x,	pos.y + size.y,	1.0f, 0.0f },
				};

				glBindTextureUnit(0, c.mTextureID);
				glNamedBufferSubData(g.mVBO, 0, sizeof(verts), verts);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}

			inPos.x += (c.mAdvance >> 6) * inScale;
		}
	}

	glDisable(GL_BLEND);
}

void UI::RenderArabicCached(const ArabicCache& inCache, glm::vec2 inPos, f32 inScale, const glm::vec3& inColor) const
{
	ZoneScopedN("Render Arabic (Cached)");

	g.mShader.Use();
	g.mShader.SetVec3("uTint", glm::vec3(0.0f, 1.0f, 0.0f));

	f32 w = (f32)inCache.mWidth;
	f32 h = (f32)inCache.mHeight;
	f32 s = inScale;

	f32 verts[6][4] = {
		// position				uv
		{ 0.0f,		h * s,		0.0f, 1.0f },
		{ 0.0f,		0.0f,		0.0f, 0.0f },
		{ w * s,	0.0f,		1.0f, 0.0f },

		{ 0.0f,		h * s,		0.0f, 1.0f },
		{ w * s,	h * s,		1.0f, 1.0f },
		{ w * s,	0.0f,		1.0f, 0.0f },
	};

	for (u32 i = 0; i < 6; i++)
	{
		verts[i][0] += inPos.x;
		verts[i][1] += inPos.y - f32(inCache.mHeight / 2);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTextureUnit(0, inCache.mTextureID);

	{
		ZoneScopedN("Upload Quad & Draw");
		glBindVertexArray(g.mVAO);
		glNamedBufferSubData(g.mVBO, 0, sizeof(verts), verts);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glDisable(GL_BLEND);
}

bool UI::StartUp()
{
	if (!g.mShader.Load("res/shaders/UI.vert", "res/shaders/UI.frag"))
	{
		fprintf(stderr, "ERROR: UI: Failed to load UI shader.\n");
		return false;
	}

	g.mShader.Use();
	g.mShader.SetInt("uComponent", 0);

	FT_Face englishFace = {};

	mEnglishCharMap = (Character*)Mem::Alloc(kNumAsciiChars * sizeof(Character), EMemSource::UiRAM);
	if (!mEnglishCharMap)
	{
		fprintf(stderr, "ERROR: UI: Failed to allocate memory for English font map.\n");
		return false;
	}

	if (FT_Init_FreeType(&g.mFT))
	{
		fprintf(stderr, "ERROR: UI: Failed to initialize FreeType.\n");
		return false;
	}

	if (FT_New_Face(g.mFT, "res/fonts/arial.ttf", 0, &englishFace))
	{
		fprintf(stderr, "ERROR: UI: Failed to load English font.\n");
		return false;
	}

	if (FT_New_Face(g.mFT, "res/fonts/NotoSansArabic-Regular.ttf", 0, &g.mArabicFace))
	{
		fprintf(stderr, "ERROR: UI: Failed to load Arabic font.\n");
		return false;
	}

	FT_Set_Pixel_Sizes(englishFace, 0, 48);
	FT_Set_Pixel_Sizes(g.mArabicFace, 0, 48);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for (u32 c = 0; c < kNumAsciiChars; c++)
	{
		if (FT_Load_Char(englishFace, c, FT_LOAD_RENDER))
		{
			fprintf(stderr, "ERROR: UI: Failed to load glyph for character '%c'.\n", (char)c);
			return false;
		}

		const FT_GlyphSlot& glyph = englishFace->glyph;

		u32 width = glyph->bitmap.width;
		u32 height = glyph->bitmap.rows;
		// this branch is for white space; glTextureStorage does not accept empty textures
		if (width == 0 || height == 0)
		{
			width = 1;
			height = 1;
		}

		glCreateTextures(GL_TEXTURE_2D, 1, &mEnglishCharMap[c].mTextureID);
		glTextureStorage2D(mEnglishCharMap[c].mTextureID, 1, GL_R8, width, height);
		
		if (glyph->bitmap.buffer)
		{
			glTextureSubImage2D(mEnglishCharMap[c].mTextureID, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
		} else
		{
			// a glyph with no buffer data but a valid size will never happen
			if (width != 1 && height != 1)
			{
				FATAL();
			}
			// empty buffer
			u8* b = (u8*)malloc(width * height);
			if (!b)
				FATAL();

			memset(b, 0, width * height);
			glTextureSubImage2D(mEnglishCharMap[c].mTextureID, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, b);
			free(b);
		}

		glTextureParameteri(mEnglishCharMap[c].mTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(mEnglishCharMap[c].mTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(mEnglishCharMap[c].mTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(mEnglishCharMap[c].mTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		mEnglishCharMap[c].mSize = glm::uvec2(
			glyph->bitmap.width,
			glyph->bitmap.rows
		);

		mEnglishCharMap[c].mBearing = glm::uvec2(
			glyph->bitmap_left,
			glyph->bitmap_top
		);

		mEnglishCharMap[c].mAdvance = glyph->advance.x;
	}

	FT_Done_Face(englishFace);

	glCreateVertexArrays(1, &g.mVAO);
	glCreateBuffers(1, &g.mVBO);
	glVertexArrayVertexBuffer(g.mVAO, 0, g.mVBO, 0, 4 * sizeof(f32));
	// 6 vertices (quad), 4 floats (2 vec2s)
	glNamedBufferData(g.mVBO, sizeof(f32) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexArrayAttrib(g.mVAO, 0);
	glVertexArrayAttribFormat(g.mVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(g.mVAO, 0, 0);

	return true;
}

void UI::ShutDown()
{
	for (u32 c = 0; c < kNumAsciiChars; c++)
	{
		glDeleteTextures(1, &mEnglishCharMap[c].mTextureID);
	}

	Mem::Free(mEnglishCharMap, EMemSource::UiRAM);

	g.mShader.Unload();
	glDeleteBuffers(1, &g.mVBO);
	glDeleteVertexArrays(1, &g.mVAO);
	FT_Done_Face(g.mArabicFace);
	FT_Done_FreeType(g.mFT);
}

void UI::UpdateProjection(u32 inWidth, u32 inHeight) const
{
	g.mShader.Use();
	g.mShader.SetMat4("uProjection", glm::ortho(0.0f, (f32)inWidth, 0.0f, (f32)inHeight, -1.0f, 1.0f));
}

void UI::RenderEnglish(const char* inText, glm::vec2 inPos, f32 inScale, const glm::vec3& inColor) const
{
	g.mShader.Use();
	g.mShader.SetVec3("uTint", inColor);

	glBindVertexArray(g.mVAO);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	usize len = strlen(inText);
	for (usize i = 0; i < len; i++)
	{
		char letter = inText[i];
		const Character& c = mEnglishCharMap[(u32)letter];

		glm::vec2 offset(
			c.mBearing.x * inScale,
			-((c.mSize.y - c.mBearing.y) * inScale)
		);

		glm::vec2 pos = inPos + offset;
		glm::vec2 size = (glm::vec2)c.mSize * inScale;

		f32 verts[6][4] = {
			//				position				uv
			{ pos.x,			pos.y + size.y,	0.0f, 0.0f },
			{ pos.x,			pos.y,			0.0f, 1.0f },
			{ pos.x + size.x,	pos.y,			1.0f, 1.0f },

			{ pos.x,			pos.y + size.y,	0.0f, 0.0f },
			{ pos.x + size.x,	pos.y,			1.0f, 1.0f },
			{ pos.x + size.x,	pos.y + size.y,	1.0f, 0.0f },
		};

		glBindTextureUnit(0, c.mTextureID);
		glNamedBufferSubData(g.mVBO, 0, sizeof(verts), verts);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		inPos.x += (c.mAdvance >> 6) * inScale;
	}

	glDisable(GL_BLEND);
}
