#pragma once

#include "defines.h"
#include <string>

enum class EMemSource : u32
{
	RendererRAM,
	RendererVRAM,
	Physics,
	DebugDrawRAM,
	DebugDrawVRAM,
	ModelRAM,
	ModelVRAM,
	TextureRAM,
	TextureVRAM,
	UiRAM,
	UiVRAM,
	Unknown,
	NumSources
};

enum class EMemUnit { B, KB, MB, GB };

extern u64 gMemTotalAllocated[(u32)EMemSource::NumSources];

namespace Mem
{
	extern const char* kAllocationSourceStr[(u32)EMemSource::NumSources];

	std::string		GetUsageTable();

	/// @brief Get the total allocated memory of a source.
	/// @param ioUnit The unit of the return value.
	/// @return Memory usage in the unit ioUnit.
	f64				GetAllocatedMem(EMemSource inSource, EMemUnit* ioUnit);
	/// @brief Get the total allocated memory of all sources.
	/// @param ioUnit The unit of the return value.
	/// @return Memory usage in the unit ioUnit.
	f64				GetTotalAllocatedMem(EMemUnit* ioUnit);

	void*			Alloc(usize inSize, EMemSource inSource);
	void*			AlignedAlloc(usize inSize, usize inAlignment, EMemSource inSource);
	void*			Realloc(void* ioBlock, usize inOldSize, usize inNewSize, EMemSource inSource);

	/**
	 * @brief It is the same as Mem::Alloc and forwards constructor arguments and
	 * constructs RAII members. Overload `new` operator is not used because `EMemSourc`e
	 * parameter is needed in the function.
	 */
	template <typename T, typename... Args>
	T*				AllocT(EMemSource inSource, Args&&... inArgs)
	{
		T* mem = (T*)Mem::Alloc(sizeof(T), inSource);
		if (mem)
			new (mem) T(std::forward<Args>(inArgs)...);
		return mem;
	}

	/**
	 * @brief It is the same as Mem::Free and calls destructors. Overload `delete`
	 * operator is not used because `EMemSource` parameter is needed in the function.
	 */
	template <typename T>
	void			FreeT(T* inObj, EMemSource inSource)
	{
		inObj->~T();
		Mem::Free(inObj, inSource);
	}

	void			ReportAlloc(usize inSize, EMemSource inSource);
	void			ReportFree(usize inSize, EMemSource inSource);

	void			Free(void* inBlock, EMemSource inSource);
	void			AlignedFree(void* inBlock, EMemSource inSource);
}
