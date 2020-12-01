cmake_minimum_required(VERSION 3.16)
project(PT2)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenGL REQUIRED)
find_package(embree 3.0 REQUIRED)
include(FetchContent)

option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_INSTALL "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG        3.3.2
)

FetchContent_MakeAvailable(glfw)

add_executable(PT2 src/main.cpp src/pt2/vec3.cpp src/pt2/vec3.h src/pt2/camera.cpp src/pt2/camera.h src/pt2/geometry.h src/pt2/geometry.cpp src/pt2/pt2.h src/pt2/pt2.cpp)
target_include_directories(PT2 PUBLIC "extern")
target_include_directories(PT2 PUBLIC "src")
target_link_libraries(PT2 ${OPENGL_LIBRARIES} glfw embree)
