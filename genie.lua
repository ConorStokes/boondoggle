solution "boondoggle"
	configurations { "Debug", "Release" }
	platforms      { "x32", "x64" }
	includedirs    { "include" }
	flags          { "NoPCH", "NoRTTI" }
	location       ( _ACTION )
	configuration { "gmake" }
		buildoptions   { "-std=c++11", "-msse4.1" }
	
	project "boondoggle"
		language "C++"
		kind "WindowedApp"
		files { "boondoggle/**.cpp", 
		        "boondoggle/**.c", 
				"boondoggle/**.h", 
				"common/**.cpp", 
				"common/**.c", 
				"common/**.h",
				"external/ddstextureloader/*.cpp",
				"external/ddstextureloader/*.h",
				"external/kissfft/*.c",
				"external/kissfft/*.h" }
		links { "libovr", "D3D11", "dxgi" }

		-- Based on how bgfx links the Oculus lib
		
		includedirs { "$(OVR_DIR)/LibOVR/Include" }

		configuration "x32"
			libdirs { path.join("$(OVR_DIR)/LibOVR/Lib/Windows/Win32/Release", _ACTION) }

		configuration "x64"
			libdirs { path.join("$(OVR_DIR)/LibOVR/Lib/Windows/x64/Release", _ACTION) }

		configuration "Debug*"
			flags { "Symbols" }
			
		configuration "Release*"
			flags { "OptimizeSpeed" }

		configuration { "x64", "Debug" }
			targetdir ( path.join( "bin", "64", "debug" ) )

		configuration { "x64", "Release" }
			targetdir ( path.join( "bin", "64", "release" ) )
			
		configuration { "x32", "Debug" }
			targetdir ( path.join( "bin", "32", "debug" ) )

		configuration { "x32", "Release" }
			targetdir ( path.join( "bin", "32", "release" ) )

	project "compiler"
		language "C++"
		kind "ConsoleApp"
		files { "compiler/**.cpp", 
		        "compiler/**.c", 
				"compiler/**.h", 
				"common/**.cpp", 
				"common/**.c", 
				"common/**.h",
				"external/json/*.c",
				"external/json/*.h" }
		links { "d3dcompiler", "D3D11" }

		configuration "Debug*"
			flags { "Symbols" }
			
		configuration "Release*"
			flags { "OptimizeSpeed" }

		configuration { "x64", "Debug" }
			targetdir ( path.join( "bin", "64", "debug" ) )

		configuration { "x64", "Release" }
			targetdir ( path.join( "bin", "64", "release" ) )
			
		configuration { "x32", "Debug" }
			targetdir ( path.join( "bin", "32", "debug" ) )

		configuration { "x32", "Release" }
			targetdir ( path.join( "bin", "32", "release" ) )