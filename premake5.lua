workspace "Twist"
    architecture "x64"
    configurations{"Debug", "Release"}
    location "build_projects"

project "Twist"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    targetdir "bin"

    fatalwarnings {"ALL"}

    includedirs {"src", "third_party"}

    files {
        "src/**",
        "third_party/**",
    }

    filter "configurations:Debug"
    symbols "On"

	-- filter "configurations:Release"
	-- 	optimize "On"