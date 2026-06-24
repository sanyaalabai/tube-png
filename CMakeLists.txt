cmake_minimum_required(VERSION 3.12)
project(tubepng LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(engine)
add_subdirectory(external/PortAudio)
add_subdirectory(external/aubio)

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
set_target_properties(tubepng PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)