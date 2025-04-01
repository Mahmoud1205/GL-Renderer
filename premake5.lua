workspace "Renderer"
	configurations { "Debug", "Release" }
	location "build"
	toolset "clang"
	system "windows"
	platforms { "x86_64" }
	architecture "x86_64"

-- TODO: make the tools folder its own project

project "RendererProject"
	language "C++"
	targetdir "bin/%{cfg.buildcfg}"
	location "build/RendererProject"

	filter { "toolset:clang" }
		linker "LLD"

	exceptionhandling "Off"
	rtti "Off"
	stl "libc++"
	cppdialect "C++17"
	clangtidy "On"
	debugger "VisualStudioLocal"
	staticruntime "On"

	debugdir (os.getcwd() .. "/")

	files {
		"src/**.cpp",
		"src/**.c",
		"src/**.h",
		"include/Shared/imgui/*.cpp",
		"include/Shared/imgui/backends/imgui_impl_opengl3.cpp",
		"include/Shared/imgui/backends/imgui_impl_glfw.cpp",
		"include/Shared/imgui/misc/cpp/imgui_stdlib.cpp",
		"include/Shared/tracy/TracyClient.cpp",
		"include/%{cfg.buildcfg}/glad/glad.c",
		"res/**",
	}

	filter "files:res/**"
		buildaction "None"

	-- reset filters so they dont applied to includedirs and libdirs
	filter {}

	includedirs {
		"src",
		"include/Shared",
		"include/Shared/imgui",
		"include/Shared/Tracy",
		"include/Shared/harfbuzz",
		"include/%{cfg.buildcfg}",
	}

	libdirs {
		"lib/Shared",
		"lib/%{cfg.buildcfg}",
	}

	links {
		"glfw3",
		"Jolt",
		"assimp-vc143-mt",
		"freetype",
		"harfbuzz",
		"user32",
		"gdi32",
		"shell32",
	}

	defines {
		"_CRT_SECURE_NO_WARNINGS",
		"JPH_OBJECT_STREAM"
	}

	filter "configurations:Debug"
		kind "ConsoleApp"
		runtime "Debug"
		links { "msvcrtd" }
		-- the minimum windows version is windows 10
		defines {
			"ZR_DEBUG",
			"TRACY_ENABLE",
			"JPH_PROFILE_ENABLED",
			"JPH_DEBUG_RENDERER",
			"_DEBUG",
			"_WIN32_WINNT=_WIN32_WINNT_WIN10",
			"_LIBCPP_DEBUG=1",
			"_ITERATOR_DEBUG_LEVEL=2",
		}
		optimize "Off"
		symbols "On"
		-- TODO: -Wall, -Werror, -Wvarargs
		buildoptions {
			"-g",
			"-gcodeview",
			-- "-fsanitize=undefined",
			-- "-fsanitize-trap=all",
		}
		linkoptions {
			"-fuse-ld=lld",
			"-g",
			-- "-fsanitize=undefined",
			-- "-fsanitize-trap=all"
		}
		sanitize {
			-- "Address",
			-- "Fuzzer",
			-- "Thread",
			-- "UndefinedBehavior",
		}

	filter "configurations:Release"
		kind "ConsoleApp"
		runtime "Release"
		links { "msvcrt" }
		-- the minimum windows version is windows 10
		defines {
			"ZR_RELEASE",
			"TRACY_ENABLE",
			"JPH_PROFILE_ENABLED",
			"JPH_DEBUG_RENDERER",
			"NDEBUG",
			"_ITERATOR_DEBUG_LEVEL=0",
			"_WIN32_WINNT=_WIN32_WINNT_WIN10",
		}
		buildoptions {
			"-g",
			"-gcodeview",
		}
		linkoptions {
			"-fuse-ld=lld",
			"-g",
		}
		optimize "Full"
		symbols "On"

	-- TODO: distribution config
	--		 D:\JoltPhysics\Build\VS2022_Clang\Distribution
