set(SRCS
	alloc.c
	alloc.h
	arith.c
	arith.h
	arr.c
	arr.h
	buf.c
	buf.h
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
	sym.c
	sym.h
	tab.c
	tab.h
)

if(MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /arch:IA32")
	add_executable(coxel WIN32 ${SRCS};platforms/win32.c)
	set_target_properties(coxel PROPERTIES
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(UNIX)
	find_package(X11 REQUIRED)
	add_executable(coxel ${SRCS};platforms/unix.c)
	target_link_libraries(coxel ${X11_LIBRARIES})
elseif(ESP_PLATFORM)
	set(SRCS
		${SRCS}
		platforms/esp32.c
	)
	idf_component_register(SRCS ${SRCS} INCLUDE_DIRS "" REQUIRES bootloader_support driver fatfs)
endif()
