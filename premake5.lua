workspace "Twist"
    architecture "x64"
    configurations{"Debug", "Release"}
    location "build_projects"

project "Twist"
    kind "ConsoleApp"
    language "C++"
    language "C"
    cppdialect "C++23"
    cdialect "C23"
    targetdir "bin"

    fatalwarnings {"ALL"}

    includedirs {"src", "third_party"}

    files {
        "src/**.cpp",
        "src/macaroni/rasterizer.c",
        "third_party/**",
    }

    includedirs {"%VULKAN_SDK%/Include/SDL2"}

    links {
        "%VULKAN_SDK%/Lib/SDL2main.lib", 
        "%VULKAN_SDK%/Lib/SDL2.lib"
    }

    -- filter "configurations:Debug"
        symbols "On"

	filter "configurations:Release"
		optimize "Full"
