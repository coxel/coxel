set(SOURCES
	alloc.c
	alloc.h
	arith.c
	arith.h
	arr.c
	arr.h
	buf.c
	buf.h
	cfunc.c
	cfunc.h
	compiler.c
	compiler.h
	config.h
	cpu.c
	cpu.h
	font.c
	gc.c
	gc.h
	gfx.c
	gfx.h
	key.c
	key.h
	lib.c
	lib.h
	platform.c
	platform.h
	rand.c
	rand.h
	str.c
	str.h
	strstr.h
	sym.c
	sym.h
	tab.c
	tab.h
)

set(BUILTIN_CARTS
	${CMAKE_CURRENT_SOURCE_DIR}/../carts/firmware.cox
)

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
	add_definitions(-DBIT64)
endif()

if(MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:IA32")
	add_executable(coxel WIN32 ${SOURCES};platforms/win32.c)
	set_target_properties(coxel PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_custom_command(TARGET coxel POST_BUILD COMMAND
		${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../carts/firmware.cox ${CMAKE_CURRENT_BINARY_DIR}/firmware.cox)
elseif(LINUX)
	find_package(X11 REQUIRED)
	add_executable(coxel ${SOURCES};platforms/unix.c)
	target_link_libraries(coxel ${X11_LIBRARIES})
elseif(ESP_PLATFORM)
	set(SOURCES
		${SOURCES}
		platforms/esp32.c
	)
	idf_component_register(SRCS ${SOURCES} INCLUDE_DIRS "" REQUIRES bootloader_support driver fatfs EMBED_FILES ../carts/firmware.cox)
elseif(APPLE)
	set(IOS_SOURCES
		platforms/ios/AppDelegate.h
		platforms/ios/AppDelegate.m
		platforms/ios/KeyboardView.h
		platforms/ios/KeyboardView.m
		platforms/ios/main.m
		platforms/ios/MainController.h
		platforms/ios/MainController.m
		platforms/ios/MainView.h
		platforms/ios/MainView.m
		platforms/ios/SettingsController.h
		platforms/ios/SettingsController.m
	)
	set(IOS_STORYBOARDS
		platforms/ios/LaunchScreen.storyboard
	)
	add_executable(coxel MACOSX_BUNDLE ${SOURCES} ${IOS_SOURCES} ${IOS_STORYBOARDS} ${BUILTIN_CARTS})
	target_link_libraries(coxel "-framework CoreGraphics -framework QuartzCore -framework Security -framework UIKit")
	set_target_properties(coxel PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_EXECUTABLE_NAME "coxel"
		MACOSX_BUNDLE_BUNDLE_NAME "Coxel"
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.coxel.coxel"
		MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/platforms/ios/MacOSXBundleInfo.plist.in
		RESOURCE ${BUILTIN_CARTS}
		XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
		XCODE_ATTRIBUTE_SKIP_INSTALL "No"
	)
endif()
