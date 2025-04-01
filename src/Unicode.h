#pragma once

#include "defines.h"

// this API is not used any more . but i keep it in case i need it in the future

namespace Unicode
{
	/**
	 * @brief Get the UTF-32 codepoints of a UTF-8 string.
	 * Note: You need to free the return value allocated by this function.
	 * @param inSize The size of the input buffer in bytes.
	 */
	u32* GetCodePoints(const u8* inUnicodeStr, u32 inSize, u32* ioNumCodePoints);
}
