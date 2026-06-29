cmake_minimum_required(VERSION 3.12)
project(tubepng LANGUAGES CXX C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(engine)
add_subdirectory(external/PortAudio)
add_subdirectory(external)

add_executable(tubepng
	"main.cpp"
	"microphone_sampler.hpp"
)
target_link_libraries(tubepng
	firesteel
	glad
	imgui
	glm
	aubio
	PortAudio
)
target_compile_features(tubepng PUBLIC cxx_std_17)
target_include_directories(tubepng PUBLIC engine/external/glfw)
target_include_directories(tubepng PUBLIC external)

target_compile_definitions(aubio PRIVATE 
    _CRT_SECURE_NO_WARNINGS
    HAVE_CONFIG_H=0
    HAVE_SNDFILE=0
    HAVE_JACK=0
    HAVE_WAVWRITE=1
    HAVE_WAVREAD=1
)
set_target_properties(aubio PROPERTIES LANGUAGE C)

set_target_properties(tubepng PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)