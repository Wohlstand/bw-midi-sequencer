cmake_minimum_required(VERSION 2.8.12...4.0)

project(FluidPlay)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(FluidSynth REQUIRED)

add_executable(fplay
    fluid_seq.cpp
    fluid_play.cpp
)
target_include_directories(fplay PRIVATE
    ${FluidSynth_INCLUDE_DIRS}
    ${FluidPlay_SOURCE_DIR}/../../include
)
target_link_libraries(fplay PRIVATE fluidsynth SDL2 ${FluidSynth_LIBRARIES})
