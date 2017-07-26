project "lumixengine_html"
	libType()
	files { 
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"external/**.c",
		"external/**.cpp",
		"external/**.h",
		"genie.lua"
	}
	removefiles {
		"external/litehtml/containers/*"
	}
	includedirs { "../lumixengine_html/src", "../lumixengine_html/external/litehtml/src/gumbo", "../lumixengine_html/external/litehtml/include" }
	buildoptions { "/wd4267", "/wd4244" }
	defines { "BUILDING_HTML", "WIN32", "LITEHTML_UTF8" }
	links { "engine" }
	useLua()
	defaultConfigurations()
