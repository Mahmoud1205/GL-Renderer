#include "Utils.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <ctime>
#include <csignal>

using namespace Utils;

static void sehHandler(u32 inCode, _EXCEPTION_POINTERS* inEP)
{
	if (inCode == EXCEPTION_FLT_DIVIDE_BY_ZERO);
	{
		printf("CAUGHT FLOAT DIVISION BY ZERO EXCEPTION!");
		SBREAK();
	}
}

bool Utils::IsDebuggerAttached()
{
	return IsDebuggerPresent();
}

void Utils::EnableFpeExcept()
{
	//_set_se_translator(sehHandler);
	//u32 ctrlWord = _controlfp(0, 0);
	//_controlfp(ctrlWord & ~_EM_ZERODIVIDE, _MCW_EM);
}

f32 Utils::InvertRange(f32 inVal, f32 inRangeStart, f32 inRangeEnd)
{
	if (inRangeStart > inRangeEnd)
	{
		f32 tmp = inRangeStart;
		inRangeStart = inRangeEnd;
		inRangeEnd = tmp;
	}
	return inRangeStart + inRangeEnd - inVal;
}

f32 Utils::RandomBetween(f32 inMin, f32 inMax)
{
	return ((inMax - inMin) * ((f32)rand() / RAND_MAX)) + inMin;
}
