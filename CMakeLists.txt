cmake_minimum_required(VERSION 3.16)
project(fm1_verbalizer C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The portable engine: no hardware, no OS, no floating point.
add_library(verbalizer_engine STATIC
  engine/vocal_tract.c
  engine/vt_tables.c
  engine/voice.c
  engine/verbalizer.c)
target_include_directories(verbalizer_engine PUBLIC engine)

# Desktop helpers: WAV output and the tract script player.
add_library(hostlib STATIC host/hostlib.c)
target_include_directories(hostlib PUBLIC host)
target_link_libraries(hostlib PUBLIC verbalizer_engine)

add_executable(vt_script host/vt_script.c)
target_link_libraries(vt_script PRIVATE hostlib)

add_executable(verbalize host/verbalize.c)
target_link_libraries(verbalize PRIVATE hostlib)

enable_testing()
add_executable(test_engine tests/test_engine.c)
target_link_libraries(test_engine PRIVATE hostlib)
add_test(NAME engine COMMAND test_engine ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)
