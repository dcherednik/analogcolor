CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

add_definitions( "-Wall -O2 -g" )
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -fsanitize=address -fno-omit-frame-pointer")
project(analogcolor)

set(SOURCE_EXE
    main.c
    libanalog/ntsc.c
)

add_executable(analogcolor ${SOURCE_EXE})
target_link_libraries(analogcolor m)
