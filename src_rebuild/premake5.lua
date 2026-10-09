-- premake5.lua

require "premake_modules/usage"
require "premake_modules/emscripten"
require "premake_modules/vscode"

IS_ANDROID = (_ACTION == "androidndk")

------------------------------------------

newoption {
   trigger     = "raspberry-pi",
   description = "adds specific define for compiling on Raspberry Pi"
}

table.insert(premake.option.get("os").allowed, { "emscripten", "Emscripten" })

------------------------------------------

-- you can redefine dependencies
SDL2_DIR = os.getenv("SDL2_DIR") or "dependencies/SDL2"
OPENAL_DIR = os.getenv("OPENAL_DIR") or "dependencies/openal-soft"
JPEG_DIR = os.getenv("JPEG_DIR") or "dependencies/jpeg"
-- Vulkan headers only. The loader is resolved at runtime, so no import library
-- or Vulkan SDK is required (see PsyX_Vk.cpp). Regenerate SPIR-V with
-- scripts/compile_vk_shaders.ps1 after changing a shader.
VULKAN_DIR = os.getenv("VULKAN_DIR") or "dependencies/vulkan"

-- E03 native renderer: the Diligent Engine SDK built by scripts/fetch_diligent.ps1
-- into the gitignored dependencies tree. Windows-only, Vulkan backend, Release.
DILIGENT_ROOT = os.getenv("DILIGENT_DIR") or "dependencies/DiligentEngine"
DILIGENT_CORE = DILIGENT_ROOT .. "/DiligentCore"
DILIGENT_BUILD = DILIGENT_ROOT .. "/build_core"

WEBDEMO_DIR = os.getenv("WEBDEMO_DIR") or "../../web_demo@/"	-- FIXME: make it better
RED2_DIR = os.getenv("RED2_DIR") or "../../data@/"
WEBSHELL_PATH = "../platform/Emscripten"	-- must be relative to makefile path (SADLY)

GAME_REGION = os.getenv("GAME_REGION") or "NTSC_VERSION" -- or PAL_VERSION
GAME_VERSION = os.getenv("APPVEYOR_BUILD_VERSION") or nil

-- RED2_BIN_DIR overrides the output folder so regional or migration builds do
-- not overwrite each other. Unset keeps the historical bin/<config> location.
BIN_DIR = os.getenv("RED2_BIN_DIR") or "bin/%{cfg.buildcfg}"

if not (GAME_REGION == "NTSC_VERSION" or GAME_REGION == "PAL_VERSION") then
    error("'GAME_REGION' should be 'NTSC_VERSION' or 'PAL_VERSION'")
end

------------------------------------------
	
workspace "REDRIVER2"
	if _ACTION ~= "vscode" then
		if os.target() == "emscripten" then
    		location "build_web"
		else
			-- RED2_BUILD_DIR lets a check generate isolated project files
			-- (e.g. a baseline or migration build) without overwriting the
			-- default "build" outputs. Unset keeps the historical location.
			location(os.getenv("RED2_BUILD_DIR") or "build")
		end
	else
		-- setup VSCode generator settings
		vscode_makefile "build"
		vscode_launch_cwd ("${workspaceRoot}/../data")
		--vscode_launch_environment {	}
	end

    configurations { "Debug", "Release", "Release_dev", "Release_dev_native" }
	
    defines { VERSION } 
	
	if os.target() == "emscripten" then
		platforms { "emscripten" }
	
		buildoptions {
			"-s USE_SDL=2",
			"-s USE_LIBJPEG=1",
			--"-s USE_WEBGL2=1",
			"-Wno-c++11-narrowing",
			"-Wno-constant-conversion",
			"-Wno-writable-strings",
			"-Wno-unused-value",
			"-Wno-switch",
			"-Wno-shift-op-parentheses",
			"-Wno-parentheses",
			"-Wno-format",
		}

		linkoptions  { 
			"-s TOTAL_MEMORY=1073741824",
			"-s USE_SDL=2",
			"-s USE_LIBJPEG=1",
			"-s FULL_ES2=1",
			--"-s USE_WEBGL2=1",
			"-s ASYNCIFY=1",
			"-s ALLOW_MEMORY_GROWTH=1",
			"-s GL_TESTING=1",
			("--shell-file " .. WEBSHELL_PATH .. "/shell.html"),
			("--preload-file " .. WEBDEMO_DIR),
			("--preload-file " .. RED2_DIR),
			"-s 'EXPORTED_RUNTIME_METHODS=[\"ccall\", \"writeArrayToMemory\"]'",
			"-s 'EXPORTED_FUNCTIONS=[\"_main\", \"_malloc\"]'"
		}
		
		filter { "kind:*App" }
			targetextension ".html"
			
		postbuildcommands {
			"{COPY} " .. WEBSHELL_PATH .. "/style.css %{cfg.buildtarget.directory}",
			"{COPY} " .. WEBSHELL_PATH .. "/lsfs.js %{cfg.buildtarget.directory}"
		}

	elseif IS_ANDROID then		
		system "android"
		shortcommands "On"
		
		platforms {
			"android-arm", "android-arm64"
		}
		
		disablewarnings {
			"c++11-narrowing",
			"constant-conversion",
			"writable-strings",
			"unused-value",
			"switch",
			"shift-op-parentheses",
			"parentheses",
			"format",
		}
		
		buildoptions {
			"-fpermissive",
			"-fexceptions",
			"-pthread",
		}
		
		linkoptions {
			"--no-undefined",
			"-fexceptions",
			"-pthread",
			
			"-mfloat-abi=softfp",	-- force NEON to be used
			"-mfpu=neon"
		}

		filter "platforms:*-x86"
			architecture "x86"

		filter "platforms:*-x64"
			architecture "x64"

		filter "platforms:*-arm"
			architecture "arm"

		filter "platforms:*-arm64"
			architecture "arm64"
	else
		platforms { "x86", "x64" }
	end
	
	startproject "REDRIVER2"
	
	configuration "raspberry-pi"
		defines { "__RPI__" }

	filter "system:Linux"
		buildoptions {
            "-Wno-narrowing",
			"-Wno-endif-labels",
			"-Wno-write-strings",
			"-Wno-format-security",
			"-Wno-unused-result",
            "-fpermissive"
        }
		
		cppdialect "C++11"
		
	filter {"system:Linux", "platforms:x86"}
		buildoptions { "-m32" }
		linkoptions { "-m32" }

	filter "system:Windows"
		disablewarnings { "4996", "4554", "4244", "4101", "4838", "4309" }

    filter "configurations:Debug"
        defines { 
            "_DEBUG", 
	        "DEBUG"
        }
        symbols "On"

    filter "configurations:Release"
        defines {
            "NDEBUG",
        }
		
	filter "configurations:Release_dev or Release_dev_native"
        defines {
            "NDEBUG",
        }
        symbols "On"
        
