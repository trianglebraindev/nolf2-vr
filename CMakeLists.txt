cmake_minimum_required(VERSION 3.5)
project(Lithtech)

option(BUILD_ENGINE "Build engine" ON)
option(BUILD_NOLF2 "Build No One Lives Forever 2 game code" ON)
option(BUILD_TRON "Build Tron 2.0 game code" OFF)
option(BUILD_TOOLS "Build tools" OFF)
option(ENABLE_D3D "Enable Direct3D" ON)
option(ENABLE_DIRECTX "Enable DirectX" ON)
option(ENABLE_DIRECTSHOW "Enable DirectShow" ON)
option(USE_INTERNAL_ZLIB "Use bundled zlib" ON)			# eventually want default to be OFF
option(USE_INTERNAL_FREETYPE "Use bundled freetype" ON)	# eventually want default to be OFF


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/OUT")


if(ENABLE_D3D)
	add_definitions(-D__D3D -D__D3DREND)
endif(ENABLE_D3D)

if(WIN32)
	add_definitions(-D_WINDOWS)
else(WIN32)
	add_definitions(-D_LINUX -D__LINUX)
endif(WIN32)

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
	# VS2019+ turns use of the old stdext::hash_map into a hard error unless silenced.
	add_definitions(-D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS)
	add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
	# /MP: compile each project's files in parallel across all CPU cores.
	# /Z7: keep debug symbols in Release builds so crashes show real function names.
	add_compile_options(/MP /Z7)
	add_link_options(/DEBUG /OPT:REF /OPT:ICF)
endif(MSVC)

if(WIN32)
	# DirectMusic headers removed from modern SDKs (see runtime/compat/directmusic/README.txt).
	include_directories(BEFORE ${CMAKE_SOURCE_DIR}/runtime/compat/directmusic)
endif(WIN32)

if(DEBUG)
	add_definitions(-D_DEBUG)
else(DEBUG)
	add_definitions(-D_FINAL)
endif(DEBUG)


if(USE_INTERNAL_ZLIB)
	#set zlib vars
	add_subdirectory(libs/zlib)							# LIB_ZLib
else(USE_INTERNAL_ZLIB)
	find_package(ZLIB)
endif(USE_INTERNAL_ZLIB)

# move to Lib_UI CMakeLists.txt?
if(USE_INTERNAL_FREETYPE)
	#set freetype vars
else(USE_INTERNAL_FREETYPE)
	find_package(Freetype)
endif(USE_INTERNAL_FREETYPE)


add_subdirectory(libs/lith)								# LIB_Lith
add_subdirectory(libs/stdlith)							# LIB_StdLith
add_subdirectory(libs/ltmem)							# LIB_LTMem
add_subdirectory(libs/rezmgr)							# LIB_RezMgr

if(WIN32)
	# OpenXR loader (static), from the vendored Khronos OpenXR SDK.
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
	set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
	add_subdirectory(external/OpenXR-SDK EXCLUDE_FROM_ALL)
	add_subdirectory(vr)							# LIB_XRBridge, EXE_VRProbe
endif(WIN32)

if(BUILD_ENGINE)
	add_subdirectory(runtime/ui)						# LIB_UI
	add_subdirectory(runtime/info)						# LIB_Info
	add_subdirectory(runtime/sound)						# LIB_ILTSound
	add_subdirectory(runtime/server)					# DLL_Server 
	add_subdirectory(runtime/client)					# EXE_Lithtech

	if(ENABLE_D3D)
		add_subdirectory(runtime/render_a/src/sys/d3d)	# LIB_D3DRender
	endif(ENABLE_D3D)

	if(ENABLE_DIRECTX)
		add_subdirectory(runtime/sound/src/sys/s_dx8)	# DLL_SndDrvDX8
	endif(ENABLE_DIRECTX)

	if(ENABLE_DIRECTSHOW)
		add_subdirectory(libs/dshow)					# LIB_DShow
	endif(ENABLE_DIRECTSHOW)

	if(WIN32)
		add_subdirectory(runtime/winbuild/ltmsg)		# DLL_LTMsg
	endif(WIN32)
endif(BUILD_ENGINE)

