#include "Unicode.h"

#include <vector>

u32* Unicode::GetCodePoints(const u8* inUnicodeStr, u32 inSize, u32* ioNumCodePoints)
{
	u32 numCodePoints = 0;

	const u8* data = inUnicodeStr;

	u32* tmp = (u32*)malloc(inSize * sizeof(u32));
	if (!tmp)
	{
		printf("ERROR: Failed to allocate %zu bytes for GetCodePoints() temporary buffer.\n", usize(inSize * sizeof(u32)));
		return nullptr;
	}

	for (u32 i = 0; i < inSize; i++)
	{
		// ascii character
		// 0x80 = 1000 0000
		if ((data[i] & 0x80) == 0)
		{
			tmp[numCodePoints++] = (u32)data[i];
		} else
		{
			// 2 byte sequence
			// 0x06 = 0000 0110
			if ((data[i] & 0x06) == 0)
			{
				/**
				 * we are optimistic here. assume that the second
				 * byte in the utf 8 sequence is valid. we dont make any
				 * check to make sure it starts with 0b10. and assume
				 * that the utf 8 string is a correct size. (i.e. 2 byte
				 * sequences dont miss their 2nd byte). or we get a buffer overflow
				 */

				u8 b0 = data[i + 0];
				u8 b1 = data[i + 1];

				// 0x1F = 0001 1111
				u8 xxxyy	= (b0 & 0x1F);
				// 0x3F = 0111 1111
				u8 yyzzzz	= (b1 & 0x3F);

				u16 codepoint = (xxxyy << 6) | yyzzzz;

				tmp[numCodePoints++] = (u32)codepoint;
			}
		}
	}

	*ioNumCodePoints = numCodePoints;

	if (numCodePoints == 0)
	{
		free(tmp);
		return nullptr;
	}

	u32* out = (u32*)malloc(numCodePoints * sizeof(u32));
	if (!out)
	{
		printf("ERROR: Failed to allocate %zu bytes for GetCodePoints() output buffer.\n", usize(numCodePoints * sizeof(u32)));
		free(tmp);
		return nullptr;
	}

	memcpy(out, tmp, numCodePoints * sizeof(u32));
	free(tmp);

	return out;
}