if os.target() == "windows" then
	include "premake_libjpeg.lua"
end

-- font tool
if os.target() ~= "emscripten" then
	include "premake5_font_tool.lua"
end

-- Psy-Cross layer
include "premake5_psycross.lua"



-- game iteslf
project "REDRIVER2"
    kind "WindowedApp"

    language "c++"
    targetdir(BIN_DIR)

    -- Launch the debugger from the executable folder so config.ini, mods/ and
    -- developer_debug_start.ini resolve from the expected working directory.
    debugdir(BIN_DIR)

    -- Release_dev_native builds exactly like Release_dev but the debugger
    -- launches the game with the native renderer enabled, so it can be tested
    -- from Visual Studio without setting the variable by hand.
    filter "configurations:Release_dev_native"
        debugargs { "-nointro", "-nofmv" }
        debugenvs { "RED2_NATIVE_RENDERER=1" }
    filter {}

    includedirs { 
        "Game", 
    }
	
	uses { 
		"PsyCross",
	}

    defines { GAME_REGION }
	defines { "BUILD_CONFIGURATION_STRING=\"%{cfg.buildcfg}\"" }
	
	if GAME_VERSION ~= nil then
		local resVersion = string.gsub(GAME_VERSION, "%.", ",")
		defines{ "GAME_VERSION_N=\""..GAME_VERSION.."\"" }
		defines{ "GAME_VERSION_RES="..resVersion.."" }
	end

    files {
        "Game/**.h",
        "Game/**.c"
    }

    filter {"system:Windows or linux or platforms:emscripten"}
        --dependson { "PsyX" }
        links { "jpeg" }
				
		files {
			"utils/**.h",
			"utils/**.cpp",
			"utils/**.c",
			"redriver2_psxpc.cpp",
		}
		
	filter "platforms:emscripten"
	    includedirs { 
			OPENAL_DIR.."/include",
			JPEG_DIR.."/",
        }
		files { 
            "platform/Emscripten/*.cpp",
        }

    filter "system:Windows"
		entrypoint "mainCRTStartup"
		
		files { -- TEMP
			"platform/Emscripten/*.h",
			"platform/Emscripten/*.css",
			"platform/Emscripten/*.html", 
        }
		
        files { 
            "platform/Windows/resource.h", 
            "platform/Windows/Resource.rc", 
            "platform/Windows/main.ico" 
        }

        includedirs { 
            SDL2_DIR.."/include",
            OPENAL_DIR.."/include",
			JPEG_DIR.."/",
        }
    
        linkoptions {
			"/SAFESEH:NO", -- Image Has Safe Exception Handers: No. Because of openal-soft
        }

    -- E03 native renderer (Diligent Engine, Vulkan). Windows-only and only when
    -- the SDK is present (scripts/fetch_diligent.ps1), so a default build needs
    -- no Diligent. The native module compiles as C++17 and links the static
    -- libraries; Diligent is built with the v145 toolset, which is also where
    -- the ATL library it references lives.
    local diligentPresent = os.isfile(DILIGENT_CORE .. "/CMakeLists.txt") and
        os.isfile(DILIGENT_BUILD .. "/Graphics/GraphicsEngineVulkan/Release/Diligent-GraphicsEngineVk-static.lib")
    if os.target() == "windows" and diligentPresent then
        filter "system:Windows"
            toolset "v145"
            files {
                "native/**.h",
                "native/**.cpp",
            }
            includedirs {
                "native",
                DILIGENT_CORE .. "/Graphics/GraphicsEngine/interface",
                DILIGENT_CORE .. "/Graphics/GraphicsEngineVulkan/interface",
                DILIGENT_CORE .. "/Graphics/GraphicsTools/interface",
                DILIGENT_CORE .. "/Graphics/GraphicsAccessories/interface",
                DILIGENT_CORE .. "/Common/interface",
                DILIGENT_CORE .. "/Primitives/interface",
                DILIGENT_CORE .. "/Platforms/interface",
                DILIGENT_CORE .. "/ThirdParty/Vulkan-Headers/include",
                DILIGENT_CORE .. "/ThirdParty/volk",
            }
            libdirs {
                DILIGENT_BUILD .. "/Graphics/GraphicsEngineVulkan/Release",
                DILIGENT_BUILD .. "/Graphics/GraphicsEngine/Release",
                DILIGENT_BUILD .. "/Graphics/GraphicsEngineNextGenBase/Release",
                DILIGENT_BUILD .. "/Graphics/GraphicsAccessories/Release",
                DILIGENT_BUILD .. "/Graphics/ShaderTools/Release",
                DILIGENT_BUILD .. "/Graphics/HLSL2GLSLConverterLib/Release",
                DILIGENT_BUILD .. "/Common/Release",
                DILIGENT_BUILD .. "/Primitives/Release",
                DILIGENT_BUILD .. "/Platforms/Basic/Release",
                DILIGENT_BUILD .. "/Platforms/Win32/Release",
                DILIGENT_BUILD .. "/ThirdParty/glslang/glslang/Release",
                DILIGENT_BUILD .. "/ThirdParty/glslang/SPIRV/Release",
                DILIGENT_BUILD .. "/ThirdParty/SPIRV-Tools/source/Release",
                DILIGENT_BUILD .. "/ThirdParty/SPIRV-Tools/source/opt/Release",
                DILIGENT_BUILD .. "/ThirdParty/SPIRV-Cross/Release",
                DILIGENT_BUILD .. "/ThirdParty/volk/Release",
            }
            -- Diligent's ShaderTools/Win32 objects reference ATL's static library
            -- by name; pass it by full path so MSBuild's ATL component check is
            -- not triggered while the library is still resolved.
            linkoptions {
                "\"$(VCToolsInstallDir)atlmfc/lib/x64/atls.lib\"",
            }
            links {
                "Diligent-GraphicsEngineVk-static",
                "Diligent-GraphicsEngine",
                "Diligent-GraphicsEngineNextGenBase",
                "Diligent-GraphicsAccessories",
                "Diligent-ShaderTools",
                "Diligent-HLSL2GLSLConverterLib",
                "Diligent-Common",
                "Diligent-Primitives",
                "Diligent-BasicPlatform",
                "Diligent-Win32Platform",
                "glslang",
                "SPIRV",
                "SPIRV-Tools",
                "SPIRV-Tools-opt",
                "spirv-cross-core",
                "volk",
                "comdlg32",
            }

        filter { "system:Windows", "files:native/**.cpp" }
            cppdialect "C++17"
            -- The VS generator does not emit cppdialect for file filters here, so
            -- pass the standard explicitly; Diligent's headers require C++17.
            buildoptions { "/std:c++17" }
            defines { "PLATFORM_WIN32=1" }
    end

    filter "system:linux"
        includedirs {
            "/usr/include/SDL2"
        }

        links {
            "GL",
            "openal",
            "SDL2",
            "dl",
        }

    filter "configurations:Debug"
		targetsuffix "_dbg"
        defines { 
            "DEBUG_OPTIONS",
            "COLLISION_DEBUG",
			"CUTSCENE_RECORDER"
         }
		 symbols "On"

    filter "configurations:Release"
        optimize "Speed"
		
	filter "configurations:Release_dev or Release_dev_native"
        defines { 
            "DEBUG_OPTIONS",
            "COLLISION_DEBUG",
			"CUTSCENE_RECORDER"
        }
        optimize "Speed"

    -- Distinct suffixes so the two dev configurations never share an output
    -- (sharing one would leave a stale incremental-link database).
	filter "configurations:Release_dev"
		targetsuffix "_dev"

	filter "configurations:Release_dev_native"
		targetsuffix "_native"

    filter { "files:**.c", "files:**.C" }
        compileas "C++"