if(BUILD_NOLF2)
	add_subdirectory(libs/MFCStub)						# LIB_MFCStub
	add_subdirectory(libs/ButeMgr)						# LIB_ButeMgr
	add_subdirectory(libs/CryptMgr)						# LIB_CryptMgr
	add_subdirectory(libs/RegMgr)						# LIB_RegMgr
	add_subdirectory(libs/LTGUIMgr)						# LIB_LTGUIMgr
	add_subdirectory(libs/ServerDir)					# ServerDir
	add_subdirectory(libs/WONAPI)						# LIB_WONAPI
	add_subdirectory(NOLF2/ClientFxDLL)					# ClientFXDLL
	add_subdirectory(NOLF2/ClientRes/TO2)				# ClientRes
	add_subdirectory(NOLF2/ClientShellDLL/TO2)			# ClientShellDLL
	add_subdirectory(NOLF2/ObjectDLL/TO2)				# ObjectDLL
	add_subdirectory(NOLF2/ServerRes/TO2)				# ServerRes
endif(BUILD_NOLF2)

if(BUILD_TRON)
	if(NOT BUILD_NOLF2)
		add_subdirectory(libs/MFCStub)
		add_subdirectory(libs/butemgr)
		add_subdirectory(libs/cryptmgr)
		add_subdirectory(libs/regmgr)
		add_subdirectory(libs/LTGUIMgr)
		add_subdirectory(libs/ServerDir)
		add_subdirectory(libs/WONAPI)
		add_subdirectory(NOLF2/ClientFxDLL)		# ClientFXDLL
	endif(NOT BUILD_NOLF2)
	add_subdirectory(NOLF2/ClientRes/TRON)		# ClientRes
	add_subdirectory(NOLF2/ClientShellDLL/TRON)	# ClientShellDLL
	add_subdirectory(NOLF2/ObjectDLL/TRON)		# ObjectDLL
	add_subdirectory(NOLF2/ServerRes/TRON)		# ServerRes
endif(BUILD_TRON)

if(BUILD_TOOLS)
	add_subdirectory(tools/DtxView)			# TOOLS_DtxView
	add_subdirectory(tools/LithRez)			# TOOLS_LithRez
endif(BUILD_TOOLS)


# ---------------------------------------------------------------------------
# Run / debug setup (VR project)
# ---------------------------------------------------------------------------
if(BUILD_ENGINE AND BUILD_NOLF2)
	# After every build, copy the freshly built game modules into run/gamedll.
	# The engine loads that folder last, so these replace the retail modules.
	set(NOLF2_GAMEDLL_DIR "${CMAKE_SOURCE_DIR}/run/gamedll")
	add_custom_target(STAGE_GAMEDLL ALL
		COMMAND ${CMAKE_COMMAND} -E make_directory "${NOLF2_GAMEDLL_DIR}"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:NOLF2_ClientShellDLL>
			$<TARGET_FILE:NOLF2_ObjectDLL>
			$<TARGET_FILE:NOLF2_ClientRes>
			$<TARGET_FILE:NOLF2_ServerRes>
			$<TARGET_FILE:NOLF2_ClientFXDLL>
			"${NOLF2_GAMEDLL_DIR}"
		COMMENT "Copying game modules to run/gamedll")
	add_dependencies(STAGE_GAMEDLL NOLF2_ClientShellDLL NOLF2_ObjectDLL NOLF2_ClientRes NOLF2_ServerRes NOLF2_ClientFXDLL)

	# Make F5 in Visual Studio launch the engine exactly like run_nolf2.bat
	# (working folder = run\, see setup_run_dir.bat), so a crash stops in the
	# debugger on the faulting line.
	set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT EXE_Lithtech)
	file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/run" _rundir)
	set_target_properties(EXE_Lithtech PROPERTIES
		VS_DEBUGGER_WORKING_DIRECTORY "${_rundir}"
		VS_DEBUGGER_COMMAND_ARGUMENTS "-windowtitle \"NOLF2 (source build)\" -rez Game.rez -rez Game2.rez -rez Sound.rez -rez Custom -rez Update_v1x3.rez -rez gamedll +Windowed 1 +ScreenWidth 1280 +ScreenHeight 720 +BitDepth 32")
endif()
