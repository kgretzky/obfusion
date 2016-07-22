cmake_minimum_required (VERSION 2.6)
project (OBFUSION)

add_definitions(-w)
include_directories (
	$(OBFUSION_SOURCE_DIR)/src
	$(OBFUSION_SOURCE_DIR)/src/hde32
	$(OBFUSION_SOURCE_DIR)/src/hde64
	$(OBFUSION_SOURCE_DIR)/include
)

add_library (obfusion-static STATIC
	src/codeinstr.cpp
	src/modbuf.cpp
	src/mt.cpp
	src/obfengine.cpp
	src/obfusion.cpp
	src/hde32/hde32.c
	src/hde64/hde64.c
)

add_library (obfusion-shared SHARED
        src/codeinstr.cpp
        src/modbuf.cpp
        src/mt.cpp
        src/obfengine.cpp
        src/obfusion.cpp
        src/hde32/hde32.c
        src/hde64/hde64.c
)

install (TARGETS obfusion-static DESTINATION lib)
install (TARGETS obfusion-shared DESTINATION lib)
install (FILES include/obfusion.h DESTINATION include)

