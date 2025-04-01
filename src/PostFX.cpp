#include "PostFX.h"

#include <memory>
#include <cstdio>

/**
 * .cube LUT format specification,
 * https://kono.phpage.fr/images/a/a1/Adobe-cube-lut-specification-1.0.pdf
 */

using namespace PostFX;

bool PostFX::LoadCLUT(CLUT* ioCLUT, const char* inPath)
{
	if (!ioCLUT)
	{
		fprintf(stderr, "Pointer to output CLUT was null.\n");
		return false;
	}

	FILE* fd = fopen(inPath, "rb");
	if (!fd)
	{
		fprintf(stderr, "Failed to open CLUT file.\n");
		return false;
	}

	// the maximum size of any line of text shouldnt exceed 250 bytes
	char buffer[251] = { 0 };
	i32 N = -1;
	u64 idx = 0;

	while (fgets(buffer, sizeof(buffer), fd))
	{
		if (strncmp(buffer, "LUT_3D_SIZE", 11) == 0)
		{
			sscanf(buffer, "LUT_3D_SIZE %d", &N);
			if (N < 0)
			{
				fprintf(stderr, "CLUT size is invalid.\n");
				fclose(fd);
				return false;
			}
			ioCLUT->mData = (f32*)malloc(3 * N * N * N * sizeof(f32));
			if (!ioCLUT->mData)
			{
				fprintf(stderr, "Failed to allocate %d bytes for CLUT.\n", 3 * N * N * N * sizeof(f32));
				fclose(fd);
				return false;
			}

			ioCLUT->mSize = (u32)N;
		} else if (N > 0 && buffer[0] != '#' && buffer[0] != '\n')
		{
			f32 r, g, b;
			if (sscanf(buffer, "%f %f %f", &r, &g, &b) == 3)
			{
				ioCLUT->mData[idx++] = r;
				ioCLUT->mData[idx++] = g;
				ioCLUT->mData[idx++] = b;
			}
		}
	}

	fclose(fd);
	return true;
}

void PostFX::FreeCLUT(CLUT* ioCLUT)
{
	if (ioCLUT->mData)
	{
		free(ioCLUT->mData);
		ioCLUT->mData = nullptr;
	}
	ioCLUT->mSize = UINT32_MAX;
}
