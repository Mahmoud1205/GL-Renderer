#include "Memory.h"

#include "Utils.h"
#include <unordered_map>
#include <cstdlib>

using namespace Mem;

// know how much to minus from the allocated memory sizes
// using the pointer.
std::unordered_map<uptr, usize> gPtrToSize;

// TODO: is this thread safe?
u64 gMemTotalAllocated[(u32)EMemSource::NumSources] = { 0 };

const char* kAllocationSourceStr[(u32)EMemSource::NumSources] = {
	"Renderer (RAM)",		"Renderer (VRAM)",		"Physics",
	"Debug Draw (RAM)",		"Debug Draw (VRAM)"		"Model (RAM)",
	"Model (VRAM)",			"Texture (RAM)",		"Texture (VRAM)",
	"UI (RAM)",				"UI (VRAM)",			"Unknown",
};

std::string Mem::GetUsageTable()
{
	std::string table = "";

	auto unitStr = [](EMemUnit inUnit) -> std::string {
		switch (inUnit)
		{
		case EMemUnit::B:	return "b";
		case EMemUnit::KB:	return "kb";
		case EMemUnit::MB:	return "mb";
		case EMemUnit::GB:	return "gb";
		default:			return "UNKNOWN UNIT"; FATAL(); // this will never happen
		}
	};

	EMemUnit curUnit{};

	char memStr[128] = {'\0'};

	f64 total = Mem::GetTotalAllocatedMem(&curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", total);
	table += "Total:               " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 rendererRAM = Mem::GetAllocatedMem(EMemSource::RendererRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", rendererRAM);
	table += "Renderer (RAM):      " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 rendererVRAM = Mem::GetAllocatedMem(EMemSource::RendererVRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", rendererVRAM);
	table += "Renderer (VRAM):     " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 physics = Mem::GetAllocatedMem(EMemSource::Physics, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", physics);
	table += "Physics:             " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 dbgDrawRAM = Mem::GetAllocatedMem(EMemSource::DebugDrawRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", dbgDrawRAM);
	table += "Debug Draw (RAM):    " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 dbgDrawVRAM = Mem::GetAllocatedMem(EMemSource::DebugDrawVRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", dbgDrawVRAM);
	table += "Debug Draw (VRAM):   " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 modelRAM = Mem::GetAllocatedMem(EMemSource::ModelRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", modelRAM);
	table += "Model (RAM):         " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 modelVRAM = Mem::GetAllocatedMem(EMemSource::ModelVRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", modelVRAM);
	table += "Model (VRAM):        " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 textureRAM = Mem::GetAllocatedMem(EMemSource::TextureRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", textureRAM);
	table += "Texture (RAM):       " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 textureVRAM = Mem::GetAllocatedMem(EMemSource::TextureVRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", textureVRAM);
	table += "Texture (VRAM):      " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 uiRAM = Mem::GetAllocatedMem(EMemSource::UiRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", uiRAM);
	table += "UI (RAM):            " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 uiVRAM = Mem::GetAllocatedMem(EMemSource::UiVRAM, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", uiVRAM);
	table += "UI (VRAM):           " + std::string(memStr) + unitStr(curUnit) + "\n";

	f64 unknown = Mem::GetAllocatedMem(EMemSource::Unknown, &curUnit);
	snprintf(memStr, sizeof(memStr), "%.3f", unknown);
	table += "Unknown:             " + std::string(memStr) + unitStr(curUnit) + "\n";

	return table;
}

f64 Mem::GetAllocatedMem(EMemSource inSource, EMemUnit* ioUnit)
{
	f64 totalMem = (f64)gMemTotalAllocated[(u32)inSource];
	if (totalMem < 1_kb)
	{
		*ioUnit = EMemUnit::B;
		return totalMem;
	}
	else if (totalMem < 1_mb)
	{
		*ioUnit = EMemUnit::KB;
		return totalMem / 1024.0f;
	}
	else if (totalMem < 1_gb)
	{
		*ioUnit = EMemUnit::MB;
		return totalMem / (1024.0f * 1024.0f);
	} else if (totalMem >= 1_gb)
	{
		*ioUnit = EMemUnit::GB;
		return totalMem / (1024.0f * 1024.0f * 1024.0f);
	}

	// this will never happen
	*ioUnit = EMemUnit::B;
	return FLT_MAX;
}

f64 Mem::GetTotalAllocatedMem(EMemUnit* ioUnit)
{
	f64 totalMem = 0.0f;
	for (u32 i = 0; i < (u32)EMemSource::NumSources; i++)
		totalMem += (f64)gMemTotalAllocated[(u32)(EMemSource)i];

	if (totalMem < 1_kb)
	{
		*ioUnit = EMemUnit::B;
		return totalMem;
	} else if (totalMem < 1_mb)
	{
		*ioUnit = EMemUnit::KB;
		return totalMem / 1024.0f;
	} else if (totalMem < 1_gb)
	{
		*ioUnit = EMemUnit::MB;
		return totalMem / (1024.0f * 1024.0f);
	} else if (totalMem >= 1_gb)
	{
		*ioUnit = EMemUnit::GB;
		return totalMem / (1024.0f * 1024.0f * 1024.0f);
	}

	// this will never happen
	*ioUnit = EMemUnit::B;
	return FLT_MAX;
}

void* Mem::Alloc(usize inSize, EMemSource inSource)
{
	void* mem = malloc(inSize);
	if (mem)
	{
		ReportAlloc(inSize, inSource);
		gPtrToSize[(uptr)mem] = inSize;
	}
	return mem;
}

void* Mem::AlignedAlloc(usize inSize, usize inAlignment, EMemSource inSource)
{
	void* mem = _aligned_malloc(inSize, inAlignment);
	if (mem)
	{
		ReportAlloc(inSize, inSource); // TODO: account for alignment
		gPtrToSize[(uptr)mem] = inSize;
	}
	return mem;
}

void* Mem::Realloc(void* ioBlock, usize inOldSize, usize inNewSize, EMemSource inSource)
{
	void* mem = realloc(ioBlock, inNewSize);
	// Note: maybe this is already made by realloc. but i do it just in case
	ioBlock = mem;
	if (mem)
	{
		ReportFree(inOldSize, inSource);
		ReportAlloc(inNewSize, inSource);
		gPtrToSize[(uptr)mem] = inNewSize;
	}
	return mem;
}

void Mem::ReportAlloc(usize inSize, EMemSource inSource)
{
	gMemTotalAllocated[(u32)inSource] += inSize;
}

void Mem::ReportFree(usize inSize, EMemSource inSource)
{
	if (inSize > gMemTotalAllocated[(u32)inSource])
	{
		printf("ERROR(Memory): Source %u reported freeing more memory than is available. (request free: %zubytes, total: %llu)\n", (u32)inSource, inSize, gMemTotalAllocated[(u32)inSource]);
		SBREAK();
		exit(1);
		return;
	}

	gMemTotalAllocated[(u32)inSource] -= inSize;
}

void Mem::Free(void* inBlock, EMemSource inSource)
{
	if (gPtrToSize.find((uptr)inBlock) == gPtrToSize.end())
	{
		printf("ERROR(Memory): Attempt to free a pointer that was never allocated or already freed!\n");
		SBREAK();
		exit(1);
		return;
	}
	gPtrToSize.erase((uptr)inBlock);

	free(inBlock);
	ReportFree(gPtrToSize[(uptr)inBlock], inSource);
}

void Mem::AlignedFree(void* inBlock, EMemSource inSource)
{
	if (gPtrToSize.find((uptr)inBlock) == gPtrToSize.end())
	{
		printf("ERROR(Memory): Attempt to free a pointer that was never allocated or already freed!\n");
		SBREAK();
		exit(1);
		return;
	}
	gPtrToSize.erase((uptr)inBlock);

	_aligned_free(inBlock);
	ReportFree(gPtrToSize[(uptr)inBlock], inSource); // TODO: account for alignment
}
