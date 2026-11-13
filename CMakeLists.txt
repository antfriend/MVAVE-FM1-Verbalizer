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
  engine/verbalizer.c
  engine/resampler.c
  engine/resampler_table.c)
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
if(UNIX)
  target_link_libraries(test_engine PRIVATE m)
endif()
add_test(NAME engine COMMAND test_engine ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)

# The desktop tools: every score in the repository has to play, and a broken
# one has to be refused.
add_test(NAME score_demo COMMAND verbalize
  ${CMAKE_CURRENT_SOURCE_DIR}/host/scores/demo.txt ${CMAKE_CURRENT_BINARY_DIR}/demo.wav)
add_test(NAME score_crackle COMMAND verbalize
  ${CMAKE_CURRENT_SOURCE_DIR}/host/scores/crackle.txt ${CMAKE_CURRENT_BINARY_DIR}/crackle.wav)
add_test(NAME score_all_commands COMMAND verbalize
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/scores/all_commands.txt ${CMAKE_CURRENT_BINARY_DIR}/all_commands.wav)
add_test(NAME score_demo_44k1 COMMAND verbalize
  ${CMAKE_CURRENT_SOURCE_DIR}/host/scores/demo.txt ${CMAKE_CURRENT_BINARY_DIR}/demo_44k1.wav 44100)
add_test(NAME score_bad_command COMMAND verbalize
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/scores/bad_command.txt ${CMAKE_CURRENT_BINARY_DIR}/bad.wav)
add_test(NAME score_bad_note COMMAND verbalize
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/scores/bad_note.txt ${CMAKE_CURRENT_BINARY_DIR}/bad.wav)
set_tests_properties(score_bad_command score_bad_note PROPERTIES WILL_FAIL TRUE)
add_test(NAME tract_script COMMAND vt_script
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/vowel.vts ${CMAKE_CURRENT_BINARY_DIR}/vowel.wav)
set_tests_properties(tract_script PROPERTIES
  PASS_REGULAR_EXPRESSION "6000 d7443ca41dfe5966")
