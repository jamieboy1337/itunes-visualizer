# todo: revisit and restructure :)

cmake_minimum_required(VERSION 3.0.0)
project(itunes-visualizer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(ExternalProject)

enable_testing()

set(exp_dir ${CMAKE_CURRENT_SOURCE_DIR}/experiments)
set(test_dir ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(deps_dir ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# sidenote: can include static libs without `add_library`
# just add it thats all you do

if(MSVC)
else()
  add_compile_options("-Wall")
endif()

set(BUILD_STATIC_LIBS ON)

add_subdirectory(${deps_dir}/glfw)
add_subdirectory(${deps_dir}/glm)

add_library(timing src/timing/timing.cpp)
add_library(DFT src/audiohandlers/DFT.cpp)
add_library(vorbismgr src/audiohandlers/VorbisManager.cpp)
add_library(GL src/gl/GL.cpp)
add_library(shaders src/shaders/SimpleShader.cpp src/shaders/WaveShader.cpp)
add_library(audioreaders src/audioreaders/VorbisReader.cpp)

target_include_directories(shaders PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(shaders PUBLIC glfw glad glm)
# i think this is good
target_include_directories(shaders PUBLIC ${deps_dir}/glm)
target_link_libraries(shaders PRIVATE GL DFT)

add_library(glad ${deps_dir}/glad/src/glad.c)
target_include_directories(glad PUBLIC ${deps_dir}/glad/include)
add_library(stb_vorbis ${deps_dir}/stb_vorbis/stb_vorbis.c)

add_library(stb_image ${deps_dir}/stb_image/stbi.c)
target_include_directories(stb_image PUBLIC ${deps_dir}/stb_image)

target_include_directories(GL PUBLIC ${deps_dir}/glm)
target_link_libraries(GL PUBLIC glfw glad glm)
target_link_libraries(GL PRIVATE stb_image)

target_include_directories(audioreaders PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(audioreaders PUBLIC stb_vorbis)

set(pa_stub OFF)

if(${pa_stub})
  set(pa_incdir ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
  
  # build the PA stub

  add_library(pastub ${CMAKE_CURRENT_SOURCE_DIR}/stubs/portaudio.cpp)
  target_include_directories(pastub PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
  set(pa_incdir ${deps_dir}/portaudio/include)
endif()


target_include_directories(timing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(DFT PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(vorbismgr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include 
                                     PRIVATE ${pa_incdir})

target_include_directories(stb_vorbis PUBLIC ${deps_dir}/stb_vorbis/include)

target_include_directories(GL PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# target_include_directories(audioshaders PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(vorbismgr 
                      PROPERTIES LINKER_LANGUAGE CXX)

# add_executable(itunes-visualizer src/main.cpp)

### BUILD TESTS ###
# from gtest docs

configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
# if (CMAKE_VERSION VERSION_LESS 2.8.11)
#   include_directories("${gtest_SOURCE_DIR}/include")
# endif()

# Now simply link against gtest or gtest_main as needed. Eg

FILE(GLOB TESTFILES ${test_dir}/*test.cpp)

set(timingtest_deps timing)
set(DFTtest_deps DFT)
set(SPSCtest_deps )
set(SPSCPerChanneltest_deps )
set(SimpleShadertest_deps )

if(${pa_stub})
  set(Vorbistest_deps vorbismgr stb_vorbis pastub DFT audioreaders)
else()
  set(Vorbistest_deps vorbismgr stb_vorbis DFT audioreaders)
endif()

foreach(TESTFILE ${TESTFILES})
  get_filename_component(TESTNAME ${TESTFILE} NAME_WE)
  add_executable(${TESTNAME} ${TESTFILE})
  target_include_directories(${TESTNAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include 
                                         PRIVATE ${pa_incdir})
  target_link_libraries(${TESTNAME} gtest_main ${${TESTNAME}_deps})
  add_test(NAME ${TESTNAME} COMMAND ${TESTNAME})
endforeach(TESTFILE)

if(NOT ${pa_stub})
  add_subdirectory(${deps_dir}/portaudio)
endif()

if (NOT ${pa_stub})
  target_link_libraries(vorbismgr stb_vorbis portaudio)
endif()

add_executable(cubedemo ${exp_dir}/cubedemo.cpp)
target_link_libraries(cubedemo PRIVATE glad glfw GL)
target_include_directories(cubedemo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(cubedemo_two ${exp_dir}/cubedemo2.cpp)
target_link_libraries(cubedemo_two PRIVATE glad glfw GL stb_image)
target_include_directories(cubedemo_two PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(finale_dingo src/main.cpp)
target_include_directories(finale_dingo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(finale_dingo shaders glad glfw portaudio vorbismgr audioreaders)

## COPY RESOURCES ##

file(COPY ${exp_dir}/resources DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

### FINAL CONFIGS ###

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
